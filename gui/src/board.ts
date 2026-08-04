import type { GameRecord, Player } from '../shared/types';

export function cellCoordinates(cell: number) {
  return { column: cell % 4, row: Math.floor(cell / 4) % 4, layer: Math.floor(cell / 16) };
}

export function moveFromCoordinates(row: number, column: number) {
  if (!Number.isInteger(row) || !Number.isInteger(column) || row < 0 || row > 3 || column < 0 || column > 3)
    throw new Error('Board coordinates must be integers from 0 to 3');
  return row * 4 + column;
}

export function cellsFromGame(game: GameRecord | null, ply: number): Array<Player | null> {
  const cells: Array<Player | null> = Array(64).fill(null);
  if (!game) return cells;
  for (const move of game.moves.slice(0, ply)) cells[move.cell] = move.player;
  return cells;
}

export function visualWinningLine(cells: Array<Player | null>) {
  const lines: number[][] = [];
  const add = (...items: number[]) => lines.push(items);
  for (let r = 0; r < 4; r++) for (let c = 0; c < 4; c++) add(r * 4 + c, r * 4 + c + 16, r * 4 + c + 32, r * 4 + c + 48);
  for (let d = 0; d < 4; d++) for (let r = 0; r < 4; r++) { const i = d * 16 + r * 4; add(i, i + 1, i + 2, i + 3); }
  for (let d = 0; d < 4; d++) for (let c = 0; c < 4; c++) { const i = d * 16 + c; add(i, i + 4, i + 8, i + 12); }
  for (let d = 0; d < 4; d++) { const i = d * 16; add(i, i + 5, i + 10, i + 15); add(i + 3, i + 6, i + 9, i + 12); }
  for (let r = 0; r < 4; r++) { const i = r * 4; add(i, i + 17, i + 34, i + 51); add(i + 3, i + 18, i + 33, i + 48); }
  for (let c = 0; c < 4; c++) { const i = c; add(i, i + 20, i + 40, i + 60); add(i + 12, i + 24, i + 36, i + 48); }
  add(0, 21, 42, 63); add(3, 22, 41, 60); add(12, 25, 38, 51); add(15, 26, 37, 48);
  return lines.find((line) => cells[line[0]] && line.every((cell) => cells[cell] === cells[line[0]])) ?? [];
}

export function formatMetric(value: number, digits = 2) {
  if (!Number.isFinite(value)) return '—';
  return Intl.NumberFormat(undefined, { maximumFractionDigits: digits, notation: Math.abs(value) >= 1_000_000 ? 'compact' : 'standard' }).format(value);
}

