import { useState } from 'react';
import { useGoTo } from '../hooks/useCommands';

export function GoToInput() {
  const [value, setValue] = useState('');
  const goTo = useGoTo();

  const handleGo = () => {
    const num = parseFloat(value);
    if (!isNaN(num) && num >= 0) {
      goTo.mutate({ position_mm: num });
      setValue('');
    }
  };

  return (
    <div style={{ display: 'flex', gap: 8 }}>
      <input
        type="number"
        placeholder="Enter position (mm)"
        step="0.1"
        min="0"
        max="1200"
        value={value}
        onChange={(e) => setValue(e.target.value)}
        onKeyDown={(e) => e.key === 'Enter' && handleGo()}
        style={{
          flex: 1, padding: '12px 16px',
          border: '2px solid rgba(255,255,255,0.15)',
          borderRadius: 'var(--btn-radius)',
          background: 'var(--bg-secondary)',
          color: 'var(--text-bright)',
          fontSize: 20, fontFamily: "'Courier New', monospace",
          minHeight: 'var(--touch-min)',
        }}
      />
      <button
        onClick={handleGo}
        style={{
          padding: '12px 24px',
          borderRadius: 'var(--btn-radius)',
          background: 'var(--blue)',
          color: 'white',
          fontSize: 18, fontWeight: 700,
          minHeight: 'var(--touch-min)', minWidth: 100,
        }}
      >
        GO TO
      </button>
    </div>
  );
}
