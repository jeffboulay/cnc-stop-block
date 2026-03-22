export type SystemState =
  | 'BOOT'
  | 'WIFI_CONNECT'
  | 'PROVISIONING'
  | 'NEEDS_HOMING'
  | 'HOMING'
  | 'IDLE'
  | 'UNLOCKING'
  | 'MOVING'
  | 'SETTLING'
  | 'LOCKING'
  | 'LOCKED'
  | 'DUST_SPINUP'
  | 'CUTTING'
  | 'ERROR'
  | 'ESTOP';

export interface ToolInfo {
  uid: string;
  name: string;
  kerf_mm: number;
}

export interface SystemStatus {
  state: SystemState;
  pos_mm: string;
  target_mm: string;
  homed: boolean;
  locked: boolean;
  dust: boolean;
  clamps: boolean;
  tool: ToolInfo | null;
  cutlist_idx: number;
  cutlist_size: number;
  error?: string;
  /** Stable code emitted by firmware alongside the raw error string.
   *  Used by ErrorBanner to display a user-friendly message.
   *  Values: HOMING_FAILED | FAR_LIMIT_TRIGGERED | MOVE_TIMEOUT | POSITION_ERROR | UNKNOWN */
  error_code?: string;
  uptime_s: number;
}

export interface CutEntry {
  label: string;
  length_mm: number;
  quantity: number;
  completed: boolean;
}
