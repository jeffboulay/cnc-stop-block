import { useReset } from '../hooks/useCommands';

export function ErrorBanner({ message }: { message: string }) {
  const reset = useReset();

  return (
    <div style={{
      position: 'fixed', bottom: 0, left: 0, right: 0,
      padding: '12px 20px',
      background: 'var(--red)', color: 'white',
      display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      fontWeight: 600, zIndex: 100,
    }}>
      <span>{message}</span>
      <button
        onClick={() => reset.mutate()}
        style={{
          padding: '8px 20px',
          border: '2px solid white', borderRadius: 8,
          background: 'transparent', color: 'white',
          fontWeight: 700, minHeight: 'var(--touch-min)',
        }}
      >
        RESET
      </button>
    </div>
  );
}
