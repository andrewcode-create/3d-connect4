import { expect, test } from '@playwright/test';

test('plays, analyzes, runs a tournament, refreshes, and opens the archive', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('heading', { name: 'Spatial Lab' })).toBeVisible();
  await expect(page.locator('canvas')).toBeVisible();
  await expect(page.getByRole('heading', { name: 'Play' })).toBeVisible();
  await page.getByRole('button', { name: 'New game' }).click();

  await page.getByRole('button', { name: 'Position Lab' }).click();
  await page.getByLabel('Analysis engine').selectOption('heuristic');
  await page.getByRole('button', { name: /Best move/ }).click();
  await expect(page.getByText('Root choice')).toBeVisible();
  await expect(page.getByRole('table')).toBeVisible();

  await page.reload();
  await expect(page.getByRole('heading', { name: 'Spatial Lab' })).toBeVisible();
  await page.getByRole('button', { name: 'Tournament' }).click();
  await page.getByLabel('Minimax AI b2 v1').uncheck();
  await page.getByLabel('Minimax AI b2 v2').uncheck();
  await page.getByLabel('Games / pair (even)').fill('2');
  await page.getByLabel('Repeats').fill('1');
  const startTournament = page.getByRole('button', { name: /Start tournament/ });
  await startTournament.click();
  await expect(page.getByText('Results')).toBeVisible({ timeout: 30_000 });
  await expect(page.getByText('Tournament heatmap')).toBeVisible();
  await expect(page.getByRole('columnheader', { name: 'A · first win' })).toBeVisible();
  await expect(page.getByRole('columnheader', { name: 'Think time / game' })).toBeVisible();
  await expect(startTournament).toBeEnabled();
  await startTournament.click();
  await expect(page.getByText('Results')).toBeVisible({ timeout: 30_000 });
  await expect(startTournament).toBeEnabled();

  await page.getByRole('button', { name: 'Archive' }).click();
  await page.getByRole('button', { name: 'Refresh archive' }).click();
  await expect(page.getByText(/Tournament ·/).first()).toBeVisible();
});
