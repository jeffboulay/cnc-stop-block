import { useState } from 'react';
import { useCutList, useClearCutList, useResetCutList, useRemoveCut } from '../hooks/useCutList';
import { useGoTo } from '../hooks/useCommands';
import { AddCutModal } from './AddCutModal';

export function CutList({ activeIndex }: { activeIndex: number }) {
  const [showAdd, setShowAdd] = useState(false);
  const { data: cuts = [] } = useCutList();
  const clearList = useClearCutList();
  const resetList = useResetCutList();
  const removeCut = useRemoveCut();
  const goTo = useGoTo();

  const smallBtn: React.CSSProperties = {
    padding: '6px 12px',
    border: '1px solid rgba(255,255,255,0.15)',
    borderRadius: 8,
    background: 'transparent',
    color: 'var(--text-dim)',
    fontSize: 12, minHeight: 32,
  };

  return (
    <div style={{
      flex: 1, display: 'flex', flexDirection: 'column',
      background: 'var(--bg-secondary)', borderRadius: 12,
      overflow: 'hidden', minHeight: 0,
    }}>
      {/* Header */}
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '10px 12px',
        borderBottom: '1px solid rgba(255,255,255,0.1)',
      }}>
        <h3 style={{ fontSize: 14, color: 'var(--text-primary)' }}>Cut List</h3>
        <div style={{ display: 'flex', gap: 6 }}>
          <button onClick={() => setShowAdd(true)} style={smallBtn}>+ Add</button>
          <button onClick={() => resetList.mutate()} style={smallBtn}>Reset</button>
          <button onClick={() => { if (confirm('Clear all cuts?')) clearList.mutate(); }} style={smallBtn}>Clear</button>
        </div>
      </div>

      {/* Items */}
      <div style={{ flex: 1, overflowY: 'auto', WebkitOverflowScrolling: 'touch' }}>
        {cuts.map((cut, i) => (
          <div
            key={i}
            style={{
              display: 'flex', alignItems: 'center',
              padding: '10px 12px', gap: 10,
              minHeight: 'var(--touch-min)',
              borderBottom: '1px solid rgba(255,255,255,0.05)',
              background: i === activeIndex ? 'rgba(68,138,255,0.15)' : undefined,
              borderLeft: i === activeIndex ? '3px solid var(--blue)' : '3px solid transparent',
              opacity: cut.completed ? 0.4 : 1,
              textDecoration: cut.completed ? 'line-through' : 'none',
            }}
          >
            <span style={{ flex: 1, fontSize: 14 }}>{cut.label || `Cut ${i + 1}`}</span>
            <span style={{
              fontFamily: "'Courier New', monospace",
              fontSize: 16, fontWeight: 600, color: 'var(--text-bright)',
            }}>
              {cut.length_mm.toFixed(1)}
            </span>
            <span style={{ fontSize: 12, color: 'var(--text-dim)', minWidth: 30, textAlign: 'right' }}>
              x{cut.quantity}
            </span>
            <button
              onClick={() => goTo.mutate({ position_mm: cut.length_mm })}
              style={{
                padding: '6px 12px', borderRadius: 6,
                background: 'var(--blue)', color: 'white',
                fontSize: 12, fontWeight: 600, minHeight: 32,
              }}
            >
              GO
            </button>
            <button
              onClick={() => removeCut.mutate(i)}
              style={{
                padding: '6px 8px', borderRadius: 6,
                background: 'transparent', color: 'var(--text-dim)',
                fontSize: 14, minHeight: 32,
                border: '1px solid rgba(255,255,255,0.1)',
              }}
            >
              ✕
            </button>
          </div>
        ))}
        {cuts.length === 0 && (
          <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-dim)', fontSize: 14 }}>
            No cuts added yet
          </div>
        )}
      </div>

      {showAdd && <AddCutModal onClose={() => setShowAdd(false)} />}
    </div>
  );
}
