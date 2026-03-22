# CNC Stop Block for Miter Saw Station

## Context
Build a CNC-controlled stop block system for a miter saw station. The stop block positions itself along the fence to set repeatable cut lengths, locks in place, and integrates dust collection, RFID tool detection, pneumatic clamps, and a web-based UI. An old **Ender 3 3D printer** is being repurposed for mechanical parts (stepper, belt, wheels, extrusion, endstops, PSU).

## Hardware Platform
- **MCU**: ESP32 DevKit V1
- **Framework**: Arduino C++ via PlatformIO
- **Motion**: NEMA 17 stepper + GT2 belt (from Ender 3)
- **Stepper driver**: TMC2209 or A4988 (from Ender 3 mainboard)
- **Linear motion**: V-slot extrusion + V-wheels (from Ender 3)
- **Endstops**: Ender 3 mechanical endstops (homing + far-limit)
- **PSU**: Ender 3 24V PSU (with 5V buck converter for ESP32/servo)
- **DRO**: Abstracted — start with open-loop step counting, swap in encoder later
- **Control UI**: iPad Pro via React PWA (touch-optimized, served separately from firmware)

## Ender 3 Parts Reuse
| Ender 3 Part | Use In This Project |
|---|---|
| NEMA 17 stepper (X or Y axis) | Belt-drive motor for stop block |
| GT2 belt + 20T pulleys | Linear motion drive |
| 2040 V-slot extrusion | Fence rail / stop block track |
| V-slot wheels + eccentric nuts | Stop block carriage |
| Mechanical endstops (2x) | Home + far-end limit switches |
| 24V PSU | Main power supply |
| TMC2208/2209 driver (if 32-bit board) | Stepper driver module |
| Wiring harness / connectors | JST-XH connectors for clean wiring |

---

## Architecture

```
┌─────────────┐     REST + WebSocket     ┌─────────────────┐
│   iPad Pro  │ ◄──────────────────────► │   ESP32 (API)   │
│  React PWA  │       WiFi               │  Motor Control  │
│  (ui/)      │                          │  (firmware/)    │
└─────────────┘                          └─────────────────┘
         ▲
         │ (dev only)
┌────────┴────────┐
│  Mock ESP32 Sim │
│  (sim/)         │
└─────────────────┘
```

The ESP32 exposes a headless JSON REST API and WebSocket. No static files are served from the device. The React UI runs independently on any host (dev server, Raspberry Pi, NAS, iPad browser) and communicates with the firmware over WiFi. During development, a Node.js mock server (`sim/`) simulates the full firmware without hardware.

---

## Project Structure
```
cnc-stop-block/
├── firmware/                    # ESP32 PlatformIO project
│   ├── platformio.ini
│   ├── include/
│   │   ├── config.h             # All tuning params — speeds, zones, timeouts
│   │   └── pins.h               # All GPIO pin assignments
│   └── src/
│       ├── main.cpp             # setup() + loop() + watchdog
│       ├── SystemController.*   # Central state machine
│       ├── StepperMotion.*      # FastAccelStepper wrapper + approach zone
│       ├── DROInterface.h       # Abstract position feedback (pure virtual)
│       ├── DROStepperOnly.*     # Fallback: open-loop step counting
│       ├── ServoLatch.*         # Servo-controlled magnet lock
│       ├── RFIDReader.*         # MFRC522 tool detection + registry
│       ├── DustCollection.*     # Relay control with spin-up/down delays
│       ├── IndicatorLights.*    # NeoPixel status patterns
│       ├── PneumaticClamps.*    # Solenoid valve control
│       ├── ButtonPanel.*        # Debounced physical buttons
│       ├── WebAPI.*             # REST API + WebSocket (no static files)
│       └── CutList.*            # Cut list persistence (LittleFS)
├── ui/                          # React + Vite + TypeScript + TanStack Query
│   ├── src/
│   │   ├── api/                 # client.ts, websocket.ts, types.ts
│   │   ├── hooks/               # useStatus, useCommands, useCutList, useTools
│   │   ├── components/          # DRODisplay, JogControls, GoToInput, CutList, …
│   │   └── layouts/             # MainLayout
│   └── vite.config.ts           # Dev proxy → ESP32 or sim
├── sim/                         # Mock ESP32 server (dev without hardware)
│   └── src/
│       ├── server.ts            # HTTP + WebSocket, 10Hz tick
│       ├── machine.ts           # State machine (mirrors SystemController.cpp)
│       ├── api.ts               # Express routes (mirrors WebAPI.cpp)
│       └── types.ts
├── docs/
│   ├── PLAN.md                  # This file — living implementation plan
│   ├── SECURITY.md              # Security review and status
│   └── SECURITY-PLAN.md         # Security remediation roadmap
└── .claude/
    └── launch.json              # Dev server configs for Claude Code preview
```

---

