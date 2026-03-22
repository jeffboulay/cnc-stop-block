/**
 * SimMachine — simulates the ESP32 SystemController state machine.
 *
 * Mirrors the state transitions and timing from firmware/src/SystemController.cpp.
 * All durations are approximate but realistic for a GT2 belt drive at 50mm/s.
 */

import type { SystemState, ToolInfo, CutEntry, SystemStatus } from "./types.js";

// Timing constants — mirror config.h exactly. Update here when config.h changes.
const MAX_SPEED_MM_S = 200; // was 50
const HOMING_SPEED_MM_S = 150; // was 10
const ACCELERATION_MM_S2 = 800;
const APPROACH_ZONE_MM = 30;
const APPROACH_SPEED_MM_S = 15;
const SERVO_MOVE_MS = 300;
const DUST_ON_DELAY_MS = 1000;
const SETTLING_MS = 350; // was 200

export class SimMachine {
  // --- Public state (sent to UI) ---
  state: SystemState = "NEEDS_HOMING";
  positionMM = 0;
  targetMM = 0;
  homed = false;
  locked = false;
  dustActive = false;
  clampsEngaged = false;
  activeTool: ToolInfo | null = null;
  error: string | undefined = undefined;
  startTimeMs = Date.now();

  // --- Collections ---
  cutList: CutEntry[] = [];
  toolRegistry: ToolInfo[] = [];

  // --- Internal sim bookkeeping ---
  private stateEnteredAt = Date.now();
  private moveStartPos = 0;
  private moveEndPos = 0;
  private moveStartMs = 0;
  private moveDurationMs = 0; // total duration of phase 1 (cruise)
  private approachStartMs = 0; // when phase 2 (approach) begins
  private approachDurationMs = 0; // duration of phase 2
  private approachStartPos = 0; // position at start of phase 2
  private inApproachZone = false;
  private pendingTargetMM = 0;

  // -------------------------------------------------------------------------
  // Tick — call at 10Hz from the WebSocket loop
  // -------------------------------------------------------------------------
  tick() {
    switch (this.state) {
      case "HOMING":
        this.tickHoming();
        break;
      case "UNLOCKING":
        this.tickUnlocking();
        break;
      case "MOVING":
        this.tickMoving();
        break;
      case "SETTLING":
        this.tickSettling();
        break;
      case "LOCKING":
        this.tickLocking();
        break;
      case "DUST_SPINUP":
        this.tickDustSpinup();
        break;
    }
  }

  // -------------------------------------------------------------------------
  // Commands — mirror SystemController command methods
  // -------------------------------------------------------------------------
  commandHome() {
    if (this.state !== "NEEDS_HOMING" && this.state !== "IDLE") return;
    this._startMove(this.positionMM, 0, HOMING_SPEED_MM_S);
    this._transition("HOMING");
  }

  commandGoTo(positionMM: number) {
    if (this.state !== "IDLE" && this.state !== "LOCKED") return;
    this.pendingTargetMM = positionMM;
    this.targetMM = positionMM;

    if (this.state === "LOCKED") {
      this.locked = false;
      this._transition("UNLOCKING");
    } else {
      this._startMove(this.positionMM, positionMM, MAX_SPEED_MM_S);
      this._transition("MOVING");
    }
  }

  commandJog(distanceMM: number) {
    if (this.state !== "IDLE") return;
    const dest = Math.max(0, Math.min(1200, this.positionMM + distanceMM));
    this.pendingTargetMM = dest;
    this.targetMM = dest;
    this._startMove(this.positionMM, dest, MAX_SPEED_MM_S);
    this._transition("MOVING");
  }

  commandLock() {
    if (this.state !== "IDLE") return;
    this._transition("LOCKING");
  }

  commandUnlock() {
    if (this.state !== "LOCKED") return;
    this.pendingTargetMM = 0;
    this.locked = false;
    this._transition("UNLOCKING");
  }

  commandEStop() {
    this.dustActive = false;
    this.clampsEngaged = false;
    this._transition("ESTOP");
  }

  commandResetError() {
    if (this.state !== "ERROR" && this.state !== "ESTOP") return;
    this.error = undefined;
    this.homed = false;
    this._transition("NEEDS_HOMING");
  }

  commandStartCut() {
    if (this.state !== "LOCKED") return;
    this.dustActive = true;
    this.clampsEngaged = true;
    this._transition("DUST_SPINUP");
  }

