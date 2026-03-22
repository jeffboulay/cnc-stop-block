import type { ToolInfo as ToolInfoType } from '../api/types';

export function ToolInfo({ tool }: { tool: ToolInfoType | null }) {
  if (!tool) {
    return <span style={{ fontSize: 14, color: 'var(--text-dim)' }}>No tool detected</span>;
  }

  return (
    <span style={{ fontSize: 14, color: 'var(--text-primary)' }}>
      {tool.name}
      <span style={{ color: 'var(--text-dim)', marginLeft: 8 }}>
        kerf: {tool.kerf_mm}mm
      </span>
    </span>
  );
}