## Key Libraries (platformio.ini)
- `FastAccelStepper` — hardware-timer step generation (won't stutter under web server load)
- `ESP32Servo` — servo control via LEDC
- `MFRC522` — RFID reader
- `ESPAsyncWebServer` + `AsyncTCP` — non-blocking web server
- `Adafruit NeoPixel` — LED status indicators
- `ArduinoJson` — JSON serialization

---

## Motion Profile

The stop block uses a **two-phase velocity profile** for every move:

### Phase 1 — Cruise
FastAccelStepper generates a trapezoidal velocity profile automatically:
- **Long moves**: accelerate → cruise at `MAX_SPEED_MM_S` → begin deceleration
- **Short moves**: triangle profile — never reaches max speed. Peak speed scales as
  `v_peak = min(MAX_SPEED, √(distance × ACCELERATION))` so a 5mm jog is
  proportionally slower than a 500mm traverse, with no special handling needed.

### Phase 2 — Approach Zone
When the block enters `APPROACH_ZONE_MM` of the target, `StepperMotion::update()`
drops speed to `APPROACH_SPEED_MM_S` via `applySpeedAcceleration()`. FastAccelStepper
replans the remaining distance at the lower speed, ensuring the block always arrives
slowly regardless of the total move distance.

```
Velocity
  │        ╭──────────────────╮
  │       ╱                    ╲         ──
  │      ╱  cruise @ MAX_SPEED  ╲   approach @ APPROACH_SPEED
  │─────╱──────────────────────────────────────╲──────────────
  │
  └──────────────────────────────────────────────────► Position
                                         ▲
                                 APPROACH_ZONE_MM from target
```

### Current Parameters (`firmware/include/config.h`)

| Parameter | Value | Notes |
|---|---|---|
| `MAX_SPEED_MM_S` | 200 mm/s | ~300 RPM on GT2/20T — well within NEMA 17 torque band |
| `HOMING_SPEED_MM_S` | 15 mm/s | Conservative for reliable switch detection |
| `ACCELERATION_MM_S2` | 800 mm/s² | Tuned for light carriage, no missed steps |
| `APPROACH_ZONE_MM` | 30 mm | Must be ≥ `MAX_SPEED² / (2 × ACCEL)` ≈ 25 mm |
| `APPROACH_SPEED_MM_S` | 15 mm/s | Final positioning speed |
| `SETTLING_MS` | 350 ms | Time after move before latch engages |

### Tuning Ladder
Increase speed one step at a time. Run 20+ repeated cycles and check for position
drift before advancing.

| Step | MAX_SPEED | ACCEL | APPROACH_ZONE |
|---|---|---|---|
| Conservative (original) | 50 mm/s | 200 | 10 mm |
| **Moderate (current)** | **200 mm/s** | **800** | **30 mm** |
| Aggressive | 350 mm/s | 1200 | 55 mm |

---

## State Machine
```
BOOT → WIFI_CONNECT → NEEDS_HOMING → HOMING → IDLE
IDLE → UNLOCKING → MOVING → SETTLING → LOCKING → LOCKED
LOCKED → DUST_SPINUP → CUTTING → LOCKED
LOCKED → UNLOCKING (new position)
ANY → ESTOP (hardware interrupt, requires manual reset)
ANY → ERROR (stall, timeout, position mismatch)
```

---

## Safety Design
- **Hardware E-Stop**: NC switch in series with stepper driver ENABLE (works even if ESP32 crashes)
- **Limit switches**: NC wiring (broken wire = triggered = fail-safe)
- **Watchdog timer**: `esp_task_wdt_init()` with 10s timeout — reboots on firmware stall
- **Firmware**: position validation, state timeouts, motion lockout during ESTOP/ERROR
- **Approach zone**: block always arrives at target slowly; latch engages after full settle

---

## Development Without Hardware

```bash
# Terminal 1: mock ESP32
cd sim && npm install && npm run dev

# Terminal 2: React UI proxied to sim
cd ui && VITE_PROXY_TARGET=http://localhost:3001 npm run dev

# Seed with sample data
curl -X POST http://localhost:3001/sim/seed
```

Simulation controls available at `http://localhost:3001/sim/*` — see `sim/README.md`.

### Claude Code dev server (preview)
`.claude/launch.json` defines both servers. Use Claude Code's preview panel to start them without manual terminal commands.

---

## Implementation Phases

### Phase 1 — Skeleton & Motion ✅
1. `platformio.ini`, `pins.h`, `config.h`
2. `StepperMotion` with FastAccelStepper, approach zone, speed profile
3. `DROStepperOnly` (open-loop fallback)
4. `ButtonPanel` (Home, Go, E-Stop)
5. `SystemController` state machine
6. `main.cpp` with watchdog timer

### Phase 2 — Latch & Indicators ✅
7. `ServoLatch`
8. UNLOCKING, SETTLING, LOCKING, LOCKED states
9. `IndicatorLights` (NeoPixel patterns per state)
10. Limit switch handling + timeout guards

### Phase 3 — API & React UI ✅
11. `WebAPI` (REST + WebSocket, CORS, security headers, body size caps)
12. React PWA (`ui/`) — TanStack Query, WebSocket real-time updates, touch-optimized
13. `CutList` with LittleFS persistence
14. Mock ESP32 sim (`sim/`) for hardware-free development

### Phase 4 — Peripherals ✅
15. `RFIDReader` with tool registry (kerf offset, LittleFS JSON)
16. `DustCollection` (relay + delays)
17. `PneumaticClamps` (solenoid)
18. DUST_SPINUP and CUTTING states

### Phase 5 — DRO & Polish (pending hardware)
19. Closed-loop DRO (rotary encoder or linear scale)
20. Closed-loop position verification in SETTLING state
21. TMC2209 UART mode (StealthChop + StallGuard for missed-step detection)
22. API key authentication (see `SECURITY.md`)
23. WiFi credential provisioning portal (see `SECURITY.md`)

## Verification Checklist
- [ ] **Phase 1**: Stepper moves to target via serial, homes correctly
- [ ] **Phase 2**: Servo locks/unlocks, LEDs reflect state, E-stop cuts power
- [ ] **Phase 3**: React UI connects over WiFi, DRO updates in real time via WS
- [ ] **Phase 4**: RFID detects tags, dust relay toggles, clamp solenoid fires
- [ ] **Phase 5**: DRO matches stepper position within 0.05mm tolerance after 20 cycles
