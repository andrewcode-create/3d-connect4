import { EventEmitter } from 'node:events';
import { execFile, spawn, type ChildProcess } from 'node:child_process';
import { promisify } from 'node:util';
import { randomUUID } from 'node:crypto';
import readline from 'node:readline';
import type { AiInfo, JobConfig, JobSnapshot, NativeEvent } from '../shared/types.ts';
import { ArchiveStore } from './archive.ts';

const execFileAsync = promisify(execFile);

function movesArg(moves: number[] | undefined) { return (moves ?? []).join(','); }

function positiveInteger(value: unknown, fallback: number, label: string) {
  const result = value === undefined ? fallback : Number(value);
  if (!Number.isSafeInteger(result) || result < 1) throw new Error(`${label} must be a positive integer`);
  return result;
}

function nativeArguments(config: JobConfig) {
  if (config.kind === 'ai-move' || config.kind === 'analysis') {
    if (!config.aiId) throw new Error('aiId is required');
    return ['analyze', '--ai', config.aiId, '--moves', movesArg(config.moves), '--seed', String(config.seed ?? 1),
      '--candidates', String(config.kind === 'analysis' && config.candidates !== false)];
  }
  const args = ['run', '--kind', config.kind, '--games', String(config.kind === 'match' ? 1 : positiveInteger(config.games, 10, 'games')),
    '--repeats', String(positiveInteger(config.repeats, 1, 'repeats')), '--samples', String(Math.max(0, Number(config.samples ?? 5))),
    '--seed', String(positiveInteger(config.seed, 1, 'seed')), '--threads', String(positiveInteger(config.threads, 2, 'threads'))];
  if (config.kind === 'tournament') {
    if (!config.aiIds || config.aiIds.length < (config.includeSelfPlay ? 1 : 2)) throw new Error(config.includeSelfPlay ? 'Self-play tournament needs at least one AI' : 'Tournament needs at least two AIs');
    args.push('--ais', config.aiIds.join(','));
    if (config.includeSelfPlay) args.push('--self-play', '1');
  } else {
    if (!config.playerA || !config.playerB) throw new Error('playerA and playerB are required');
    args.push('--a', config.playerA, '--b', config.playerB);
  }
  return args;
}

export async function runNativeOnce(binary: string, args: string[]) {
  const { stdout } = await execFileAsync(binary, args, { maxBuffer: 64 * 1024 * 1024, timeout: 120_000 });
  const lines = stdout.trim().split(/\r?\n/).filter(Boolean);
  if (!lines.length) throw new Error('Native engine returned no data');
  const event = JSON.parse(lines.at(-1)!) as NativeEvent;
  if (event.type === 'error') throw new Error(event.message);
  if (event.type !== 'complete') throw new Error('Native engine did not complete');
  return event.result;
}

interface ManagedJob extends JobSnapshot {
  emitter: EventEmitter;
  child?: ChildProcess;
  archived?: boolean;
}

export class JobManager {
  private readonly jobs = new Map<string, ManagedJob>();
  private aiIds = new Set<string>();

  constructor(private readonly binary: string, private readonly archives: ArchiveStore) {}

  async initialize() {
    const ais = await runNativeOnce(this.binary, ['ais']) as AiInfo[];
    this.aiIds = new Set(ais.map((ai) => ai.id));
    return ais;
  }

  private validate(config: JobConfig) {
    if (!['ai-move', 'analysis', 'match', 'batch', 'tournament'].includes(config.kind)) throw new Error('Unknown job kind');
    const ids = [config.aiId, config.playerA, config.playerB, ...(config.aiIds ?? [])].filter(Boolean) as string[];
    for (const id of ids) if (!this.aiIds.has(id)) throw new Error(`Unknown AI id: ${id}`);
    if (config.moves && (!Array.isArray(config.moves) || config.moves.some((move) => !Number.isInteger(move) || move < 0 || move > 15)))
      throw new Error('moves must contain integers from 0 to 15');
    if ((config.kind === 'batch' || config.kind === 'tournament') && positiveInteger(config.games, 10, 'games') % 2)
      throw new Error('Balanced runs require an even game count');
    if (config.kind === 'tournament' && new Set(config.aiIds).size !== config.aiIds?.length)
      throw new Error('Tournament entrants must be unique');
    return nativeArguments(config);
  }

