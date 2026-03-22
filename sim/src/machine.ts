/**
 * SimMachine — simulates the ESP32 SystemController state machine.
 *
 * Mirrors the state transitions and timing from firmware/src/SystemController.cpp.
 * All durations are approximate but realistic for a GT2 belt drive at 50mm/s.
 */

import type { SystemState, ToolInfo, CutEntry, SystemStatus } from './types.js'

// Timing constants (ms) — mirror config.h
const MAX_SPEED_MM_S = 50
const HOMING_SPEED_MM_S = 10
const SERVO_MOVE_MS = 300
const DUST_ON_DELAY_MS = 1000
const SETTLING_MS = 200

export class SimMachine {
  // --- Public state (sent to UI) ---
  state: SystemState = 'NEEDS_HOMING'
  positionMM = 0
  targetMM = 0
  homed = false
  locked = false
  dustActive = false
  clampsEngaged = false
  activeTool: ToolInfo | null = null
  error: string | undefined = undefined
  startTimeMs = Date.now()

  // --- Collections ---
  cutList: CutEntry[] = []
  toolRegistry: ToolInfo[] = []

  // --- Internal sim bookkeeping ---
  private stateEnteredAt = Date.now()
  private moveStartPos = 0
  private moveEndPos = 0
  private moveStartMs = 0
  private moveDurationMs = 0
  private pendingTargetMM = 0

  // -------------------------------------------------------------------------
  // Tick — call at 10Hz from the WebSocket loop
  // -------------------------------------------------------------------------
  tick() {
    switch (this.state) {
      case 'HOMING':        this.tickHoming(); break
      case 'UNLOCKING':     this.tickUnlocking(); break
      case 'MOVING':        this.tickMoving(); break
      case 'SETTLING':      this.tickSettling(); break
      case 'LOCKING':       this.tickLocking(); break
      case 'DUST_SPINUP':   this.tickDustSpinup(); break
    }
  }

  // -------------------------------------------------------------------------
  // Commands — mirror SystemController command methods
  // -------------------------------------------------------------------------
  commandHome() {
    if (this.state !== 'NEEDS_HOMING' && this.state !== 'IDLE') return
    this._startMove(this.positionMM, 0, HOMING_SPEED_MM_S)
    this._transition('HOMING')
  }

  commandGoTo(positionMM: number) {
    if (this.state !== 'IDLE' && this.state !== 'LOCKED') return
    this.pendingTargetMM = positionMM
    this.targetMM = positionMM

    if (this.state === 'LOCKED') {
      this.locked = false
      this._transition('UNLOCKING')
    } else {
      this._startMove(this.positionMM, positionMM, MAX_SPEED_MM_S)
      this._transition('MOVING')
    }
  }

  commandJog(distanceMM: number) {
    if (this.state !== 'IDLE') return
    const dest = Math.max(0, Math.min(1200, this.positionMM + distanceMM))
    this.pendingTargetMM = dest
    this.targetMM = dest
    this._startMove(this.positionMM, dest, MAX_SPEED_MM_S)
    this._transition('MOVING')
  }

  commandLock() {
    if (this.state !== 'IDLE') return
    this._transition('LOCKING')
  }

  commandUnlock() {
    if (this.state !== 'LOCKED') return
    this.pendingTargetMM = 0
    this.locked = false
    this._transition('UNLOCKING')
  }

  commandEStop() {
    this.dustActive = false
    this.clampsEngaged = false
    this._transition('ESTOP')
  }

  commandResetError() {
    if (this.state !== 'ERROR' && this.state !== 'ESTOP') return
    this.error = undefined
    this.homed = false
    this._transition('NEEDS_HOMING')
  }

  commandStartCut() {
    if (this.state !== 'LOCKED') return
    this.dustActive = true
    this.clampsEngaged = true
    this._transition('DUST_SPINUP')
  }

  commandEndCut() {
    if (this.state !== 'CUTTING') return
    this.dustActive = false
    this.clampsEngaged = false
    const idx = this._nextCutIndex()
    if (idx >= 0) this.cutList[idx]!.completed = true
    this._transition('LOCKED')
  }

  commandNextCut() {
    const idx = this._nextCutIndex()
    if (idx < 0) return
    const cut = this.cutList[idx]!
    let targetMM = cut.length_mm
    if (this.activeTool) targetMM += this.activeTool.kerf_mm
    this.commandGoTo(Math.min(targetMM, 1200))
  }

  // --- Error / tool injection (sim-only) ---
  injectError(message: string) {
    this.error = message
    this._transition('ERROR')
  }

  injectTool(tool: ToolInfo | null) {
    this.activeTool = tool
  }

