import { useEffect, useMemo, useRef, useState } from 'react';
import type { AiInfo, ArchiveRecord, GameRecord, JobConfig, JobSnapshot, MoveRecord, NativeEvent, Player, PositionState, RunSummary, SearchReport } from '../shared/types';
import { Board3D } from './Board3D';
import { api, streamJob } from './api';
import { cellsFromGame, formatMetric, visualWinningLine } from './board';

type Mode = 'play' | 'position' | 'match' | 'tournament' | 'archive';
const emptyPosition: PositionState = { schemaVersion: 1, moves: [], cells: Array(64).fill(null), turn: 'A', legalMoves: Array.from({ length: 16 }, (_, i) => i), winner: null, draw: false, winningCells: [] };

function AiSelect({ ais, value, onChange, label }: { ais: AiInfo[]; value: string; onChange: (id: string) => void; label: string }) {
  return <label><span>{label}</span><select value={value} onChange={(event) => onChange(event.target.value)}>
    {ais.map((ai) => <option key={ai.id} value={ai.id}>{ai.name}{ai.depth ? ` · d${ai.depth}` : ''}</option>)}
  </select></label>;
}

function NumberField({ label, value, onChange, min = 1, step = 1 }: { label: string; value: number; onChange: (value: number) => void; min?: number; step?: number }) {
  return <label><span>{label}</span><input type="number" min={min} step={step} value={value} onChange={(event) => onChange(Number(event.target.value))} /></label>;
}


function DepthField({ ai, value, onChange, label }: { ai?: AiInfo; value: number; onChange: (value: number) => void; label: string }) {
  const configurable = ai?.family === "minimax";
  return <label className="depth-field"><span>{label}</span><input aria-label={label} type={configurable ? "number" : "text"} min={1} max={12} step={1} disabled={!configurable} value={configurable ? value : "Fixed"} onChange={(event) => onChange(Number(event.target.value))} /></label>;
}

function SearchCard({ title, report }: { title: string; report?: SearchReport | null }) {
  if (!report) return null;
  return <div className="search-card"><div><span className="eyebrow">{title}</span><strong>Column {report.move}</strong></div>
    <dl><div><dt>Eval</dt><dd>{report.score === null ? 'n/a' : formatMetric(report.score, 5)}</dd></div>
      <div><dt>Time</dt><dd>{formatMetric(report.elapsedMs)} ms</dd></div><div><dt>Nodes</dt><dd>{formatMetric(report.nodes, 0)}</dd></div>
      <div><dt>Collisions</dt><dd>{formatMetric(report.hashCollisions, 0)}</dd></div></dl></div>;
}

function Standings({ result, ais }: { result: RunSummary; ais: AiInfo[] }) {
  const name = (id: string) => ais.find((ai) => ai.id === id)?.name ?? id;
  return <div className="table-wrap"><table><thead><tr><th>#</th><th>AI</th><th>W–D–L</th><th>Pts</th><th>Win rate</th><th>σ</th><th>Time</th><th>Nodes</th></tr></thead>
    <tbody>{result.standings.map((standing, index) => <tr key={standing.aiId}><td>{index + 1}</td><td>{name(standing.aiId)}</td>
      <td>{standing.wins}–{standing.draws}–{standing.losses}</td><td>{standing.points}</td>
      <td>{formatMetric((standing.meanWinRate ?? standing.wins / Math.max(1, standing.played)) * 100)}%</td>
      <td>{formatMetric((standing.stdDevWinRate ?? 0) * 100)}%</td><td>{formatMetric(standing.elapsedMs)} ms</td><td>{formatMetric(standing.nodes, 0)}</td></tr>)}</tbody></table></div>;
}


