interface Props {
  homed: boolean;
  locked: boolean;
  dust: boolean;
  clamps: boolean;
}

function Dot({ active }: { active: boolean }) {
  return (
    <span style={{
      width: 10, height: 10, borderRadius: '50%',
      background: active ? 'var(--green)' : '#444',
      boxShadow: active ? '0 0 8px rgba(0,230,118,0.5)' : 'none',
      transition: 'background 0.3s',
      display: 'inline-block',
    }} />
  );
}

export function StatusIndicators({ homed, locked, dust, clamps }: Props) {
  const indicators = [
    { label: 'Homed', active: homed },
    { label: 'Locked', active: locked },
    { label: 'Dust', active: dust },
    { label: 'Clamps', active: clamps },
  ];

  return (
    <div style={{
      display: 'flex', gap: 12, padding: '8px 12px',
      background: 'var(--bg-secondary)', borderRadius: 10,
    }}>
      {indicators.map((ind) => (
        <div key={ind.label} style={{
          display: 'flex', alignItems: 'center', gap: 6,
          fontSize: 12, color: 'var(--text-dim)',
        }}>
          <Dot active={ind.active} />
          {ind.label}
        </div>
      ))}
    </div>
  );
}
