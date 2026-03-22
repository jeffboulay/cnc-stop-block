import { useState } from 'react';
import { useAddCut } from '../hooks/useCutList';

interface Props {
  onClose: () => void;
}

export function AddCutModal({ onClose }: Props) {
  const [label, setLabel] = useState('');
  const [length, setLength] = useState('');
  const [qty, setQty] = useState('1');
  const addCut = useAddCut();

  const handleSave = () => {
    const len = parseFloat(length);
    if (isNaN(len) || len <= 0) return;

    addCut.mutate(
      { label: label || `Cut`, length_mm: len, quantity: parseInt(qty) || 1 },
      { onSuccess: onClose },
    );
  };

  const inputStyle: React.CSSProperties = {
    display: 'block', width: '100%', marginTop: 4,
    padding: 12,
    border: '2px solid rgba(255,255,255,0.15)', borderRadius: 10,
    background: 'var(--bg-primary)', color: 'var(--text-bright)',
    fontSize: 18, minHeight: 'var(--touch-min)',
  };

  return (
    <div
      onClick={(e) => e.target === e.currentTarget && onClose()}
      style={{
        position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
        background: 'rgba(0,0,0,0.7)',
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        zIndex: 200,
      }}
    >
      <div style={{
        background: 'var(--bg-secondary)', borderRadius: 16, padding: 24,
        width: '90%', maxWidth: 400,
      }}>
        <h3 style={{ marginBottom: 16, fontSize: 18, color: 'var(--text-bright)' }}>Add Cut</h3>

        <label style={{ display: 'block', marginBottom: 12, fontSize: 14, color: 'var(--text-dim)' }}>
          Label
          <input type="text" placeholder="e.g. Shelf Side A" value={label}
            onChange={(e) => setLabel(e.target.value)} style={inputStyle} />
        </label>

        <label style={{ display: 'block', marginBottom: 12, fontSize: 14, color: 'var(--text-dim)' }}>
          Length (mm)
          <input type="number" step="0.1" min="0" max="1200" value={length}
            onChange={(e) => setLength(e.target.value)} style={inputStyle} />
        </label>

        <label style={{ display: 'block', marginBottom: 12, fontSize: 14, color: 'var(--text-dim)' }}>
          Quantity
          <input type="number" min="1" max="100" value={qty}
            onChange={(e) => setQty(e.target.value)} style={inputStyle} />
        </label>

        <div style={{ display: 'flex', gap: 10, marginTop: 16, justifyContent: 'flex-end' }}>
          <button onClick={onClose} style={{
            padding: '6px 12px', border: '1px solid rgba(255,255,255,0.15)',
            borderRadius: 8, background: 'transparent', color: 'var(--text-dim)',
            fontSize: 12, minHeight: 32,
          }}>
            Cancel
          </button>
          <button onClick={handleSave} style={{
            padding: '12px 24px', borderRadius: 'var(--btn-radius)',
            background: 'var(--blue)', color: 'white',
            fontSize: 18, fontWeight: 700, minHeight: 'var(--touch-min)', minWidth: 100,
          }}>
            Add
          </button>
        </div>
      </div>
    </div>
  );
}
