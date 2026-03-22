export function DRODisplay({ position }: { position: string }) {
  return (
    <div style={{
      background: '#111',
      border: '2px solid var(--dro-color)',
      borderRadius: 16,
      padding: '16px 24px',
      textAlign: 'center',
    }}>
      <div style={{ fontSize: 12, color: 'var(--text-dim)', letterSpacing: 2, marginBottom: 4 }}>
        POSITION
      </div>
      <div style={{
        fontSize: 72,
        fontWeight: 700,
        fontFamily: "'Courier New', monospace",
        color: 'var(--dro-color)',
        lineHeight: 1,
        textShadow: '0 0 20px rgba(0,230,118,0.3)',
      }}>
        {parseFloat(position).toFixed(2)}
      </div>
      <div style={{ fontSize: 18, color: 'var(--text-dim)', marginTop: 4 }}>mm</div>
    </div>
  );
}
