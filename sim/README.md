# CNC Stop Block — Mock ESP32 Simulator

A Node.js development server that simulates the full ESP32 REST API and WebSocket without any hardware. Lets you build and test the React UI before the physical machine exists.

## What it simulates

- **Full state machine** — all 14 states with realistic timing: `NEEDS_HOMING → HOMING → IDLE → MOVING → SETTLING → LOCKING → LOCKED → DUST_SPINUP → CUTTING → ERROR → ESTOP`
- **Realistic motion** — smoothstep-interpolated position, travel time proportional to distance at 50 mm/s
- **Servo timing** — 300ms lock/unlock delay matching the real latch
- **Dust spinup** — 1s delay before transitioning `DUST_SPINUP → CUTTING`
- **Cut list** — full CRUD with same size caps and validation as firmware (`MAX_CUTS = 100`)
- **Tool registry** — RFID tool management (`MAX_TOOLS = 50`)
- **Error/E-Stop injection** — force any error state for UI testing

Every endpoint, HTTP status code, and validation rule mirrors `firmware/src/WebAPI.cpp` exactly.

---

## Quick start

```bash
cd sim
npm install
npm run dev        # starts on :3001, auto-restarts on file change
```

In a second terminal:

```bash
cd ui
VITE_PROXY_TARGET=http://localhost:3001 npm run dev
```

Open the Vite dev URL. The UI connects to the sim over the Vite proxy — no CORS issues, no ESP32 needed.

Seed the sim with sample cuts and a registered tool to get a realistic starting state:

```bash
curl -X POST http://localhost:3001/sim/seed
```

---

## Endpoints

### REST API (`/api/*`)

These match the firmware exactly and are the same endpoints the UI calls in production.

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/status` | System status snapshot |
| `POST` | `/api/home` | Start homing sequence |
| `POST` | `/api/goto` | Move to position `{"position_mm": 150.5}` |
| `POST` | `/api/jog` | Relative jog `{"distance_mm": 1.0}` |
| `POST` | `/api/lock` | Engage latch |
| `POST` | `/api/unlock` | Release latch |
| `POST` | `/api/estop` | Emergency stop |
| `POST` | `/api/reset` | Clear error state |
| `GET` | `/api/cutlist` | Get all cuts |
| `POST` | `/api/cutlist` | Replace entire cut list |
| `POST` | `/api/cutlist/add` | Add a cut |
| `DELETE` | `/api/cutlist/:index` | Remove cut by index |
| `POST` | `/api/cutlist/clear` | Clear all cuts |
| `POST` | `/api/cutlist/reset` | Reset completed status |
| `POST` | `/api/cut/start` | Start cut (dust + clamps) |
| `POST` | `/api/cut/end` | End cut |
| `POST` | `/api/cut/next` | Move to next cut in list |
| `GET` | `/api/tools` | List registered tools |
| `POST` | `/api/tools` | Register/update tool |
| `DELETE` | `/api/tools/:uid` | Remove tool |

### WebSocket

`ws://localhost:3001/ws` — broadcasts JSON status at 10 Hz. Same format as the firmware.

### Simulation controls (`/sim/*`)

These endpoints exist only in the sim. They let you drive the machine into specific states for UI testing.

| Method | Endpoint | Body | Description |
|--------|----------|------|-------------|
| `GET` | `/sim/state` | — | Full sim state dump (for debugging) |
| `POST` | `/sim/seed` | — | Seed with sample cuts + tools, set state to IDLE |
| `POST` | `/sim/reset` | — | Reset everything to initial state |
| `POST` | `/sim/inject/error` | `{"message": "Stall at 150mm"}` | Force ERROR state |
| `POST` | `/sim/inject/estop` | — | Force ESTOP state |
| `POST` | `/sim/inject/tool` | `{"uid": "AABB1122", "name": "Freud 10\" 40T", "kerf_mm": 3.2}` | Simulate RFID tag detected |
| `POST` | `/sim/inject/tool` | `{}` | Simulate RFID tag removed |
| `POST` | `/sim/inject/position` | `{"pos_mm": 250.0}` | Teleport position (no motion) |

