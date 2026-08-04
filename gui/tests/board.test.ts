import { describe, expect, it } from 'vitest';
import { cellCoordinates, cellsFromGame, moveFromCoordinates, visualWinningLine } from '../src/board.ts';
import type { GameRecord } from '../shared/types.ts';

describe('board presentation helpers', () => {
  it('maps native cell indices to 3D coordinates', () => {
    expect(cellCoordinates(0)).toEqual({ column: 0, row: 0, layer: 0 });
    expect(cellCoordinates(63)).toEqual({ column: 3, row: 3, layer: 3 });
    expect(cellCoordinates(21)).toEqual({ column: 1, row: 1, layer: 1 });
    expect(moveFromCoordinates(0, 0)).toBe(0);
    expect(moveFromCoordinates(1, 1)).toBe(5);
    expect(moveFromCoordinates(3, 3)).toBe(15);
  });

  it('scrubs game history without mutating its record', () => {
    const game = { moves: [{ cell: 0, player: 'A' }, { cell: 1, player: 'B' }] } as GameRecord;
    expect(cellsFromGame(game, 1)[0]).toBe('A');
    expect(cellsFromGame(game, 1)[1]).toBeNull();
    expect(game.moves).toHaveLength(2);
  });

  it('highlights a spatial winning line', () => {
    const cells = Array(64).fill(null);
    for (const cell of [0, 21, 42, 63]) cells[cell] = 'A';
    expect(visualWinningLine(cells)).toEqual([0, 21, 42, 63]);
  });
});

