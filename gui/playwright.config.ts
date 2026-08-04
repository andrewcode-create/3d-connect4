import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: './e2e',
  timeout: 45_000,
  use: { baseURL: 'http://127.0.0.1:5199', headless: true },
  webServer: {
    command: 'CONNECT4_DATA=/tmp/connect4-playwright-data NODE_ENV=production PORT=5199 npx tsx server/index.ts',
    url: 'http://127.0.0.1:5199/api/health',
    reuseExistingServer: false
  }
});

