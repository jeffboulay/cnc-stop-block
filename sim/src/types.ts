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
  | 'ESTOP'

export interface ToolInfo {
  uid: string
  name: string
  kerf_mm: number
}

export interface CutEntry {
  label: string
  length_mm: number
  quantity: number
  completed: boolean
}

export interface SystemStatus {
  state: SystemState
  pos_mm: string
  target_mm: string
  homed: boolean
  locked: boolean
  dust: boolean
  clamps: boolean
  tool: ToolInfo | null
  cutlist_idx: number
  cutlist_size: number
  error?: string
  /** Stable error code — mirrors firmware/src/WebAPI.cpp::buildStatusJSON() */
  error_code?: string
  uptime_s: number
}

// Simulation-only internals not exposed to the UI
export interface SimInternals {
  moveStartPos: number
  moveEndPos: number
  moveStartTime: number
  moveDurationMs: number
  stateEnteredAt: number
}
