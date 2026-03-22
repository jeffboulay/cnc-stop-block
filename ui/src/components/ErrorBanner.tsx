import { useState } from 'react';
import { useReset } from '../hooks/useCommands';
import { getErrorMessage } from '../api/errorMessages';

interface ErrorBannerProps {
  /** 'ERROR' shows error_code-based message; 'ESTOP' shows the ESTOP message. */
  state: 'ERROR' | 'ESTOP';
  /** Stable firmware error code (only present for state === 'ERROR'). */
  errorCode?: string;
  /** Raw firmware error string — shown in a collapsible details section. */
  rawError?: string;
}

export function ErrorBanner({ state, errorCode, rawError }: ErrorBannerProps) {
  const reset = useReset();
  const [showDetails, setShowDetails] = useState(false);

  // ESTOP gets its own message; ERROR uses the error_code lookup
  const code = state === 'ESTOP' ? 'ESTOP' : errorCode;
  const { title, detail } = getErrorMessage(code);

  return (
    <div style={{
      position: 'fixed', bottom: 0, left: 0, right: 0,
      padding: '12px 20px',
      background: 'var(--red)', color: 'white',
      zIndex: 100,
    }}>
      <div style={{ display: 'flex', alignItems: 'flex-start', gap: 16 }}>

        {/* Message area */}
        <div style={{ flex: 1 }}>
          <div style={{ fontWeight: 700, fontSize: 15 }}>{title}</div>
          <div style={{ fontSize: 13, opacity: 0.88, marginTop: 3 }}>{detail}</div>

          {/* Collapsible raw details — only shown when rawError is present */}
          {rawError && (
            <>
              <button
                onClick={() => setShowDetails((s) => !s)}
                style={{
                  marginTop: 8,
                  padding: '2px 8px',
                  border: '1px solid rgba(255,255,255,0.5)',
                  borderRadius: 4,
                  background: 'transparent',
                  color: 'white',
                  fontSize: 11,
                  cursor: 'pointer',
                }}
              >
                {showDetails ? '▾ Hide details' : '▸ Details'}
              </button>

              {showDetails && (
                <div style={{
                  marginTop: 6,
                  padding: '6px 8px',
                  background: 'rgba(0,0,0,0.3)',
                  borderRadius: 4,
                  fontFamily: 'monospace',
                  fontSize: 11,
                  wordBreak: 'break-all',
                  opacity: 0.9,
                }}>
                  {rawError}
                </div>
              )}
            </>
          )}
        </div>

        {/* Reset button */}
        <button
          onClick={() => reset.mutate()}
          style={{
            padding: '8px 20px',
            flexShrink: 0,
            border: '2px solid white',
            borderRadius: 8,
            background: 'transparent',
            color: 'white',
            fontWeight: 700,
            minHeight: 'var(--touch-min)',
          }}
        >
          RESET
        </button>
      </div>
    </div>
  );
}
