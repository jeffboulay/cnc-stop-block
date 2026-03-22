import type { SystemStatus } from '../api/types';
import {
  useHome, useLock, useUnlock, useNextCut,
  useStartCut, useEndCut, useEStop, useReset,
} from '../hooks/useCommands';

export function ControlButtons({ status }: { status: SystemStatus }) {
  const home = useHome();
  const lock = useLock();
  const unlock = useUnlock();
  const nextCut = useNextCut();
  const startCut = useStartCut();
  const endCut = useEndCut();
  const estop = useEStop();
  const reset = useReset();

  const isError = status.state === 'ERROR' || status.state === 'ESTOP';
  const isCutting = status.state === 'CUTTING';

  const btnStyle = (bg: string, color = 'white'): React.CSSProperties => ({
    padding: 16,
    borderRadius: 'var(--btn-radius)',
    fontSize: 16, fontWeight: 700, letterSpacing: 1,
    minHeight: 56,
    background: bg, color,
  });

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8 }}>
      <button onClick={() => home.mutate()} style={btnStyle('var(--blue)')}>
        HOME
      </button>

      <button
        onClick={() => status.locked ? unlock.mutate() : lock.mutate()}
        style={btnStyle(status.locked ? 'var(--green)' : '#6c757d', status.locked ? '#111' : 'white')}
      >
        {status.locked ? 'UNLOCK' : 'LOCK'}
      </button>

      <button onClick={() => nextCut.mutate()} style={btnStyle('var(--target-color)')}>
        NEXT CUT
      </button>

      <button
        onClick={() => isCutting ? endCut.mutate() : startCut.mutate()}
        style={btnStyle(isCutting ? 'var(--red)' : 'var(--orange)', isCutting ? 'white' : '#111')}
      >
        {isCutting ? 'END CUT' : 'START CUT'}
      </button>

      {isError ? (
        <button
          onClick={() => reset.mutate()}
          style={{ ...btnStyle('var(--yellow)', '#111'), gridColumn: 'span 2', fontSize: 18 }}
        >
          RESET
        </button>
      ) : (
        <button
          onClick={() => estop.mutate()}
          style={{
            ...btnStyle('var(--red)'),
            gridColumn: 'span 2',
            fontSize: 20, minHeight: 64,
            border: '3px solid #ff5252',
            textShadow: '0 1px 2px rgba(0,0,0,0.5)',
          }}
        >
          E-STOP
        </button>
      )}
    </div>
  );
}
