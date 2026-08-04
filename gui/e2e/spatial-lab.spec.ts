import { expect, test } from '@playwright/test';

test('plays, analyzes, runs a tournament, refreshes, and opens the archive', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('heading', { name: 'Spatial Lab' })).toBeVisible();
  await expect(page.locator('canvas')).toBeVisible();
  expect(await page.locator('aside.panel').evaluate((panel) => panel.getBoundingClientRect().width)).toBeGreaterThan(550);
  await expect(page.getByRole('heading', { name: 'Play' })).toBeVisible();
  await page.getByRole('button', { name: 'New game' }).click();

  await page.getByRole('button', { name: 'Match Lab' }).click();
  await page.getByLabel('Player A · cyan').selectOption('b1-v1');
  await page.getByLabel('Player B · orange').selectOption('b1-v1');
  await page.getByLabel('Player A depth').fill('1');
  await page.getByLabel('Player B depth').fill('2');
  await expect(page.getByText('Self-play enabled: each color gets an independent AI instance.')).toBeVisible();
  const runMatch = page.getByRole('button', { name: 'Run match' });
  await expect(runMatch).toBeEnabled();
  await runMatch.click();
  await expect(page.getByText('Results')).toBeVisible({ timeout: 30_000 });
  await expect(page.getByText(/b1-v1 · d1 vs b1-v1 · d2/)).toBeVisible();

  await page.getByRole('button', { name: 'Position Lab' }).click();
  await page.getByLabel('Analysis engine').selectOption('heuristic');
  await page.getByRole('button', { name: /Best move/ }).click();
  await expect(page.getByText('Root choice')).toBeVisible();
  await expect(page.getByRole('table')).toBeVisible();

  await page.reload();
  await expect(page.getByRole('heading', { name: 'Spatial Lab' })).toBeVisible();
  await page.getByRole('button', { name: 'Tournament' }).click();
  await page.getByRole('checkbox', { name: 'Heuristic Bot' }).uncheck();
  await page.getByRole('checkbox', { name: 'Minimax AI b2 v2' }).uncheck();
  await page.getByLabel('Minimax AI b2 v1 depth').fill('1');
  await page.getByLabel('Games / pair (even)').fill('2');
  await page.getByLabel('Repeats').fill('1');
  await expect(page.getByLabel('Include self-play · fills matrix diagonal')).toBeChecked();
  const startTournament = page.getByRole('button', { name: /Start tournament/ });
  await startTournament.click();
  await expect(page.getByText('Results')).toBeVisible({ timeout: 30_000 });
  await expect(page.getByText('Tournament heatmap')).toBeVisible();
  await expect(page.getByRole('columnheader', { name: 'A · first win' })).toBeVisible();
  await expect(page.getByRole('columnheader', { name: 'Think time / game' })).toBeVisible();
  await expect(page.getByText('Color matchup matrix')).toBeVisible();
  await expect(page.getByText('Each cell describes the orange row AI against the cyan column AI.')).toBeVisible();
  await expect(page.getByText('Columns · cyan · moves first')).toBeVisible();
  await expect(page.getByText('Rows · orange · moves second')).toBeVisible();
  await expect(page.locator('.matrix-cell')).toHaveCount(4);
  await expect(startTournament).toBeEnabled();
  await startTournament.click();
  await expect(page.getByText('Results')).toBeVisible({ timeout: 30_000 });
  await expect(startTournament).toBeEnabled();

  await page.getByRole('button', { name: 'Archive' }).click();
  await page.getByRole('button', { name: 'Refresh archive' }).click();
  await expect(page.getByText(/Tournament ·/).first()).toBeVisible();
});