  commandEndCut() {
    if (this.state !== "CUTTING") return;
    this.dustActive = false;
    this.clampsEngaged = false;
    const idx = this._nextCutIndex();
    if (idx >= 0) this.cutList[idx]!.completed = true;
    this._transition("LOCKED");
  }

  commandNextCut() {
    const idx = this._nextCutIndex();
    if (idx < 0) return;
    const cut = this.cutList[idx]!;
    let targetMM = cut.length_mm;
    if (this.activeTool) targetMM += this.activeTool.kerf_mm;
    this.commandGoTo(Math.min(targetMM, 1200));
  }

  // --- Error / tool injection (sim-only) ---
  injectError(message: string) {
    this.error = message;
    this._transition("ERROR");
  }

  injectTool(tool: ToolInfo | null) {
    this.activeTool = tool;
  }

  // -------------------------------------------------------------------------
  // Cut list
  // -------------------------------------------------------------------------
  addCut(entry: CutEntry) {
    this.cutList.push(entry);
  }

  removeCut(index: number) {
    this.cutList.splice(index, 1);
  }

  clearCuts() {
    this.cutList = [];
  }

  resetCuts() {
    this.cutList = this.cutList.map((c) => ({ ...c, completed: false }));
  }

  replaceCuts(entries: CutEntry[]) {
    this.cutList = entries;
  }

  // -------------------------------------------------------------------------
  // Tool registry
  // -------------------------------------------------------------------------
  registerTool(tool: ToolInfo) {
    const idx = this.toolRegistry.findIndex((t) => t.uid === tool.uid);
    if (idx >= 0) {
      this.toolRegistry[idx] = tool;
    } else {
      this.toolRegistry.push(tool);
    }
  }

  removeTool(uid: string) {
    this.toolRegistry = this.toolRegistry.filter((t) => t.uid !== uid);
    if (this.activeTool?.uid === uid) this.activeTool = null;
  }