function TournamentHeatmap({ result, ais }: { result: RunSummary; ais: AiInfo[] }) {
  const name = (id: string) => ais.find((ai) => ai.id === id)?.name ?? id;
  const rows = result.standings.map((standing) => ({
    standing,
    values: [
      standing.byColor?.A ? standing.byColor.A.wins / Math.max(1, standing.byColor.A.played) : null,
      standing.byColor?.B ? standing.byColor.B.wins / Math.max(1, standing.byColor.B.played) : null,
      standing.wins / Math.max(1, standing.played),
      standing.points,
      standing.elapsedMs / Math.max(1, standing.played)
    ]
  }));
  const maxima = rows[0]?.values.map((_, column) => Math.max(0, ...rows.map((row) => row.values[column] ?? 0))) ?? [];
  const hues = [190, 28, 153, 218, 278];
  const styleFor = (value: number | null, column: number) => ({
    backgroundColor: value === null ? "transparent" : `hsla(${hues[column]}, 78%, 48%, ${0.08 + 0.62 * value / Math.max(maxima[column], Number.EPSILON)})`
  });
  return <section className="heatmap" aria-labelledby="tournament-heatmap-title">
    <div className="heatmap-heading"><div><span className="eyebrow">Color performance</span><strong id="tournament-heatmap-title">Tournament heatmap</strong></div><small>Darker cells are higher within each column.</small></div>
    <div className="table-wrap"><table><thead><tr><th>AI</th><th>A · first win</th><th>B · second win</th><th>Overall win</th><th>Points</th><th>Think time / game</th></tr></thead>
      <tbody>{rows.map(({ standing, values }) => <tr key={standing.aiId}><th scope="row">{name(standing.aiId)}</th>
        <td className="heat-cell" style={styleFor(values[0], 0)}>{values[0] === null ? "—" : <>{formatMetric(values[0] * 100)}%<small>{standing.byColor?.A.played} games</small></>}</td>
        <td className="heat-cell" style={styleFor(values[1], 1)}>{values[1] === null ? "—" : <>{formatMetric(values[1] * 100)}%<small>{standing.byColor?.B.played} games</small></>}</td>
        <td className="heat-cell" style={styleFor(values[2], 2)}>{formatMetric(values[2]! * 100)}%<small>{standing.wins}/{standing.played} wins</small></td>
        <td className="heat-cell" style={styleFor(values[3], 3)}>{standing.points}<small>{formatMetric(standing.points / Math.max(1, standing.played), 2)} / game</small></td>
        <td className="heat-cell" style={styleFor(values[4], 4)}>{formatMetric(values[4]!)} ms<small>{formatMetric(standing.elapsedMs)} ms total</small></td>
      </tr>)}</tbody></table></div>
  </section>;
}


function MatchupMatrix({ result, ais }: { result: RunSummary; ais: AiInfo[] }) {
  if (!result.matchups?.length) return null;
  const name = (id: string) => ais.find((ai) => ai.id === id)?.name ?? id;
  const entrants = result.standings.map((standing) => standing.aiId);
  const matchups = new Map(result.matchups.map((matchup) => [`${matchup.playerB}::${matchup.playerA}`, matchup]));
  const rate = (value: number, played: number) => `${formatMetric(value / Math.max(1, played) * 100, 1)}%`;
  const cellStyle = (wins: number, losses: number, played: number) => {
    const balance = (wins - losses) / Math.max(1, played);
    const hue = balance > 0 ? 153 : balance < 0 ? 2 : 210;
    return { backgroundColor: `hsla(${hue}, 72%, 45%, ${0.1 + Math.abs(balance) * 0.5})` };
  };
  return <section className="matchup-matrix" aria-labelledby="matchup-matrix-title">
    <div className="heatmap-heading"><div><span className="eyebrow">Head-to-head by color</span><strong id="matchup-matrix-title">Color matchup matrix</strong></div><small>Each cell describes the orange row AI against the cyan column AI.</small></div>
    <div className="matrix-key"><span className="role-a"><b>A</b> Columns · cyan · moves first</span><span className="role-b"><b>B</b> Rows · orange · moves second</span></div>
    <div className="table-wrap"><table><thead><tr><th className="matrix-axis">Second player vs first player</th>{entrants.map((id) => <th key={id} scope="col"><span className="axis-token a">A</span>{name(id)}</th>)}</tr></thead>
      <tbody>{entrants.map((playerB) => <tr key={playerB}><th scope="row"><span className="axis-token b">B</span>{name(playerB)}</th>{entrants.map((playerA) => {
        const matchup = matchups.get(`${playerB}::${playerA}`);
        if (!matchup) return <td className="matrix-empty" key={playerA}>—</td>;
        return <td className="matrix-cell" key={playerA} style={cellStyle(matchup.secondWins, matchup.secondLosses, matchup.played)} title={`${matchup.played} games`} aria-label={`${name(playerB)} as B against ${name(playerA)} as A: ${rate(matchup.secondWins, matchup.played)} wins, ${rate(matchup.draws, matchup.played)} draws, ${rate(matchup.secondLosses, matchup.played)} losses`}>
          <div className="matchup-rates"><span className="rate-win"><b>Win</b>{rate(matchup.secondWins, matchup.played)}</span><span className="rate-draw"><b>Draw</b>{rate(matchup.draws, matchup.played)}</span><span className="rate-loss"><b>Loss</b>{rate(matchup.secondLosses, matchup.played)}</span></div><small>{matchup.played} game{matchup.played === 1 ? "" : "s"}</small>
        </td>;
      })}</tr>)}</tbody></table></div>
  </section>;
}

