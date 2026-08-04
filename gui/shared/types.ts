export const SCHEMA_VERSION = 1 as const;

export type Player = 'A' | 'B';
export type JobKind = 'ai-move' | 'analysis' | 'match' | 'batch' | 'tournament';
export type JobStatus = 'queued' | 'running' | 'complete' | 'cancelled' | 'failed';

export interface AiInfo {
  id: string;
  name: string;
  family: 'random' | 'heuristic' | 'minimax';
  depth: number | null;
}

export interface PositionState {
  schemaVersion: 1;
  moves: number[];
  cells: Array<Player | null>;
  turn: Player;
  legalMoves: number[];
  winner: Player | null;
  draw: boolean;
  winningCells: number[];
}

export interface SearchReport {
  move: number;
  score: number | null;
  elapsedMs: number;
  nodes: number;
  hashCollisions: number;
  reply?: number | null;
  outcome?: 'A_WIN' | 'B_WIN' | 'DRAW' | null;
}

export interface MoveRecord extends SearchReport {
  ply: number;
  player: Player;
  cell: number;
}

export interface GameRecord {
  id: string;
  repeat: number;
  pairing: string;
  playerA: string;
  playerB: string;
  winner: Player | null;
  draw: boolean;
  moves: MoveRecord[];
  totals: Record<Player, { elapsedMs: number; nodes: number; hashCollisions: number }>;
}

export interface Standing {
  aiId: string;
  played: number;
  wins: number;
  draws: number;
  losses: number;
  points: number;
  elapsedMs: number;
  nodes: number;
  hashCollisions: number;
  meanWinRate?: number;
  stdDevWinRate?: number;
  byColor?: Record<Player, ColorStanding>;
}

export interface ColorStanding {
  played: number;
  wins: number;
  draws: number;
  losses: number;
  points: number;
  elapsedMs: number;
  nodes: number;
  hashCollisions: number;
}

export interface RunSummary {
  kind: JobKind;
  completedGames: number;
  totalGames: number;
  standings: Standing[];
  sampledGames: GameRecord[];
}

export interface JobConfig {
  kind: JobKind;
  moves?: number[];
  aiId?: string;
  playerA?: string;
  playerB?: string;
  aiIds?: string[];
  games?: number;
  repeats?: number;
  samples?: number;
  seed?: number;
  threads?: number;
  candidates?: boolean;
}

export interface JobSnapshot {
  schemaVersion: 1;
  id: string;
  status: JobStatus;
  config: JobConfig;
  createdAt: string;
  updatedAt: string;
  progress: number;
  events: NativeEvent[];
  result?: unknown;
  error?: string;
}

export type NativeEvent =
  | { type: 'started'; totalGames?: number }
  | { type: 'move'; gameId: string; move: MoveRecord }
  | { type: 'candidate'; report: SearchReport }
  | { type: 'progress'; completed: number; total: number }
  | { type: 'complete'; result: unknown }
  | { type: 'error'; message: string };

export interface ArchiveRecord {
  schemaVersion: 1;
  id: string;
  name: string;
  kind: 'run' | 'position' | 'play';
  createdAt: string;
  updatedAt: string;
  payload: unknown;
}
