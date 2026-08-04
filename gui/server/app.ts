import express, { type Express, type NextFunction, type Request, type Response } from 'express';
import type { ArchiveRecord, JobConfig } from '../shared/types.ts';
import { ArchiveStore } from './archive.ts';
import { JobManager, runNativeOnce } from './native.ts';

function asyncRoute(handler: (request: Request, response: Response) => Promise<void>) {
  return (request: Request, response: Response, next: NextFunction) => { void handler(request, response).catch(next); };
}

function routeId(value: string | string[]) { return Array.isArray(value) ? (value[0] ?? '') : value; }

export async function createApi(binary: string, dataDirectory: string) {
  const app = express();
  const archives = new ArchiveStore(dataDirectory);
  await archives.initialize();
  const jobs = new JobManager(binary, archives);
  const ais = await jobs.initialize();
  app.use(express.json({ limit: '2mb' }));

  app.get('/api/health', (_request, response) => response.json({ ok: true, schemaVersion: 1 }));
  app.get('/api/ais', (_request, response) => response.json(ais));
  app.post('/api/positions/inspect', asyncRoute(async (request, response) => {
    const moves = request.body?.moves;
    if (!Array.isArray(moves) || moves.some((move) => !Number.isInteger(move) || move < 0 || move > 15))
      throw new Error('moves must contain integers from 0 to 15');
    response.json(await runNativeOnce(binary, ['inspect', '--moves', moves.join(',')]));
  }));
  app.post('/api/jobs', (request, response, next) => {
    try { response.status(202).json(jobs.create(request.body as JobConfig)); } catch (error) { next(error); }
  });
  app.get('/api/jobs/:id', (request, response, next) => {
    try { response.json(jobs.get(routeId(request.params.id))); } catch (error) { next(error); }
  });
  app.delete('/api/jobs/:id', (request, response, next) => {
    try { response.json(jobs.cancel(routeId(request.params.id))); } catch (error) { next(error); }
  });
  app.get('/api/jobs/:id/events', (request, response, next) => {
    try {
      const snapshot = jobs.get(routeId(request.params.id));
      response.setHeader('Content-Type', 'text/event-stream');
      response.setHeader('Cache-Control', 'no-cache');
      response.setHeader('Connection', 'keep-alive');
      response.flushHeaders();
      response.write(`event: snapshot\ndata: ${JSON.stringify(snapshot)}\n\n`);
      const unsubscribe = jobs.subscribe(routeId(request.params.id), (event) => response.write(`event: native\ndata: ${JSON.stringify(event)}\n\n`));
      const heartbeat = setInterval(() => response.write(': heartbeat\n\n'), 15_000);
      request.on('close', () => { clearInterval(heartbeat); unsubscribe(); });
    } catch (error) { next(error); }
  });

  app.get('/api/archive', asyncRoute(async (_request, response) => { response.json(await archives.list()); }));
  app.get('/api/archive/:id', asyncRoute(async (request, response) => { response.json(await archives.get(routeId(request.params.id))); }));
  app.post('/api/archive', asyncRoute(async (request, response) => {
    const { name, kind, payload } = request.body as Partial<ArchiveRecord>;
    if (!name?.trim() || !['position', 'play'].includes(kind ?? '')) throw new Error('A name and position/play kind are required');
    response.status(201).json(await archives.save({ name: name.trim().slice(0, 120), kind: kind as 'position' | 'play', payload }));
  }));
  app.patch('/api/archive/:id', asyncRoute(async (request, response) => {
    if (!request.body?.name?.trim()) throw new Error('name is required');
    response.json(await archives.rename(routeId(request.params.id), request.body.name.trim().slice(0, 120)));
  }));
  app.delete('/api/archive/:id', asyncRoute(async (request, response) => {
    await archives.delete(routeId(request.params.id)); response.status(204).end();
  }));

  installErrorHandler(app);
  return { app, jobs, archives };
}

export function installErrorHandler(app: Express) {
  app.use((error: Error, _request: Request, response: Response, _next: NextFunction) => {
    const notFound = /not found/i.test(error.message);
    response.status(notFound ? 404 : 400).json({ error: error.message });
  });
}

