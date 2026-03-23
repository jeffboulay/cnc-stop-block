export function TargetDisplay({ target, current }: { target: string; current: string }) {
  const error = Math.abs(parseFloat(current) - parseFloat(target));

  return (
    <div style={{ display: 'flex', gap: 10 }}>
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: 10,
        padding: 8,
        background: 'var(--bg-secondary)',
        borderRadius: 10,
      }}>
        <span style={{ fontSize: 11, letterSpacing: 1, color: 'var(--text-dim)', width: 50 }}>TARGET</span>
        <span style={{ fontSize: 28, fontFamily: "'Courier New', monospace", fontWeight: 700, color: 'var(--target-color)' }}>
          {parseFloat(target).toFixed(2)}
        </span>
        <span style={{ fontSize: 14, color: 'var(--text-dim)' }}>mm</span>
      </div>
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: 10,
        padding: 8,
        background: 'var(--bg-secondary)',
        borderRadius: 10,
      }}>
        <span style={{ fontSize: 11, letterSpacing: 1, color: 'var(--text-dim)', width: 50 }}>OFFSET</span>
        <span style={{ fontSize: 28, fontFamily: "'Courier New', monospace", fontWeight: 700, color: 'var(--yellow)' }}>
          {error.toFixed(2)}
        </span>
        <span style={{ fontSize: 14, color: 'var(--text-dim)' }}>mm</span>
      </div>
    </div>
  );
}