---

## Common dev workflows

### Start fresh with sample data

```bash
curl -X POST http://localhost:3001/sim/seed
```

Sets state to `IDLE`, adds 4 sample cuts, registers 2 blades.

### Test the homing flow

```bash
# Start from NEEDS_HOMING (default)
curl -X POST http://localhost:3001/api/home
# Watch state: HOMING → IDLE (takes ~1.2s for 0mm travel)
curl http://localhost:3001/api/status | jq .state
```

### Test a full cut cycle

```bash
curl -X POST http://localhost:3001/sim/seed
curl -X POST http://localhost:3001/api/cut/next     # IDLE → MOVING → SETTLING → LOCKING → LOCKED
curl -X POST http://localhost:3001/api/cut/start    # LOCKED → DUST_SPINUP → CUTTING
curl -X POST http://localhost:3001/api/cut/end      # CUTTING → LOCKED (marks cut completed)
```

### Test error handling

```bash
curl -X POST http://localhost:3001/sim/inject/error \
  -H 'Content-Type: application/json' \
  -d '{"message": "Far limit switch triggered"}'

# UI should show error banner — then reset:
curl -X POST http://localhost:3001/api/reset        # ERROR → NEEDS_HOMING
```

### Test E-Stop

```bash
curl -X POST http://localhost:3001/sim/inject/estop
# UI should flash red — then:
curl -X POST http://localhost:3001/api/reset
```

### Test tool detection

```bash
curl -X POST http://localhost:3001/sim/inject/tool \
  -H 'Content-Type: application/json' \
  -d '{"uid":"AABB1122","name":"Freud 10\" 40T","kerf_mm":3.2}'

# Status now shows tool present, kerf offset applied to next cut
curl -X POST http://localhost:3001/sim/inject/tool \
  -H 'Content-Type: application/json' \
  -d '{}'    # removes tool
```

### Test input validation

```bash
# Should return 400:
curl -X POST http://localhost:3001/api/goto \
  -H 'Content-Type: application/json' \
  -d '{"position_mm": 9999}'

# Should return 413 on large body:
python3 -c "import sys; sys.stdout.write('X' * 20000)" | \
  curl -X POST http://localhost:3001/api/goto \
  -H 'Content-Type: application/json' --data-binary @-
```

---

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SIM_PORT` | `3001` | Port to listen on |

---

## Architecture

```
sim/
├── src/
│   ├── server.ts     # HTTP + WebSocket server, 10Hz tick loop
│   ├── machine.ts    # State machine — mirrors SystemController.cpp
│   ├── api.ts        # Express routes — mirrors WebAPI.cpp
│   └── types.ts      # Shared types (matches ui/src/api/types.ts)
├── package.json
└── tsconfig.json
```

`machine.ts` is intentionally kept in sync with `firmware/src/SystemController.cpp`. When you add a new state or command to the firmware, add it here too.

---

## Wokwi (firmware circuit simulation)

For simulating the firmware itself (rather than the API), use [Wokwi](https://wokwi.com).

1. Install the [Wokwi VS Code extension](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode)
2. Build the firmware: `cd firmware && pio run`
3. Add `firmware/wokwi.toml`:
   ```toml
   [wokwi]
   version = 1
   elf = ".pio/build/esp32dev/firmware.elf"
   firmware = ".pio/build/esp32dev/firmware.bin"
   ```
4. Add `firmware/diagram.json` describing the circuit (stepper, servo, buttons, NeoPixels)
5. Press **F1 → Wokwi: Start Simulator**

Wokwi simulates WiFi — the simulated ESP32 can connect to a virtual access point and serve the API. You can then point the Vite dev server at Wokwi's tunneled URL.

## PlatformIO native unit tests

Logic-only tests (CutList, input validation) that run on macOS/Linux without any hardware or emulator:

```bash
cd firmware
pio test -e native
```

Requires a `[env:native]` section in `platformio.ini` — see firmware README for setup.
