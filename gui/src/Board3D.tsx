import { Canvas, useFrame, useThree } from '@react-three/fiber';
import { ContactShadows, OrbitControls } from '@react-three/drei';
import { useEffect, useMemo, useRef, useState } from 'react';
import * as THREE from 'three';
import type { ThreeEvent } from '@react-three/fiber';
import type { Player } from '../shared/types';
import { cellCoordinates, moveFromCoordinates } from './board';

interface Props {
  cells: Array<Player | null>;
  legalMoves?: number[];
  winningCells?: number[];
  interactive?: boolean;
  layerGap: number;
  view: 'perspective' | 'top' | 'front';
  onColumn?: (column: number) => void;
}

interface PickableProps {
  enabled: boolean;
  move: number;
  onHover: (move: number | null) => void;
  onSelect: (move: number) => void;
}

function pickHandlers({ enabled, move, onHover, onSelect }: PickableProps) {
  return {
    onPointerOver: (event: ThreeEvent<PointerEvent>) => { event.stopPropagation(); if (enabled) onHover(move); },
    onPointerOut: (event: ThreeEvent<PointerEvent>) => { event.stopPropagation(); if (enabled) onHover(null); },
    onClick: (event: ThreeEvent<MouseEvent>) => { event.stopPropagation(); if (enabled) onSelect(move); }
  };
}

function CameraRig({ view }: { view: Props['view'] }) {
  const { camera } = useThree();
  useEffect(() => {
    const positions = { perspective: [8, 7, 9], top: [0, 13, 0.01], front: [0, 1, 13] } as const;
    const [x, y, z] = positions[view];
    camera.position.set(x, y, z); camera.lookAt(0, 0, 0); camera.updateProjectionMatrix();
  }, [camera, view]);
  return null;
}

function Piece({ player, position, winning, pickable }: {
  player: Player;
  position: [number, number, number];
  winning: boolean;
  pickable?: PickableProps;
}) {
  const mesh = useRef<THREE.Mesh>(null);
  const start = useRef(position[1] + 5);
  useFrame((_state, delta) => {
    if (!mesh.current) return;
    start.current = THREE.MathUtils.damp(start.current, position[1], 9, delta);
    mesh.current.position.y = start.current;
    mesh.current.rotation.y += delta * 0.3;
  });
  return <mesh ref={mesh} position={position} castShadow {...(pickable ? pickHandlers(pickable) : {})}>
    <sphereGeometry args={[0.43, 32, 24]} />
    <meshStandardMaterial color={player === 'A' ? '#22d3ee' : '#fb923c'} roughness={0.18} metalness={0.42}
      emissive={winning ? (player === 'A' ? '#0ea5e9' : '#f97316') : '#000000'} emissiveIntensity={winning ? 1.5 : 0} />
  </mesh>;
}

