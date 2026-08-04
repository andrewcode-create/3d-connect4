import path from 'node:path';
import { fileURLToPath } from 'node:url';
import express from 'express';
import { createApi } from './app.ts';

const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(directory, '..');
const binary = process.env.CONNECT4_ENGINE ?? path.join(root, 'bin', 'connect4-gui-engine');
const data = process.env.CONNECT4_DATA ?? path.join(root, 'data');
const { app } = await createApi(binary, data);

if (process.env.NODE_ENV === 'production') {
  const dist = path.join(root, 'dist');
  app.use(express.static(dist));
  app.use((_request, response) => response.sendFile(path.join(dist, 'index.html')));
} else {
  const { createServer: createViteServer } = await import('vite');
  const vite = await createViteServer({ root, server: { middlewareMode: true }, appType: 'spa' });
  app.use(vite.middlewares);
}

const port = Number(process.env.PORT ?? 5173);
app.listen(port, '127.0.0.1', () => {
  console.log(`Connect Four Spatial Lab: http://127.0.0.1:${port}`);
});

