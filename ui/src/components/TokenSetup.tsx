import { useState } from 'react';
import { setToken } from '../api/auth';

interface TokenSetupProps {
  onPaired: () => void;
}

/**
 * First-boot pairing screen.
 *
 * Shown when no API token is stored in localStorage. The user reads the
 * 64-character hex token from the ESP32 serial output and enters it here.
 * On submit the token is saved and the main UI is shown.
 */
export function TokenSetup({ onPaired }: TokenSetupProps) {
  const [value, setValue] = useState('');
  const [error, setError] = useState('');

  const handleSubmit = () => {
    const trimmed = value.trim();
    if (trimmed.length === 0) {
      setError('Paste the token from the ESP32 serial output.');
      return;
    }
    if (!/^[0-9a-fA-F]{64}$/.test(trimmed)) {
      setError('Token must be exactly 64 hexadecimal characters.');
      return;
    }
    setToken(trimmed);
    onPaired();
  };

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        minHeight: '100vh',
        background: 'var(--bg-primary)',
        padding: 32,
        gap: 24,
      }}
    >
      <h1 style={{ color: 'var(--text-bright)', fontSize: 28, fontWeight: 700, margin: 0 }}>
        Pair with CNC Stop Block
      </h1>

      <p style={{ color: 'var(--text-secondary)', maxWidth: 480, textAlign: 'center', margin: 0, lineHeight: 1.6 }}>
        Open a serial monitor connected to the ESP32. A 64-character token was
        printed on first boot. Copy it and paste it below.
      </p>

      <div
        style={{
          background: 'var(--bg-secondary)',
          border: '1px solid rgba(255,255,255,0.1)',
          borderRadius: 8,
          padding: '12px 16px',
          fontFamily: "'Courier New', monospace",
          fontSize: 12,
          color: 'var(--text-secondary)',
          maxWidth: 480,
          width: '100%',
        }}
      >
        <span style={{ color: 'var(--green)' }}>[AUTH]</span> *** NEW API TOKEN GENERATED ***<br />
        <span style={{ color: 'var(--green)' }}>[AUTH]</span> Enter this token in the UI pairing screen:<br />
        <span style={{ color: 'var(--text-bright)' }}>[AUTH]</span>{' '}
        <span style={{ color: 'var(--yellow)' }}>a1b2c3d4e5f6…</span>
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: 8, width: '100%', maxWidth: 480 }}>
        <input
          type="text"
          placeholder="Paste 64-character hex token"
          value={value}
          onChange={(e) => {
            setValue(e.target.value);
            setError('');
          }}
          onKeyDown={(e) => e.key === 'Enter' && handleSubmit()}
          style={{
            padding: '12px 16px',
            border: `2px solid ${error ? 'var(--red)' : 'rgba(255,255,255,0.15)'}`,
            borderRadius: 'var(--btn-radius)',
            background: 'var(--bg-secondary)',
            color: 'var(--text-bright)',
            fontSize: 14,
            fontFamily: "'Courier New', monospace",
            minHeight: 'var(--touch-min)',
          }}
          autoFocus
          autoComplete="off"
          spellCheck={false}
        />
        {error && (
          <span style={{ fontSize: 12, color: 'var(--red)', paddingLeft: 4 }}>
            ⚠ {error}
          </span>
        )}
        <button
          onClick={handleSubmit}
          style={{
            padding: '14px 24px',
            borderRadius: 'var(--btn-radius)',
            background: 'var(--blue)',
            color: 'white',
            fontSize: 18,
            fontWeight: 700,
            minHeight: 'var(--touch-min)',
            cursor: 'pointer',
          }}
        >
          Pair Device
        </button>
      </div>

      <p style={{ color: 'var(--text-dim)', fontSize: 12, maxWidth: 480, textAlign: 'center' }}>
        The token is stored in this browser only. To re-pair, rotate the token
        via <code>POST /api/token/rotate</code> or press the factory reset
        combination on the device.
      </p>
    </div>
  );
}