export function Board3D({ cells, legalMoves = [], winningCells = [], interactive = false, layerGap, view, onColumn }: Props) {
  const [hovered, setHovered] = useState<number | null>(null);
  const gap = 1.05 + layerGap;
  const pieces = useMemo(() => cells.flatMap((player, cell) => player ? [{ player, cell }] : []), [cells]);
  const ghostCell = hovered === null ? -1 : [0, 1, 2, 3].map((layer) => hovered + layer * 16).find((cell) => !cells[cell]) ?? -1;
  const select = (move: number) => onColumn?.(move);

  return <div className={`board-canvas${interactive ? ' interactive' : ''}`} aria-label="Interactive 3D Connect Four board">
    <Canvas shadows camera={{ position: [8, 7, 9], fov: 42 }} dpr={[1, 1.75]}>
      <color attach="background" args={['#06101b']} />
      <fog attach="fog" args={['#06101b', 15, 27]} />
      <ambientLight intensity={0.75} />
      <directionalLight position={[7, 11, 5]} intensity={2.3} castShadow />
      <pointLight position={[-7, 3, -4]} color="#2563eb" intensity={35} />
      <CameraRig view={view} />

      {[0, 1, 2, 3].map((layer) => <group key={layer} position={[0, (layer - 1.5) * gap, 0]}>
        <mesh>
          <boxGeometry args={[5.25, 0.055, 5.25]} />
          <meshStandardMaterial color="#5b7891" transparent opacity={0.22} wireframe />
        </mesh>
        {[0, 1, 2, 3].flatMap((row) => [0, 1, 2, 3].map((column) => {
          const move = moveFromCoordinates(row, column);
          const enabled = interactive && legalMoves.includes(move);
          return <mesh key={`${row}-${column}`} position={[(column - 1.5) * 1.2, 0, (row - 1.5) * 1.2]}
            {...pickHandlers({ enabled, move, onHover: setHovered, onSelect: select })}>
            <cylinderGeometry args={[0.5, 0.5, 0.06, 32]} />
            <meshStandardMaterial color={hovered === move ? '#24748b' : '#17324a'} transparent opacity={hovered === move ? 0.72 : 0.38} />
          </mesh>;
        }))}
      </group>)}

      {[0, 1, 2, 3].flatMap((row) => [0, 1, 2, 3].map((column) => {
        const move = moveFromCoordinates(row, column);
        const enabled = interactive && legalMoves.includes(move);
        const pickable = { enabled, move, onHover: setHovered, onSelect: select };
        return <group key={`tower-${move}`} position={[(column - 1.5) * 1.2, 0, (row - 1.5) * 1.2]}>
          <mesh {...pickHandlers(pickable)}>
            <cylinderGeometry args={[0.025, 0.025, 3 * gap + 0.12, 10]} />
            <meshStandardMaterial color={hovered === move ? '#67e8f9' : '#7694a8'} transparent
              opacity={hovered === move ? 0.88 : 0.42} metalness={0.55} roughness={0.28} />
          </mesh>
          <group position={[0, 1.5 * gap + 0.48, 0]} {...pickHandlers(pickable)}>
            <mesh rotation={[Math.PI / 2, 0, 0]}>
              <torusGeometry args={[0.32, hovered === move ? 0.048 : 0.028, 10, 32]} />
              <meshStandardMaterial color={hovered === move ? '#67e8f9' : '#7694a8'} transparent opacity={enabled ? 0.8 : 0.2}
                emissive={hovered === move ? '#0891b2' : '#000000'} emissiveIntensity={1.1} />
            </mesh>
            <mesh>
              <cylinderGeometry args={[0.46, 0.46, 0.16, 24]} />
              <meshBasicMaterial transparent opacity={0} depthWrite={false} />
            </mesh>
          </group>
        </group>;
      }))}

      {pieces.map(({ player, cell }) => {
        const { column, row, layer } = cellCoordinates(cell);
        const move = moveFromCoordinates(row, column);
        return <Piece key={cell} player={player} winning={winningCells.includes(cell)}
          position={[(column - 1.5) * 1.2, (layer - 1.5) * gap, (row - 1.5) * 1.2]}
          pickable={{ enabled: interactive && legalMoves.includes(move), move, onHover: setHovered, onSelect: select }} />;
      })}

      {ghostCell >= 0 && (() => {
        const { column, row, layer } = cellCoordinates(ghostCell);
        return <mesh position={[(column - 1.5) * 1.2, (layer - 1.5) * gap, (row - 1.5) * 1.2]}>
          <sphereGeometry args={[0.43, 24, 18]} />
          <meshStandardMaterial color="#e2e8f0" transparent opacity={0.35} />
        </mesh>;
      })()}
      <ContactShadows position={[0, -3.25, 0]} opacity={0.45} scale={12} blur={2.5} />
      <OrbitControls makeDefault minDistance={7} maxDistance={19} enableDamping />
    </Canvas>
    <div className="axis-key"><span>Columns 0–3 →</span><span>Rows 0–3 ↗</span><span>Layers ↑</span></div>
  </div>;
}
