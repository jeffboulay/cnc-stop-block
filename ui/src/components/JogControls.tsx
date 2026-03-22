import { useState } from 'react';
import { useJog } from '../hooks/useCommands';

const STEP_SIZES = [0.1, 1, 10, 100];

export function JogControls() {
  const [stepSize, setStepSize] = useState(0.1);
  const jog = useJog();

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
      {/* Step size selector */}
      <div style={{ display: 'flex', gap: 6 }}>
        {STEP_SIZES.map((s) => (
          <button
            key={s}
            onClick={() => setStepSize(s)}
            style={{
              flex: 1,
              padding: 10,
              border: `2px solid ${stepSize === s ? 'var(--blue)' : 'rgba(255,255,255,0.15)'}`,
              borderRadius: 'var(--btn-radius)',
              background: stepSize === s ? 'rgba(68,138,255,0.2)' : 'var(--bg-secondary)',
              color: stepSize === s ? 'var(--blue)' : 'var(--text-primary)',
              fontSize: 16,
              fontWeight: 600,
              minHeight: 'var(--touch-min)',
            }}
          >
            {s}
          </button>
        ))}
      </div>

      {/* Jog buttons */}
      <div style={{ display: 'flex', gap: 8 }}>
        <button
          onClick={() => jog.mutate({ distance_mm: -stepSize })}
          style={{
            flex: 1, padding: 14,
            border: '2px solid rgba(255,255,255,0.15)',
            borderRadius: 'var(--btn-radius)',
            background: 'var(--bg-secondary)',
            color: 'var(--text-primary)',
            fontSize: 18, fontWeight: 700, minHeight: 56,
          }}
        >
          ◀ JOG
        </button>
        <button
          onClick={() => jog.mutate({ distance_mm: stepSize })}
          style={{
            flex: 1, padding: 14,
            border: '2px solid rgba(255,255,255,0.15)',
            borderRadius: 'var(--btn-radius)',
            background: 'var(--bg-secondary)',
            color: 'var(--text-primary)',
            fontSize: 18, fontWeight: 700, minHeight: 56,
          }}
        >
          JOG ▶
        </button>
      </div>
    </div>
  );
}