export function App() {
  const [mode, setMode] = useState<Mode>('play');
  const [ais, setAis] = useState<AiInfo[]>([]);
  const [position, setPosition] = useState<PositionState>(emptyPosition);
  const [redo, setRedo] = useState<number[]>([]);
  const [humanSide, setHumanSide] = useState<Player>('A');
  const [playAi, setPlayAi] = useState('heuristic');
  const [analysisAi, setAnalysisAi] = useState('b2-v1');
  const [analysis, setAnalysis] = useState<{ best: SearchReport; candidates: SearchReport[] } | null>(null);
  const [playerA, setPlayerA] = useState('heuristic');
  const [playerB, setPlayerB] = useState('random');
  const [matchDepthA, setMatchDepthA] = useState(1);
  const [matchDepthB, setMatchDepthB] = useState(1);
  const [batchGames, setBatchGames] = useState(10);
  const [runKind, setRunKind] = useState<'match' | 'batch'>('match');
  const [selectedAis, setSelectedAis] = useState<string[]>(['random', 'heuristic', 'b2-v1', 'b2-v2']);
  const [tournamentGames, setTournamentGames] = useState(4);
  const [includeSelfPlay, setIncludeSelfPlay] = useState(true);
  const [tournamentDepths, setTournamentDepths] = useState<Record<string, number>>({});
  const [repeats, setRepeats] = useState(3);
  const [samples, setSamples] = useState(5);
  const [seed, setSeed] = useState(1);
  const [threads, setThreads] = useState(2);
  const [activeJob, setActiveJob] = useState<JobSnapshot | null>(null);
  const [runResult, setRunResult] = useState<RunSummary | null>(null);
  const [activeGame, setActiveGame] = useState<GameRecord | null>(null);
  const [replayPly, setReplayPly] = useState(0);
  const [playingReplay, setPlayingReplay] = useState(false);
  const [playbackSpeed, setPlaybackSpeed] = useState(650);
  const [layerGap, setLayerGap] = useState(0.25);
  const [view, setView] = useState<'perspective' | 'top' | 'front'>('perspective');
  const [archives, setArchives] = useState<ArchiveRecord[]>([]);
  const [archiveName, setArchiveName] = useState('Interesting position');
  const [message, setMessage] = useState('Ready');
  const cancelRef = useRef<(() => Promise<void>) | null>(null);

  const refreshArchives = async () => setArchives(await api.get<ArchiveRecord[]>('/api/archive'));
  const inspect = async (moves: number[]) => {
    const next = await api.inspect(moves); setPosition(next); setAnalysis(null); return next;
  };

  useEffect(() => {
    void Promise.all([api.get<AiInfo[]>('/api/ais'), api.inspect([]), api.get<ArchiveRecord[]>('/api/archive')])
      .then(([loadedAis, initial, stored]) => { setAis(loadedAis); setPosition(initial); setArchives(stored);
        setMatchDepthA(loadedAis.find((ai) => ai.id === playerA)?.depth ?? 1); setMatchDepthB(loadedAis.find((ai) => ai.id === playerB)?.depth ?? 1);
        setTournamentDepths(Object.fromEntries(loadedAis.filter((ai) => ai.family === "minimax").map((ai) => [ai.id, ai.depth ?? 1]))); })
      .catch((error: Error) => setMessage(error.message));
    const activeId = sessionStorage.getItem('connect4-active-job');
    if (activeId) {
      const watch = async () => {
        try {
          const job = await api.get<JobSnapshot>('/api/jobs/' + activeId);
          setActiveJob(job); if (job.result) restoreRunResult(job.result);
          if (['running', 'queued'].includes(job.status)) setTimeout(() => void watch(), 750);
          else sessionStorage.removeItem('connect4-active-job');
        } catch { sessionStorage.removeItem('connect4-active-job'); }
      };
      void watch();
    }
  }, []);

  useEffect(() => {
    if (!playingReplay || !activeGame) return;
    const timer = setInterval(() => setReplayPly((ply) => {
      if (ply >= activeGame.moves.length) { setPlayingReplay(false); return ply; }
      return ply + 1;
    }), playbackSpeed);
    return () => clearInterval(timer);
  }, [playingReplay, activeGame, playbackSpeed]);

  function restoreRunResult(raw: unknown) {
    const result = raw as RunSummary;
    if (!result?.sampledGames) return;
    setRunResult(result); const first = result.sampledGames[0] ?? null; setActiveGame(first); setReplayPly(first?.moves.length ?? 0);
  }

  function launch(config: JobConfig, eventHandler?: (event: NativeEvent) => void) {
    setMessage('Native engine is working…');
    const handle = streamJob(config, (event) => {
      eventHandler?.(event);
      if (event.type === 'progress') setActiveJob((job) => job ? { ...job, progress: event.completed / Math.max(1, event.total), updatedAt: new Date().toISOString() } : job);
    }, setActiveJob);
    cancelRef.current = handle.cancel;
    return handle.promise.then((result) => {
      setActiveJob((job) => job ? { ...job, status: 'complete', progress: 1, result, updatedAt: new Date().toISOString() } : job);
      return result;
    }, (error: unknown) => {
      setActiveJob((job) => job ? { ...job, status: error instanceof Error && error.message === 'Job cancelled' ? 'cancelled' : 'failed',
        error: error instanceof Error ? error.message : 'Job failed', updatedAt: new Date().toISOString() } : job);
      throw error;
    }).finally(() => { cancelRef.current = null; void refreshArchives(); });
  }

  async function runAiMove(moves: number[]) {
    try {
      const raw = await launch({ kind: 'ai-move', aiId: playAi, moves, seed }) as { best: SearchReport };
      const best = raw.best;
      if (best.move >= 0) await inspect([...moves, best.move]);
      setMessage(`AI played column ${best.move} in ${formatMetric(best.elapsedMs)} ms`);
    } catch (error) { setMessage((error as Error).message); }
  }

  async function playColumn(column: number) {
    if (activeJob?.status === 'running' || position.turn !== humanSide || position.winner || position.draw) return;
    try {
      const moves = [...position.moves, column]; const next = await inspect(moves); setRedo([]);
      if (!next.winner && !next.draw && next.turn !== humanSide) await runAiMove(moves);
    } catch (error) { setMessage((error as Error).message); }
  }

  async function resetPlay() {
    setRedo([]); const initial = await inspect([]); setMessage('New game');
    if (humanSide === 'B' && initial.turn === 'A') await runAiMove([]);
  }

  async function addPositionMove(column: number) {
    try { await inspect([...position.moves, column]); setRedo([]); } catch (error) { setMessage((error as Error).message); }
  }

  async function undoPosition(count = 1) {
    if (!position.moves.length) return;
    const removed = position.moves.slice(-count); setRedo((items) => [...removed, ...items]); await inspect(position.moves.slice(0, -count));
  }

  async function redoPosition() {
    if (!redo.length) return; const [move, ...rest] = redo; setRedo(rest); await inspect([...position.moves, move]);
  }

  async function analyzePosition() {
    try {
      const result = await launch({ kind: 'analysis', aiId: analysisAi, moves: position.moves, candidates: true, seed }) as { best: SearchReport; candidates: SearchReport[] };
      setAnalysis(result); setMessage('Analysis complete');
    } catch (error) { setMessage((error as Error).message); }
  }

  async function launchRun(config: JobConfig) {
    setRunResult(null); setActiveGame(null); setReplayPly(0);
    const liveMoves: MoveRecord[] = [];
    try {
      const result = await launch(config, (event) => {
        if (event.type === 'move') {
          liveMoves.push(event.move);
          const game: GameRecord = { id: event.gameId, repeat: 0, pairing: 'live', playerA: config.playerA!, playerB: config.playerB!, playerADepth: config.depthA, playerBDepth: config.depthB, winner: null, draw: false,
            moves: [...liveMoves], totals: { A: { elapsedMs: 0, nodes: 0, hashCollisions: 0 }, B: { elapsedMs: 0, nodes: 0, hashCollisions: 0 } } };
          setActiveGame(game); setReplayPly(game.moves.length);
        }
      }) as RunSummary;
      restoreRunResult(result); setMessage(`${result.completedGames} game${result.completedGames === 1 ? '' : 's'} complete`);
    } catch (error) { setMessage((error as Error).message); }
  }

  async function saveCurrent(kind: 'position' | 'play') {
    try { await api.archive(archiveName, kind, { moves: position.moves, position }); await refreshArchives(); setMessage('Saved to archive'); }
    catch (error) { setMessage((error as Error).message); }
  }

  async function openArchive(record: ArchiveRecord) {
    if (record.kind === 'run') {
      const snapshot = record.payload as JobSnapshot; restoreRunResult(snapshot.result); setMode(snapshot.config.kind === 'tournament' ? 'tournament' : 'match');
    } else {
      const payload = record.payload as { moves?: number[]; position?: PositionState };
      await inspect(payload.moves ?? payload.position?.moves ?? []); setMode(record.kind === 'play' ? 'play' : 'position');
    }
    setMessage(`Opened ${record.name}`);
  }

  const replayCells = useMemo(() => cellsFromGame(activeGame, replayPly), [activeGame, replayPly]);
  const replayWinning = replayPly === activeGame?.moves.length ? visualWinningLine(replayCells) : [];
  const showingReplay = (mode === 'match' || mode === 'tournament' || mode === 'archive') && activeGame;
  const boardCells = showingReplay ? replayCells : position.cells;
  const boardWinning = showingReplay ? replayWinning : position.winningCells;
  const interactive = !showingReplay && !activeJob?.status.includes('running') && ((mode === 'position') || (mode === 'play' && position.turn === humanSide));
  const currentMove = activeGame?.moves[Math.max(0, replayPly - 1)];
  const busy = activeJob?.status === 'running' || activeJob?.status === 'queued';
  const validDepth = (depth: number) => Number.isInteger(depth) && depth >= 1 && depth <= 12;
  const matchDepthsValid = (ais.find((ai) => ai.id === playerA)?.family !== 'minimax' || validDepth(matchDepthA)) && (ais.find((ai) => ai.id === playerB)?.family !== 'minimax' || validDepth(matchDepthB));
  const tournamentDepthsValid = selectedAis.every((id) => ais.find((ai) => ai.id === id)?.family !== 'minimax' || validDepth(tournamentDepths[id]));

  return <main>
    <header><div className="brand"><div className="brand-mark">4³</div><div><span className="eyebrow">Native AI observatory</span><h1>Spatial Lab</h1></div></div>
      <div className="status"><span className={busy ? 'pulse' : ''}></span>{message}</div>
      <nav aria-label="Application modes">{(['play', 'position', 'match', 'tournament', 'archive'] as Mode[]).map((item) =>
        <button key={item} className={mode === item ? 'active' : ''} onClick={() => setMode(item)}>{item === 'position' ? 'Position Lab' : item === 'match' ? 'Match Lab' : item === 'tournament' ? 'Tournament' : item[0].toUpperCase() + item.slice(1)}</button>)}</nav>
    </header>

    <section className="workspace">
      <div className="stage">
        <Board3D cells={boardCells} winningCells={boardWinning} legalMoves={position.legalMoves} interactive={interactive}
          layerGap={layerGap} view={view} onColumn={mode === 'play' ? playColumn : addPositionMove} />
        <div className="stage-tools"><div className="segmented">{(['perspective', 'top', 'front'] as const).map((item) => <button key={item} className={view === item ? 'active' : ''} onClick={() => setView(item)}>{item}</button>)}</div>
          <label className="range"><span>Layer spread</span><input type="range" min="0" max="1.1" step="0.05" value={layerGap} onChange={(event) => setLayerGap(Number(event.target.value))} /></label></div>
        {showingReplay && activeGame && <div className="timeline"><button onClick={() => setReplayPly(0)}>↤</button><button onClick={() => setReplayPly(Math.max(0, replayPly - 1))}>←</button>
          <button className="primary" onClick={() => setPlayingReplay((value) => !value)}>{playingReplay ? 'Pause' : 'Play'}</button>
          <button onClick={() => setReplayPly(Math.min(activeGame.moves.length, replayPly + 1))}>→</button><button onClick={() => setReplayPly(activeGame.moves.length)}>↦</button>
          <input aria-label="Replay position" type="range" min="0" max={activeGame.moves.length} value={replayPly} onChange={(event) => setReplayPly(Number(event.target.value))} />
          <span>{replayPly}/{activeGame.moves.length}</span><select value={playbackSpeed} onChange={(event) => setPlaybackSpeed(Number(event.target.value))}><option value="1100">0.5×</option><option value="650">1×</option><option value="300">2×</option></select></div>}
      </div>

      <aside className="panel">
        {mode === 'play' && <><div className="panel-heading"><span className="eyebrow">Human vs machine</span><h2>Play</h2><p>Click any open tower. The native engine validates every move.</p></div>
          <div className="form-grid"><AiSelect ais={ais} value={playAi} onChange={setPlayAi} label="Opponent" />
            <label><span>Your color</span><select value={humanSide} onChange={(event) => setHumanSide(event.target.value as Player)}><option value="A">Cyan · first</option><option value="B">Orange · second</option></select></label></div>
          <div className="turn-card"><div className={`token ${position.turn.toLowerCase()}`}></div><div><span className="eyebrow">Current state</span><strong>{position.winner ? `${position.winner} wins` : position.draw ? 'Draw' : `${position.turn}'s turn`}</strong></div></div>
          <div className="button-row"><button className="primary" onClick={() => void resetPlay()}>New game</button><button disabled={position.moves.length < 1 || busy} onClick={() => void undoPosition(Math.min(2, position.moves.length))}>Undo turn</button></div>
          <ArchiveSave name={archiveName} setName={setArchiveName} onSave={() => void saveCurrent('play')} />
        </>}

        {mode === 'position' && <><div className="panel-heading"><span className="eyebrow">Legal move workspace</span><h2>Position Lab</h2><p>Build, branch, and ask an engine what it sees.</p></div>
          <div className="button-row"><button disabled={!position.moves.length} onClick={() => void undoPosition()}>Undo</button><button disabled={!redo.length} onClick={() => void redoPosition()}>Redo</button><button onClick={() => { setRedo([]); void inspect([]); }}>Clear</button></div>
          <div className="position-code"><span>Ply {position.moves.length} · {position.turn} to move</span><code>{position.moves.length ? position.moves.join(' · ') : 'Empty board'}</code></div>
          <AiSelect ais={ais} value={analysisAi} onChange={setAnalysisAi} label="Analysis engine" />
          <button className="primary full" disabled={busy || !!position.winner || position.draw} onClick={() => void analyzePosition()}>Best move + score every column</button>
          <SearchCard title="Root choice" report={analysis?.best} />
          {analysis?.candidates.length ? <CandidateTable reports={analysis.candidates} turn={position.turn} /> : null}
          <ArchiveSave name={archiveName} setName={setArchiveName} onSave={() => void saveCurrent('position')} />
        </>}

        {mode === 'match' && <><div className="panel-heading"><span className="eyebrow">Head-to-head</span><h2>Match Lab</h2><p>Watch a full game or benchmark a balanced series.</p></div>
          <div className="form-grid"><AiSelect ais={ais} value={playerA} onChange={(id) => { setPlayerA(id); setMatchDepthA(ais.find((ai) => ai.id === id)?.depth ?? 1); }} label="Player A · cyan" /><AiSelect ais={ais} value={playerB} onChange={(id) => { setPlayerB(id); setMatchDepthB(ais.find((ai) => ai.id === id)?.depth ?? 1); }} label="Player B · orange" /></div>
          <div className="form-grid depth-grid"><DepthField ai={ais.find((ai) => ai.id === playerA)} value={matchDepthA} onChange={setMatchDepthA} label="Player A depth" /><DepthField ai={ais.find((ai) => ai.id === playerB)} value={matchDepthB} onChange={setMatchDepthB} label="Player B depth" /></div>
          <label><span>Run type</span><select value={runKind} onChange={(event) => setRunKind(event.target.value as 'match' | 'batch')}><option value="match">Live single game</option><option value="batch">Balanced batch</option></select></label>
          {runKind === 'batch' && <NumberField label="Games (even)" value={batchGames} min={2} step={2} onChange={setBatchGames} />}
          <RunOptions samples={samples} setSamples={setSamples} seed={seed} setSeed={setSeed} threads={threads} setThreads={setThreads} />
          {playerA === playerB && <p className="self-play-note">Self-play enabled: each color gets an independent AI instance.</p>}
          <button className="primary full" disabled={busy || !matchDepthsValid || (runKind === 'batch' && batchGames % 2 !== 0)} onClick={() => void launchRun({ kind: runKind, playerA, playerB, games: runKind === 'match' ? 1 : batchGames, samples, seed, threads, depthA: ais.find((ai) => ai.id === playerA)?.family === 'minimax' ? matchDepthA : undefined, depthB: ais.find((ai) => ai.id === playerB)?.family === 'minimax' ? matchDepthB : undefined })}>Run {runKind}</button>
          <JobProgress job={activeJob} onCancel={() => void cancelRef.current?.()} />
          {runResult && <Results result={runResult} ais={ais} active={activeGame} setActive={(game) => { setActiveGame(game); setReplayPly(game.moves.length); }} />}
          <SearchCard title={`Ply ${replayPly}`} report={currentMove} />
        </>}

        {mode === 'tournament' && <><div className="panel-heading"><span className="eyebrow">Repeated round robin</span><h2>Tournament Lab</h2><p>Balanced colors, repeat variance, deterministic seeds.</p></div>
          <div className="ai-checks">{ais.map((ai) => <div className="ai-entry" key={ai.id}><label><input type="checkbox" checked={selectedAis.includes(ai.id)} onChange={() => setSelectedAis((items) => items.includes(ai.id) ? items.filter((id) => id !== ai.id) : [...items, ai.id])} /><span>{ai.name}</span></label>{ai.family === "minimax" ? <label className="inline-depth"><span>Depth</span><input aria-label={`${ai.name} depth`} type="number" min="1" max="12" value={tournamentDepths[ai.id] ?? ai.depth ?? 1} disabled={!selectedAis.includes(ai.id)} onChange={(event) => setTournamentDepths((depths) => ({ ...depths, [ai.id]: Number(event.target.value) }))} /></label> : <small>Fixed policy</small>}</div>)}</div>
          <div className="form-grid"><NumberField label="Games / pair (even)" value={tournamentGames} min={2} step={2} onChange={setTournamentGames} /><NumberField label="Repeats" value={repeats} onChange={setRepeats} /></div>
          <RunOptions samples={samples} setSamples={setSamples} seed={seed} setSeed={setSeed} threads={threads} setThreads={setThreads} />
          <label className="check-option"><input type="checkbox" checked={includeSelfPlay} onChange={(event) => setIncludeSelfPlay(event.target.checked)} /><span>Include self-play · fills matrix diagonal</span></label>
          <button className="primary full" disabled={busy || !tournamentDepthsValid || selectedAis.length < (includeSelfPlay ? 1 : 2) || tournamentGames % 2 !== 0} onClick={() => void launchRun({ kind: 'tournament', aiIds: selectedAis, games: tournamentGames, repeats, samples, seed, threads, includeSelfPlay, depths: Object.fromEntries(selectedAis.filter((id) => ais.find((ai) => ai.id === id)?.family === 'minimax').map((id) => [id, tournamentDepths[id] ?? ais.find((ai) => ai.id === id)?.depth ?? 1])) })}>Start tournament · {(selectedAis.length * (selectedAis.length - 1) / 2 + (includeSelfPlay ? selectedAis.length : 0)) * tournamentGames * repeats} games</button>
          <JobProgress job={activeJob} onCancel={() => void cancelRef.current?.()} />
          {runResult && <Results result={runResult} ais={ais} active={activeGame} heatmap setActive={(game) => { setActiveGame(game); setReplayPly(game.moves.length); }} />}
        </>}

        {mode === 'archive' && <><div className="panel-heading"><span className="eyebrow">Server storage</span><h2>Archive</h2><p>Completed and cancelled runs are saved automatically.</p></div>
          <button onClick={() => void refreshArchives()}>Refresh archive</button><div className="archive-list">{archives.length === 0 && <p className="muted">Nothing saved yet.</p>}
            {archives.map((record) => <article key={record.id}><div><span className="badge">{record.kind}</span><strong>{record.name}</strong><small>{new Date(record.updatedAt).toLocaleString()}</small></div>
              <div className="button-row"><button onClick={() => void openArchive(record)}>Open</button><button className="danger" onClick={async () => { await api.delete(`/api/archive/${record.id}`); await refreshArchives(); }}>Delete</button></div></article>)}</div>
        </>}
      </aside>
    </section>
  </main>;
}

