import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import request from 'supertest';
import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
import { createApi } from '../server/app.ts';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const binary = path.join(root, 'bin', 'connect4-gui-engine');
let dataDirectory: string;

beforeEach(async () => { dataDirectory = await mkdtemp(path.join(os.tmpdir(), 'connect4-gui-test-')); });
afterEach(async () => { await rm(dataDirectory, { recursive: true, force: true }); });

describe('local API', () => {
  it('lists every native AI and inspects a winning position', async () => {
    const { app } = await createApi(binary, dataDirectory);
    const ais = await request(app).get('/api/ais').expect(200);
    expect(ais.body).toHaveLength(11);
    const position = await request(app).post('/api/positions/inspect').send({ moves: [0, 1, 0, 1, 0, 1, 0] }).expect(200);
    expect(position.body.winner).toBe('A');
    expect(position.body.winningCells).toEqual([0, 16, 32, 48]);
  });

  it('validates balanced jobs and completes a native batch', async () => {
    const { app } = await createApi(binary, dataDirectory);
    await request(app).post('/api/jobs').send({ kind: 'batch', playerA: 'random', playerB: 'heuristic', games: 3 }).expect(400);
    const created = await request(app).post('/api/jobs').send({ kind: 'batch', playerA: 'random', playerB: 'heuristic', games: 2, threads: 1 }).expect(202);
    let snapshot = created.body;
    for (let attempt = 0; attempt < 100 && !['complete', 'failed'].includes(snapshot.status); ++attempt) {
      await new Promise((resolve) => setTimeout(resolve, 20));
      snapshot = (await request(app).get(`/api/jobs/${created.body.id}`).expect(200)).body;
    }
    expect(snapshot.status).toBe('complete');
    expect(snapshot.result.completedGames).toBe(2);
    for (const standing of snapshot.result.standings) {
      expect(standing.byColor.A.played).toBe(1);
      expect(standing.byColor.B.played).toBe(1);
      expect(standing.byColor.A.played + standing.byColor.B.played).toBe(standing.played);
    }
  });

  it('archives safely and ignores corrupted records', async () => {
    const { app } = await createApi(binary, dataDirectory);
    const saved = await request(app).post('/api/archive').send({ name: '../analysis', kind: 'position', payload: { moves: [1, 2] } }).expect(201);
    expect(saved.body.id).toMatch(/^[0-9a-f-]{36}$/);
    await writeFile(path.join(dataDirectory, 'corrupt.json'), '{broken');
    const listed = await request(app).get('/api/archive').expect(200);
    expect(listed.body).toHaveLength(1);
    await request(app).delete(`/api/archive/${saved.body.id}`).expect(204);
  });
});
