import { useStatus, useIsConnected } from '../hooks/useStatus';
import { useWebSocket } from '../api/websocket';
import { StatusBadge } from '../components/StatusBadge';
import { ToolInfo } from '../components/ToolInfo';
import { DRODisplay } from '../components/DRODisplay';
import { TargetDisplay } from '../components/TargetDisplay';
import { JogControls } from '../components/JogControls';
import { GoToInput } from '../components/GoToInput';
import { ControlButtons } from '../components/ControlButtons';
import { StatusIndicators } from '../components/StatusIndicators';
import { CutList } from '../components/CutList';
import { ErrorBanner } from '../components/ErrorBanner';

export function MainLayout() {
  useWebSocket();
  const { data: status } = useStatus();
  const { data: connected } = useIsConnected();

  if (!status) {
    return (
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        height: '100%', color: 'var(--text-dim)', fontSize: 18,
      }}>
        Connecting to CNC Stop Block...
      </div>
    );
  }

  const isError = status.state === 'ERROR' || status.state === 'ESTOP';

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100dvh' }}>
      {/* Top Bar */}
      <header style={{
        display: 'flex', alignItems: 'center', gap: 16,
        padding: '8px 16px',
        background: 'var(--bg-secondary)',
        borderBottom: '1px solid rgba(255,255,255,0.1)',
        flexShrink: 0,
      }}>
        <StatusBadge state={status.state} />
        <div style={{ flex: 1 }}>
          <ToolInfo tool={status.tool} />
        </div>
        <span style={{ fontSize: 12, color: connected ? 'var(--green)' : 'var(--text-dim)' }}>
          {connected ? 'Connected' : 'Disconnected'}
        </span>
      </header>

      {/* Main Content */}
      <main style={{ display: 'flex', flex: 1, overflow: 'hidden', gap: 12, padding: 12 }}>
        {/* Left: DRO + Controls */}
        <section style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 10 }}>
          <DRODisplay position={status.pos_mm} />
          <TargetDisplay target={status.target_mm} current={status.pos_mm} />
          <JogControls />
          <GoToInput />
        </section>

        {/* Right: Actions + Cut List */}
        <section style={{ width: 380, display: 'flex', flexDirection: 'column', gap: 10, flexShrink: 0 }}>
          <ControlButtons status={status} />
          <StatusIndicators
            homed={status.homed}
            locked={status.locked}
            dust={status.dust}
            clamps={status.clamps}
          />
          <CutList activeIndex={status.cutlist_idx} />
        </section>
      </main>

      {/* Error Banner */}
      {isError && <ErrorBanner message={status.error ?? status.state} />}
    </div>
  );
}
