/**
 * Express router — mirrors all endpoints in firmware/src/WebAPI.cpp.
 *
 * Route signatures, validation rules, and HTTP status codes match the
 * firmware exactly so the UI behaves identically against the sim and
 * the real device.
 */

import { Router } from "express";
import type { SimMachine } from "./machine.js";
import type { CutEntry, ToolInfo } from "./types.js";

const MAX_TRAVEL_MM = 2000;
const MAX_STRING_LENGTH = 64;
const MAX_CUTS = 100;
const MAX_TOOLS = 50;

function isValidFloat(v: unknown): v is number {
  return typeof v === "number" && isFinite(v) && !isNaN(v);
}

function truncate(s: string, max: number) {
  return s.slice(0, max);
}

export function buildRouter(sim: SimMachine): Router {
  const router = Router();

  // --------------------------------------------------------------------------
  // Status
  // --------------------------------------------------------------------------
  router.get("/api/status", (_req, res) => {
    res.json(sim.getStatus());
  });

  // --------------------------------------------------------------------------
  // Commands
  // --------------------------------------------------------------------------
  router.post("/api/home", (_req, res) => {
    sim.commandHome();
    res.json({ ok: true });
  });

  router.post("/api/estop", (_req, res) => {
    sim.commandEStop();
    res.json({ ok: true });
  });

  router.post("/api/reset", (_req, res) => {
    sim.commandResetError();
    res.json({ ok: true });
  });

  router.post("/api/lock", (_req, res) => {
    sim.commandLock();
    res.json({ ok: true });
  });

  router.post("/api/unlock", (_req, res) => {
    sim.commandUnlock();
    res.json({ ok: true });
  });

  router.post("/api/cut/start", (_req, res) => {
    sim.commandStartCut();
    res.json({ ok: true });
  });

  router.post("/api/cut/end", (_req, res) => {
    sim.commandEndCut();
    res.json({ ok: true });
  });

  router.post("/api/cut/next", (_req, res) => {
    sim.commandNextCut();
    res.json({ ok: true });
  });

  router.post("/api/goto", (req, res) => {
    const pos = req.body?.position_mm;
    if (!isValidFloat(pos) || pos < 0 || pos > MAX_TRAVEL_MM) {
      res
        .status(400)
        .json({ error: `position_mm out of range (0 - ${MAX_TRAVEL_MM})` });
      return;
    }
    sim.commandGoTo(pos);
    res.json({ ok: true });
  });

  router.post("/api/jog", (req, res) => {
    const dist = req.body?.distance_mm;
    if (!isValidFloat(dist) || dist < -MAX_TRAVEL_MM || dist > MAX_TRAVEL_MM) {
      res.status(400).json({ error: "distance_mm out of range" });
      return;
    }
    sim.commandJog(dist);
    res.json({ ok: true });
  });

  // --------------------------------------------------------------------------
  // Cut list
  // --------------------------------------------------------------------------
  router.get("/api/cutlist", (_req, res) => {
    res.json(sim.cutList);
  });

  router.post("/api/cutlist", (req, res) => {
    if (!Array.isArray(req.body)) {
      res.status(400).json({ error: "Body must be an array of cut entries" });
      return;
    }
    sim.replaceCuts(req.body as CutEntry[]);
    res.json({ ok: true });
  });

  router.post("/api/cutlist/add", (req, res) => {
    if (sim.cutList.length >= MAX_CUTS) {
      res.status(409).json({ error: `Cut list full (max ${MAX_CUTS})` });
      return;
    }
    const lengthMM = req.body?.length_mm;
    if (!isValidFloat(lengthMM) || lengthMM <= 0 || lengthMM > MAX_TRAVEL_MM) {
      res
        .status(400)
        .json({ error: `length_mm out of range (0 - ${MAX_TRAVEL_MM})` });
      return;
    }
    const qty = req.body?.quantity ?? 1;
    if (!Number.isInteger(qty) || qty < 1 || qty > 999) {
      res.status(400).json({ error: "quantity out of range (1 - 999)" });
      return;
    }
    sim.addCut({
      label: truncate(String(req.body?.label ?? "Untitled"), MAX_STRING_LENGTH),
      length_mm: lengthMM,
      quantity: qty,
      completed: false,
    });
    res.json({ ok: true });
  });

  router.post("/api/cutlist/clear", (_req, res) => {
    if (sim.state === "CUTTING") {
      res
        .status(409)
        .json({ error: "Cannot modify cut list during active cut" });
      return;
    }
    sim.clearCuts();
    res.json({ ok: true });
  });

  router.post("/api/cutlist/reset", (_req, res) => {
    sim.resetCuts();
    res.json({ ok: true });
  });

  router.delete("/api/cutlist/:index", (req, res) => {
    if (sim.state === "CUTTING") {
      res
        .status(409)
        .json({ error: "Cannot modify cut list during active cut" });
      return;
    }
    const index = parseInt(req.params.index ?? "", 10);
    if (isNaN(index) || index < 0 || index >= sim.cutList.length) {
      res.status(404).json({ error: "Cut index out of range" });
      return;
    }
    sim.removeCut(index);
    res.json({ ok: true });
  });

  // --------------------------------------------------------------------------
  // Tool registry
  // --------------------------------------------------------------------------
  router.get("/api/tools", (_req, res) => {
    res.json(sim.toolRegistry);
  });

  router.post("/api/tools", (req, res) => {
    const uid = req.body?.uid;
    const name = req.body?.name;
    if (
      typeof uid !== "string" ||
      uid.length === 0 ||
      uid.length > MAX_STRING_LENGTH
    ) {
      res
        .status(400)
        .json({ error: `uid must be 1-${MAX_STRING_LENGTH} characters` });
      return;
    }
    if (typeof name !== "string" || name.length === 0) {
      res.status(400).json({ error: "name is required" });
      return;
    }
    const isUpdate = sim.toolRegistry.some((t) => t.uid === uid);
    if (!isUpdate && sim.toolRegistry.length >= MAX_TOOLS) {
      res.status(409).json({ error: `Tool registry full (max ${MAX_TOOLS})` });
      return;
    }
    const kerfMM = req.body?.kerf_mm ?? 0;
    if (!isValidFloat(kerfMM) || kerfMM < 0 || kerfMM > 20) {
      res.status(400).json({ error: "kerf_mm out of range (0 - 20)" });
      return;
    }
    const tool: ToolInfo = {
      uid,
      name: truncate(name, MAX_STRING_LENGTH),
      kerf_mm: kerfMM,
    };
    sim.registerTool(tool);
    res.json({ ok: true });
  });

  router.delete("/api/tools/:uid", (req, res) => {
    const uid = (req.params.uid ?? "").toUpperCase();
    sim.removeTool(uid);
    res.json({ ok: true });
  });

  // --------------------------------------------------------------------------
  // Simulation control endpoints (not present on real device)
  // Prefixed with /sim/ to make them obviously sim-only.
  // --------------------------------------------------------------------------

  /**
   * GET /sim/state — full sim internals for debugging
   */
  router.get("/sim/state", (_req, res) => {
    res.json({
      ...sim.getStatus(),
      toolRegistry: sim.toolRegistry,
      cutList: sim.cutList,
    });
  });

  /**
   * POST /sim/inject/error
   * Body: { "message": "Stall detected at 150mm" }
   * Drives the machine into ERROR state with a custom message.
   */
  router.post("/sim/inject/error", (req, res) => {
    const message = String(req.body?.message ?? "Simulated error");
    sim.injectError(message);
    res.json({ ok: true, injected: "ERROR", message });
  });

  /**
   * POST /sim/inject/estop
   * Immediately triggers E-Stop.
   */
  router.post("/sim/inject/estop", (_req, res) => {
    sim.commandEStop();
    res.json({ ok: true, injected: "ESTOP" });
  });

  /**
   * POST /sim/inject/tool
   * Body: { "uid": "AABB1122", "name": "Freud 10\" 40T", "kerf_mm": 3.2 }
   * Simulates an RFID tag detection. Send null body to remove tool.
   */
  router.post("/sim/inject/tool", (req, res) => {
    if (req.body === null || Object.keys(req.body).length === 0) {
      sim.injectTool(null);
      res.json({ ok: true, injected: "TOOL_REMOVED" });
      return;
    }
    const { uid, name, kerf_mm } = req.body as Partial<ToolInfo>;
    if (!uid || !name) {
      res.status(400).json({ error: "uid and name required" });
      return;
    }
    const tool: ToolInfo = { uid, name, kerf_mm: kerf_mm ?? 0 };
    sim.injectTool(tool);
    res.json({ ok: true, injected: "TOOL_PRESENT", tool });
  });

  /**
   * POST /sim/inject/position
   * Body: { "pos_mm": 250.0 }
   * Teleports position without motion (useful for testing DRO display).
   */
  router.post("/sim/inject/position", (req, res) => {
    const pos = req.body?.pos_mm;
    if (!isValidFloat(pos) || pos < 0 || pos > MAX_TRAVEL_MM) {
      res
        .status(400)
        .json({ error: `pos_mm out of range (0 - ${MAX_TRAVEL_MM})` });
      return;
    }
    sim.positionMM = pos;
    res.json({ ok: true, injected: "POSITION", pos_mm: pos });
  });

  /**
   * POST /sim/reset
   * Resets the entire sim to its initial state.
   */
  router.post("/sim/reset", (_req, res) => {
    sim.state = "NEEDS_HOMING";
    sim.positionMM = 0;
    sim.targetMM = 0;
    sim.homed = false;
    sim.locked = false;
    sim.dustActive = false;
    sim.clampsEngaged = false;
    sim.activeTool = null;
    sim.error = undefined;
    sim.cutList = [];
    sim.startTimeMs = Date.now();
    res.json({ ok: true, injected: "RESET" });
  });

  /**
   * POST /sim/seed
   * Seeds the sim with sample data — useful for UI development.
   * Adds a few example cuts and a registered tool.
   */
  router.post("/sim/seed", (_req, res) => {
    sim.cutList = [
      { label: "Shelf Side A", length_mm: 600, quantity: 2, completed: false },
      { label: "Shelf Side B", length_mm: 400, quantity: 2, completed: false },
      { label: "Back Panel", length_mm: 596, quantity: 1, completed: true },
      { label: "Stretcher", length_mm: 350, quantity: 4, completed: false },
    ];
    sim.toolRegistry = [
      { uid: "AABB1122", name: 'Freud 10" 40T General', kerf_mm: 3.2 },
      { uid: "CCDD3344", name: 'Diablo 10" 60T Finish', kerf_mm: 2.8 },
    ];
    sim.homed = true;
    sim.state = "IDLE";
    res.json({ ok: true, injected: "SEED" });
  });

  return router;
}