  create(config: JobConfig) {
    const args = this.validate(config);
    const id = randomUUID();
    const now = new Date().toISOString();
    const job: ManagedJob = { schemaVersion: 1, id, status: 'queued', config, createdAt: now, updatedAt: now,
      progress: 0, events: [], emitter: new EventEmitter() };
    this.jobs.set(id, job);
    this.launch(job, args);
    return this.publicSnapshot(job);
  }

  private publicSnapshot(job: ManagedJob): JobSnapshot {
    const { emitter: _emitter, child: _child, archived: _archived, ...snapshot } = job;
    return structuredClone(snapshot);
  }

  get(id: string) {
    const job = this.jobs.get(id);
    if (!job) throw new Error('Job not found');
    return this.publicSnapshot(job);
  }

  subscribe(id: string, listener: (event: NativeEvent) => void) {
    const job = this.jobs.get(id);
    if (!job) throw new Error('Job not found');
    job.emitter.on('event', listener);
    return () => job.emitter.off('event', listener);
  }

  private record(job: ManagedJob, event: NativeEvent) {
    job.updatedAt = new Date().toISOString();
    if (event.type === 'progress') {
      job.progress = event.total ? event.completed / event.total : 0;
      if (job.events.at(-1)?.type === 'progress') job.events[job.events.length - 1] = event;
      else job.events.push(event);
    } else {
      job.events.push(event);
    }
    if (event.type === 'complete') { job.result = event.result; job.progress = 1; }
    if (event.type === 'error') job.error = event.message;
    job.emitter.emit('event', event);
  }

  private launch(job: ManagedJob, args: string[]) {
    job.status = 'running';
    const child = spawn(this.binary, args, { stdio: ['ignore', 'pipe', 'pipe'], shell: false });
    job.child = child;
    if (!child.stdout || !child.stderr) throw new Error('Native process pipes were not created');
    const lines = readline.createInterface({ input: child.stdout });
    lines.on('line', (line) => {
      try { this.record(job, JSON.parse(line) as NativeEvent); }
      catch { this.record(job, { type: 'error', message: 'Native engine emitted malformed JSON' }); }
    });
    let stderr = '';
    child.stderr.on('data', (data: Buffer) => { stderr += data.toString(); if (stderr.length > 16_384) stderr = stderr.slice(-16_384); });
    child.on('error', (error) => this.fail(job, error.message));
    child.on('close', (code, signal) => {
      job.child = undefined;
      if (job.status === 'cancelled') { void this.archive(job); return; }
      if (code === 0 && job.result !== undefined) job.status = 'complete';
      else this.fail(job, job.error ?? stderr.trim() ?? `Native engine exited with ${signal ?? code}`);
      job.updatedAt = new Date().toISOString();
      void this.archive(job);
    });
  }

  private fail(job: ManagedJob, message: string) {
    if (job.status === 'failed') return;
    job.status = 'failed'; job.error = message; job.updatedAt = new Date().toISOString();
    this.record(job, { type: 'error', message });
  }

  cancel(id: string) {
    const job = this.jobs.get(id);
    if (!job) throw new Error('Job not found');
    if (!job.child || !['queued', 'running'].includes(job.status)) return this.publicSnapshot(job);
    job.status = 'cancelled'; job.updatedAt = new Date().toISOString();
    job.child.kill('SIGTERM');
    const child = job.child;
    setTimeout(() => { if (child.exitCode === null) child.kill('SIGKILL'); }, 2_000).unref();
    return this.publicSnapshot(job);
  }

  private async archive(job: ManagedJob) {
    if (job.archived || !['complete', 'cancelled'].includes(job.status)) return;
    job.archived = true;
    const label = job.config.kind === 'tournament' ? 'Tournament' : job.config.kind === 'batch' ? 'Batch' : 'Match';
    await this.archives.save({ name: `${label} · ${new Date(job.createdAt).toLocaleString()}`, kind: 'run', payload: this.publicSnapshot(job) });
  }
}