  // -------------------------------------------------------------------------
  // Cut list
  // -------------------------------------------------------------------------
  addCut(entry: CutEntry) {
    this.cutList.push(entry)
  }

  removeCut(index: number) {
    this.cutList.splice(index, 1)
  }

  clearCuts() {
    this.cutList = []
  }

  resetCuts() {
    this.cutList = this.cutList.map(c => ({ ...c, completed: false }))
  }

  replaceCuts(entries: CutEntry[]) {
    this.cutList = entries
  }

  // -------------------------------------------------------------------------
  // Tool registry
  // -------------------------------------------------------------------------
  registerTool(tool: ToolInfo) {
    const idx = this.toolRegistry.findIndex(t => t.uid === tool.uid)
    if (idx >= 0) {
      this.toolRegistry[idx] = tool
    } else {
      this.toolRegistry.push(tool)
    }
  }

  removeTool(uid: string) {
    this.toolRegistry = this.toolRegistry.filter(t => t.uid !== uid)
    if (this.activeTool?.uid === uid) this.activeTool = null
  }

  // -------------------------------------------------------------------------
  // Status snapshot (sent to UI over WebSocket and REST)
  // -------------------------------------------------------------------------
  getStatus(): SystemStatus {
    const nextIdx = this._nextCutIndex()
    const status: SystemStatus = {
      state: this.state,
      pos_mm: this.positionMM.toFixed(2),
      target_mm: this.targetMM.toFixed(2),
      homed: this.homed,
      locked: this.locked,
      dust: this.dustActive,
      clamps: this.clampsEngaged,
      tool: this.activeTool,
      cutlist_idx: nextIdx,
      cutlist_size: this.cutList.length,
      uptime_s: Math.floor((Date.now() - this.startTimeMs) / 1000),
    }
    if (this.state === 'ERROR' && this.error) status.error = this.error
    return status
  }

  // -------------------------------------------------------------------------
  // Private — state tick handlers
  // -------------------------------------------------------------------------

  private tickHoming() {
    this.positionMM = this._interpolateMove()
    if (this._isMoveComplete()) {
      this.positionMM = 0
      this.targetMM = 0
      this.homed = true
      this._transition('IDLE')
    }
  }

  private tickUnlocking() {
    if (Date.now() - this.stateEnteredAt >= SERVO_MOVE_MS) {
      if (this.pendingTargetMM > 0) {
        this._startMove(this.positionMM, this.pendingTargetMM, MAX_SPEED_MM_S)
        this._transition('MOVING')
      } else {
        this._transition('IDLE')
      }
    }
  }

  private tickMoving() {
    this.positionMM = this._interpolateMove()
    if (this._isMoveComplete()) {
      this.positionMM = this.moveEndPos
      this._transition('SETTLING')
    }
  }

  private tickSettling() {
    if (Date.now() - this.stateEnteredAt >= SETTLING_MS) {
      this._transition('LOCKING')
    }
  }

  private tickLocking() {
    if (Date.now() - this.stateEnteredAt >= SERVO_MOVE_MS) {
      this.locked = true
      this._transition('LOCKED')
    }
  }

  private tickDustSpinup() {
    if (Date.now() - this.stateEnteredAt >= DUST_ON_DELAY_MS) {
      this._transition('CUTTING')
    }
  }

  // -------------------------------------------------------------------------
  // Private — helpers
  // -------------------------------------------------------------------------

  private _transition(newState: SystemState) {
    console.log(`[SIM] ${this.state} → ${newState}`)
    this.state = newState
    this.stateEnteredAt = Date.now()
  }

  private _startMove(fromMM: number, toMM: number, speedMMS: number) {
    this.moveStartPos = fromMM
    this.moveEndPos = toMM
    this.moveStartMs = Date.now()
    const distanceMM = Math.abs(toMM - fromMM)
    // Trapezoidal motion approximation: add 20% for accel/decel ramp
    this.moveDurationMs = distanceMM === 0 ? 50 : Math.round((distanceMM / speedMMS) * 1000 * 1.2)
  }

  private _interpolateMove(): number {
    const elapsed = Date.now() - this.moveStartMs
    const t = Math.min(elapsed / this.moveDurationMs, 1)
    // Smoothstep easing (mimics trapezoidal ramp)
    const ease = t * t * (3 - 2 * t)
    return this.moveStartPos + (this.moveEndPos - this.moveStartPos) * ease
  }

  private _isMoveComplete(): boolean {
    return Date.now() - this.moveStartMs >= this.moveDurationMs
  }

  private _nextCutIndex(): number {
    return this.cutList.findIndex(c => !c.completed)
  }
}
