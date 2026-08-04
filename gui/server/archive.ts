import { randomUUID } from 'node:crypto';
import { mkdir, readdir, readFile, rename, unlink, writeFile } from 'node:fs/promises';
import path from 'node:path';
import type { ArchiveRecord } from '../shared/types.ts';

export class ArchiveStore {
  constructor(private readonly directory: string) {}

  async initialize() {
    await mkdir(this.directory, { recursive: true });
  }

  private fileFor(id: string) {
    if (!/^[0-9a-f-]{36}$/.test(id)) throw new Error('Invalid archive id');
    return path.join(this.directory, `${id}.json`);
  }

  async save(input: Pick<ArchiveRecord, 'name' | 'kind' | 'payload'>, id: string = randomUUID()) {
    await this.initialize();
    const now = new Date().toISOString();
    let createdAt = now;
    try { createdAt = (await this.get(id)).createdAt; } catch { /* New archive. */ }
    const record: ArchiveRecord = { schemaVersion: 1, id, createdAt, updatedAt: now, ...input };
    const target = this.fileFor(id);
    const temporary = `${target}.tmp-${process.pid}-${Date.now()}`;
    await writeFile(temporary, `${JSON.stringify(record, null, 2)}\n`, { encoding: 'utf8', mode: 0o600 });
    await rename(temporary, target);
    return record;
  }

  async list() {
    await this.initialize();
    const files = (await readdir(this.directory)).filter((file) => file.endsWith('.json'));
    const records: ArchiveRecord[] = [];
    for (const file of files) {
      try {
        const parsed = JSON.parse(await readFile(path.join(this.directory, file), 'utf8')) as ArchiveRecord;
        if (parsed.schemaVersion === 1 && parsed.id && parsed.name) records.push(parsed);
      } catch { /* A corrupt or partial file is ignored without breaking the archive browser. */ }
    }
    return records.sort((a, b) => b.updatedAt.localeCompare(a.updatedAt));
  }

  async get(id: string) {
    const parsed = JSON.parse(await readFile(this.fileFor(id), 'utf8')) as ArchiveRecord;
    if (parsed.schemaVersion !== 1 || parsed.id !== id) throw new Error('Invalid archive record');
    return parsed;
  }

  async rename(id: string, name: string) {
    const existing = await this.get(id);
    return this.save({ name, kind: existing.kind, payload: existing.payload }, id);
  }

  async delete(id: string) {
    await unlink(this.fileFor(id));
  }
}

