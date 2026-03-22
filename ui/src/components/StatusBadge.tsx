import type { SystemState } from '../api/types';

const stateStyles: Record<string, string> = {
  BOOT: '#448aff',
  WIFI_CONNECT: '#448aff',
  NEEDS_HOMING: '#ffd600',
  HOMING: '#ffd600',
  IDLE: '#00e676',
  UNLOCKING: '#00e676',
  MOVING: '#ffd600',
  SETTLING: '#00e676',
  LOCKING: '#00e676',
  LOCKED: '#00e676',
  DUST_SPINUP: '#ff9100',
  CUTTING: '#ff9100',
  ERROR: '#ff1744',
  ESTOP: '#ff1744',
};

export function StatusBadge({ state }: { state: SystemState }) {
  const bg = stateStyles[state] ?? '#448aff';
  const isDark = ['NEEDS_HOMING', 'HOMING', 'MOVING', 'IDLE', 'LOCKED', 'CUTTING', 'DUST_SPINUP'].includes(state);

  return (
    <span
      style={{
        padding: '4px 14px',
        borderRadius: 20,
        fontSize: 13,
        fontWeight: 700,
        letterSpacing: 1,
        background: bg,
        color: isDark ? '#111' : '#fff',
        animation: state === 'ESTOP' ? 'flash 0.5s infinite' : undefined,
      }}
    >
      {state}
    </span>
  );
}