function ArchiveSave({ name, setName, onSave }: { name: string; setName: (value: string) => void; onSave: () => void }) {
  return <div className="archive-save"><label><span>Archive name</span><input value={name} maxLength={120} onChange={(event) => setName(event.target.value)} /></label><button disabled={!name.trim()} onClick={onSave}>Save</button></div>;
}

function RunOptions({ samples, setSamples, seed, setSeed, threads, setThreads }: { samples: number; setSamples: (v: number) => void; seed: number; setSeed: (v: number) => void; threads: number; setThreads: (v: number) => void }) {
  return <details><summary>Run options</summary><div className="form-grid"><NumberField label="Replay samples" value={samples} min={0} onChange={setSamples} /><NumberField label="Seed" value={seed} onChange={setSeed} /><NumberField label="Workers" value={threads} onChange={setThreads} /></div></details>;
}

function JobProgress({ job, onCancel }: { job: JobSnapshot | null; onCancel: () => void }) {
  if (!job || !['running', 'queued'].includes(job.status)) return null;
  return <div className="job-progress"><div><span>Native job</span><strong>{Math.round(job.progress * 100)}%</strong></div><progress value={job.progress} max="1" /><button onClick={onCancel}>Cancel</button></div>;
}

function CandidateTable({ reports, turn }: { reports: SearchReport[]; turn: Player }) {
  const ordered = [...reports].sort((a, b) => {
    if (a.outcome) return a.outcome === `${turn}_WIN` ? -1 : 1;
    if (b.outcome) return b.outcome === `${turn}_WIN` ? 1 : -1;
    const delta = (a.score ?? 0) - (b.score ?? 0); return turn === 'A' ? -delta : delta;
  });
  return <div className="table-wrap candidates"><table><thead><tr><th>Move</th><th>Reply</th><th>Eval</th><th>Time</th><th>Nodes</th></tr></thead><tbody>{ordered.map((report) => <tr key={report.move}><td>{report.move}</td><td>{report.reply ?? '—'}</td><td>{report.outcome ?? (report.score === null ? 'n/a' : formatMetric(report.score, 5))}</td><td>{formatMetric(report.elapsedMs)} ms</td><td>{formatMetric(report.nodes, 0)}</td></tr>)}</tbody></table></div>;
}

function Results({ result, ais, active, heatmap = false, setActive }: { result: RunSummary; ais: AiInfo[]; active: GameRecord | null; heatmap?: boolean; setActive: (game: GameRecord) => void }) {
  const playerLabel = (id: string, depth?: number) => ais.find((ai) => ai.id === id)?.family === "minimax" && depth ? id + " · d" + depth : id;
  return <div className="results"><div className="result-title"><div><span className="eyebrow">Results</span><strong>{result.completedGames}/{result.totalGames} games</strong></div></div>
    <Standings result={result} ais={ais} />
    {heatmap && <TournamentHeatmap result={result} ais={ais} />}
    {heatmap && <MatchupMatrix result={result} ais={ais} />} 
    {result.sampledGames.length > 0 && <div className="samples"><span>Representative replays</span>{result.sampledGames.map((game) => <button key={`${game.pairing}-${game.id}`} className={active?.id === game.id && active.pairing === game.pairing ? 'active' : ''} onClick={() => setActive(game)}>
      {playerLabel(game.playerA, game.playerADepth)} vs {playerLabel(game.playerB, game.playerBDepth)} · {game.draw ? 'draw' : `${game.winner} win`}</button>)}</div>}
  </div>;
}