  // -------------------------------------------------------------------------
  // Status snapshot (sent to UI over WebSocket and REST)
  // -------------------------------------------------------------------------
  getStatus(): SystemStatus {
    const nextIdx = this._nextCutIndex();
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
    };
    if (this.state === "ERROR" && this.error) {
      status.error = this.error;
      // Mirror the prefix-matching logic in firmware/src/WebAPI.cpp::buildStatusJSON()
      if      (this.error.startsWith("Homing failed"))  status.error_code = "HOMING_FAILED";
      else if (this.error.startsWith("Far limit"))      status.error_code = "FAR_LIMIT_TRIGGERED";
      else if (this.error.startsWith("Move timeout"))   status.error_code = "MOVE_TIMEOUT";
      else if (this.error.startsWith("Position error")) status.error_code = "POSITION_ERROR";
      else                                               status.error_code = "UNKNOWN";
    }
    return status;
  }

  // -------------------------------------------------------------------------
  // Private — state tick handlers
  // -------------------------------------------------------------------------

  private tickHoming() {
    this.positionMM = this._interpolateMove();
    if (this._isMoveComplete()) {
      this.positionMM = 0;
      this.targetMM = 0;
      this.homed = true;
      this._transition("IDLE");
    }
  }

  private tickUnlocking() {
    if (Date.now() - this.stateEnteredAt >= SERVO_MOVE_MS) {
      if (this.pendingTargetMM > 0) {
        this._startMove(this.positionMM, this.pendingTargetMM, MAX_SPEED_MM_S);
        this._transition("MOVING");
      } else {
        this._transition("IDLE");
      }
    }
  }

  private tickMoving() {
    this.positionMM = this._interpolateMove();
    if (this._isMoveComplete()) {
      this.positionMM = this.moveEndPos;
      this._transition("SETTLING");
    }
  }

  private tickSettling() {
    if (Date.now() - this.stateEnteredAt >= SETTLING_MS) {
      this._transition("LOCKING");
    }
  }

  private tickLocking() {
    if (Date.now() - this.stateEnteredAt >= SERVO_MOVE_MS) {
      this.locked = true;
      this._transition("LOCKED");
    }
  }

  private tickDustSpinup() {
    if (Date.now() - this.stateEnteredAt >= DUST_ON_DELAY_MS) {
      this._transition("CUTTING");
    }
  }

  // -------------------------------------------------------------------------
  // Private — helpers
  // -------------------------------------------------------------------------

  private _transition(newState: SystemState) {
    console.log(`[SIM] ${this.state} → ${newState}`);
    this.state = newState;
    this.stateEnteredAt = Date.now();
  }

  /**
   * Start a two-phase move that mirrors the firmware approach zone behaviour:
   *
   * Phase 1 — Cruise: move from fromMM to (toMM - APPROACH_ZONE_MM) at
   *           the given cruiseSpeed using a trapezoidal profile.
   *           For short moves (< APPROACH_ZONE_MM) this phase has zero length.
   *
   * Phase 2 — Approach: cover the final APPROACH_ZONE_MM at APPROACH_SPEED_MM_S
   *           for accurate final positioning.
   */
  private _startMove(fromMM: number, toMM: number, cruiseSpeed: number) {
    const totalDist = Math.abs(toMM - fromMM);
    const dir = toMM >= fromMM ? 1 : -1;

    this.moveStartPos = fromMM;
    this.moveEndPos = toMM;
    this.moveStartMs = Date.now();
    this.inApproachZone = false;

    const approachDist = Math.min(APPROACH_ZONE_MM, totalDist);
    const cruiseDist = totalDist - approachDist;
    const approachStartPosMM = toMM - dir * approachDist;

    // Phase 1: trapezoidal cruise duration
    if (cruiseDist > 0) {
      const vMax = Math.min(
        cruiseSpeed,
        Math.sqrt(cruiseDist * ACCELERATION_MM_S2),
      );
      const tRamp = vMax / ACCELERATION_MM_S2;
      const dRamp = 0.5 * ACCELERATION_MM_S2 * tRamp * tRamp;
      const dCruise = cruiseDist - 2 * dRamp;
      const tCruise = dCruise > 0 ? dCruise / vMax : 0;
      this.moveDurationMs = Math.round((2 * tRamp + tCruise) * 1000);
    } else {
      this.moveDurationMs = 0;
    }

    // Phase 2: approach duration (triangle profile at low speed)
    const vApproach = Math.min(
      APPROACH_SPEED_MM_S,
      Math.sqrt(approachDist * ACCELERATION_MM_S2),
    );
    const tApproachRamp = vApproach / ACCELERATION_MM_S2;
    const dApproachRamp =
      0.5 * ACCELERATION_MM_S2 * tApproachRamp * tApproachRamp;
    const dApproachCruise = approachDist - 2 * dApproachRamp;
    const tApproachCruise =
      dApproachCruise > 0 ? dApproachCruise / vApproach : 0;
    this.approachDurationMs = Math.round(
      (2 * tApproachRamp + tApproachCruise) * 1000,
    );
    this.approachStartPos = approachStartPosMM;

    console.log(
      `[SIM] Move ${fromMM.toFixed(1)}→${toMM.toFixed(1)}mm | ` +
        `cruise ${(this.moveDurationMs / 1000).toFixed(2)}s | ` +
        `approach ${(this.approachDurationMs / 1000).toFixed(2)}s`,
    );
  }

  private _interpolateMove(): number {
    const now = Date.now();
    const cruiseElapsed = now - this.moveStartMs;

    // Still in cruise phase
    if (!this.inApproachZone) {
      if (this.moveDurationMs > 0 && cruiseElapsed < this.moveDurationMs) {
        const t = cruiseElapsed / this.moveDurationMs;
        const ease = t * t * (3 - 2 * t); // smoothstep ≈ trapezoid
        return (
          this.moveStartPos + (this.approachStartPos - this.moveStartPos) * ease
        );
      }
      // Transition to approach zone
      this.inApproachZone = true;
      this.approachStartMs = now;
      console.log("[SIM] → approach zone");
    }

    // Approach phase
    const approachElapsed = now - this.approachStartMs;
    const t = Math.min(approachElapsed / this.approachDurationMs, 1);
    const ease = t * t * (3 - 2 * t);
    return (
      this.approachStartPos + (this.moveEndPos - this.approachStartPos) * ease
    );
  }

  private _isMoveComplete(): boolean {
    if (!this.inApproachZone) return false;
    return Date.now() - this.approachStartMs >= this.approachDurationMs;
  }

  private _nextCutIndex(): number {
    return this.cutList.findIndex((c) => !c.completed);
  }
}
