# CNC Stop Block

ESP32-powered CNC stop block for a miter saw station. Automatically positions a stop block along the fence for repeatable cut lengths, locks it in place, and integrates dust collection, RFID tool detection, pneumatic clamps, and a touch-optimized web UI controlled from an iPad Pro.

Built using parts salvaged from an **Ender 3 3D printer** (NEMA 17 stepper, GT2 belt/pulleys, V-slot extrusion, V-wheels, endstops, 24V PSU).

## Architecture

```
┌─────────────┐     REST + WebSocket     ┌─────────────────┐
│   iPad Pro  │ ◄──────────────────────► │   ESP32 (API)   │
│  React PWA  │       WiFi               │  Motor Control  │
│  (ui/)      │                          │  (firmware/)    │
└─────────────┘                          └─────────────────┘
```

**Two-project monorepo:**

- `firmware/` — ESP32 PlatformIO project: headless REST API + WebSocket status server
- `ui/` — React + Vite + TypeScript + TanStack Query: touch-optimized iPad Pro PWA

The ESP32 serves no static files — it only exposes a JSON API. The React UI runs independently (dev server, Raspberry Pi, NAS, etc.) and communicates with the ESP32 over WiFi.

## Features

- **Motorized Positioning** — NEMA 17 stepper + GT2 belt drive moves the stop block to a target position
- **Servo-Controlled Latch** — Locks/unlocks the block magnetically at the target position
- **Digital Readout (DRO)** — Abstracted position feedback (open-loop step counting, with support for rotary/linear encoders)
- **RFID Tool Detection** — MFRC522 reader identifies the installed blade and applies kerf offset automatically
- **Dust Collection** — Relay-controlled with configurable spin-up/spin-down delays
- **Pneumatic Clamps** — Solenoid valve control for workpiece clamping
- **NeoPixel Status Lights** — Color-coded patterns for each system state
- **Physical Buttons** — Home, Go, Lock/Unlock, and hardware E-Stop
- **Cut List Management** — Create and step through a list of cuts with quantities, persisted to flash
- **iPad Pro Web UI** — React PWA with TanStack Query, dark theme, real-time WebSocket updates at 10 Hz

## Documentation

- Security review: [docs/SECURITY.md](docs/SECURITY.md)
- Security remediation plan: [docs/SECURITY-PLAN.md](docs/SECURITY-PLAN.md)

## Hardware

### From Ender 3

| Part                           | Use                           |
| ------------------------------ | ----------------------------- |
| NEMA 17 stepper (X or Y axis)  | Belt-drive motor              |
| GT2 belt + 20-tooth pulleys    | Linear motion drive           |
| 2040 V-slot extrusion          | Fence rail / track            |
| V-slot wheels + eccentric nuts | Stop block carriage           |
| Mechanical endstops (x2)       | Home + far-end limit switches |
| 24V PSU                        | Main power supply             |
| TMC2208/2209 driver            | Stepper driver                |

### Additional Parts

| Part                    | Purpose                                 |
| ----------------------- | --------------------------------------- |
| ESP32 DevKit V1         | Microcontroller (WiFi + GPIO)           |
| 5V buck converter       | Power for ESP32 and servo from 24V rail |
| SG90 servo              | Magnet latch actuation                  |
| MFRC522 RFID module     | Tool/blade detection                    |
| NeoPixel strip (8 LEDs) | Status indicators                       |
| Relay module            | Dust collection switching               |
| Solenoid valve          | Pneumatic clamp control                 |
| Momentary buttons (x4)  | Home, Go, Lock, E-Stop                  |

### Wiring

All GPIO assignments are in [`firmware/include/pins.h`](firmware/include/pins.h). Key connections:

| Function                            | GPIO             |
| ----------------------------------- | ---------------- |
| Stepper STEP / DIR / EN             | 25 / 26 / 27     |
| Limit switches (home / far)         | 34 / 35          |
| Servo latch                         | 13               |
| RFID SS / RST                       | 5 / 22           |
| NeoPixel data                       | 4                |
| Dust relay                          | 32               |
| Clamp solenoid                      | 33               |
| Buttons (Go / Home / Lock / E-Stop) | 14 / 12 / 15 / 2 |

Tuning parameters (steps/mm, speeds, delays) are in [`firmware/include/config.h`](firmware/include/config.h).

## Getting Started

### Firmware

Requires [PlatformIO](https://platformio.org/).

On macOS, prefer installing Python via Homebrew and letting PlatformIO use that interpreter instead of the system/Xcode Python. This avoids the `urllib3` `NotOpenSSLWarning` caused by LibreSSL-backed Python builds.

```bash
# Recommended on macOS
brew install python3

cd firmware

# Build
pio run

# Upload to ESP32
pio run -t upload

# Monitor serial output
pio device monitor
```

Notes:

- Run PlatformIO from the `firmware/` directory, where `platformio.ini` lives.
- The firmware currently uses `esp32async/ESPAsyncWebServer` with `esp32async/AsyncTCP`.
- `firmware/platformio.ini` enables `ASYNCWEBSERVER_REGEX`, which is required for the route handlers that use `request->pathArg(...)`.

### Web UI

Requires [Node.js](https://nodejs.org/) (v18+).

```bash
cd ui

# Install dependencies
npm install

# Start dev server (proxies API to ESP32)
# Edit vite.config.ts to set your ESP32's IP address
npm run dev

# Build for production
npm run build
```

Open the dev server URL on your iPad Pro. Tap **Share > Add to Home Screen** for a full-screen kiosk experience.

### First-boot pairing

The ESP32 generates a random 64-character API token on first boot, stores it in flash, and prints it to the serial monitor:

```
[AUTH] *** NEW API TOKEN GENERATED ***
[AUTH] Enter this token in the UI pairing screen:
[AUTH] a3f8c1d2e4b59f7a...  (64 hex chars)
```

Open the UI in a browser — a **Pair with CNC Stop Block** screen appears. Paste the token and tap **Pair Device**. The token is saved in the browser's `localStorage` and used automatically on every subsequent request.

The token persists across reboots (stored in LittleFS). To rotate it:

```bash
curl -X POST http://<esp32-ip>/api/token/rotate \
  -H "Authorization: Bearer <current-token>"
# → {"token":"<new-token>"}
```

After rotation, the UI detects the 401 response on its next request, clears the stored token, and returns to the pairing screen automatically.

## Development Without Hardware

A mock ESP32 server in `sim/` lets you build and test the React UI before the physical machine exists. It runs the full state machine with realistic timing — no ESP32 required.

```bash
# Terminal 1 — start the mock ESP32
cd sim
npm install
npm run dev
```

The sim prints a token on startup:

```
  *** SIM API TOKEN ***
  a3f8c1d2e4b59f7a...
  Enter this in the UI pairing screen (or set SIM_TOKEN env to fix it).
```

```bash
# Terminal 2 — start the UI, pointed at the sim
cd ui
VITE_PROXY_TARGET=http://localhost:3001 npm run dev
```

Open `http://localhost:5173`, paste the token into the pairing screen, and tap **Pair Device**.

Then seed the sim with sample data (no auth needed for `/sim/*` endpoints):

```bash
curl -X POST http://localhost:3001/sim/seed
```

To avoid re-pairing every time the sim restarts, pin the token with an env var:

```bash
SIM_TOKEN=deadbeef0123456789abcdef0123456789abcdef0123456789abcdef01234567 npm run dev
```

### Simulation controls

The sim exposes `/sim/*` endpoints for driving the machine into specific states:

| Endpoint                    | Description                                 |
| --------------------------- | ------------------------------------------- |
| `POST /sim/seed`            | Load sample cuts + tools, set state to IDLE |
| `POST /sim/reset`           | Reset everything to initial state           |
| `POST /sim/inject/error`    | Force ERROR state with custom message       |
| `POST /sim/inject/estop`    | Force ESTOP                                 |
| `POST /sim/inject/tool`     | Simulate RFID tag present/absent            |
| `POST /sim/inject/position` | Teleport position without motion            |
| `GET /sim/state`            | Dump full sim state for debugging           |

See [`sim/README.md`](sim/README.md) for full documentation, common workflows, and Wokwi firmware simulation setup.

## Troubleshooting

### PlatformIO says this is not a project

If `pio run` reports that `platformio.ini` was not found, you are probably in the repo root instead of the firmware project directory.

```bash
cd firmware
pio run
```

### macOS shows `NotOpenSSLWarning`

This means PlatformIO is running under Apple/Xcode Python linked against LibreSSL instead of a Homebrew Python linked against OpenSSL.

Install Homebrew Python:

```bash
brew install python3
```

If PlatformIO was previously installed against Apple/Xcode Python, rebuild its private virtual environment:

```bash
rm -rf ~/.platformio/penv
/opt/homebrew/bin/python3 -m venv ~/.platformio/penv
~/.platformio/penv/bin/python -m pip install -U pip setuptools wheel platformio
```

Verify the new runtime:

```bash
~/.platformio/penv/bin/python -c "import ssl; print(ssl.OPENSSL_VERSION)"
~/.platformio/penv/bin/pio system info
```

### Async web server route errors with `pathArg()`

Newer `ESPAsyncWebServer` releases require regex route support for handlers that call `request->pathArg(...)`.

This project already enables the required flag in `firmware/platformio.ini`:

```ini
build_flags =
    -DASYNCWEBSERVER_REGEX
```

### ArduinoJson `containsKey()` deprecation warnings

ArduinoJson v7 deprecates `containsKey()` in favor of checking the keyed variant directly.

Use one of these patterns instead:

```cpp
if (doc["position_mm"].isNull()) {
    // missing key
}

if (doc["position_mm"].is<float>()) {
    // typed validation
}
```

## API

The ESP32 exposes a REST API. All endpoints return JSON.

### Authentication

All endpoints except `GET /api/status` require a bearer token:

```
Authorization: Bearer <token>
```

The token is generated on first boot and printed to the serial monitor. Rotate it with `POST /api/token/rotate`. The UI stores the token in `localStorage` and sends it automatically after the one-time pairing step.

### Status & Commands

| Method | Endpoint               | Auth | Body                     | Description            |
| ------ | ---------------------- | ---- | ------------------------ | ---------------------- |
| `GET`  | `/api/status`          | —    | —                        | System status snapshot |
| `POST` | `/api/home`            | ✓    | —                        | Start homing sequence  |
| `POST` | `/api/goto`            | ✓    | `{"position_mm": 150.5}` | Move to position       |
| `POST` | `/api/jog`             | ✓    | `{"distance_mm": 1.0}`   | Relative jog           |
| `POST` | `/api/lock`            | ✓    | —                        | Engage latch           |
| `POST` | `/api/unlock`          | ✓    | —                        | Release latch          |
| `POST` | `/api/estop`           | ✓    | —                        | Emergency stop         |
| `POST` | `/api/reset`           | ✓    | —                        | Clear error state      |
| `POST` | `/api/token/rotate`    | ✓    | —                        | Rotate API token       |

### Cut List

| Method   | Endpoint              | Auth | Body                             | Description               |
| -------- | --------------------- | ---- | -------------------------------- | ------------------------- |
| `GET`    | `/api/cutlist`        | ✓    | —                                | Get all cuts              |
| `POST`   | `/api/cutlist`        | ✓    | `[{label, length_mm, quantity}]` | Replace list              |
| `POST`   | `/api/cutlist/add`    | ✓    | `{label, length_mm, quantity}`   | Add cut                   |
| `DELETE` | `/api/cutlist/:index` | ✓    | —                                | Remove cut                |
| `POST`   | `/api/cutlist/clear`  | ✓    | —                                | Clear all                 |
| `POST`   | `/api/cutlist/reset`  | ✓    | —                                | Reset completed status    |
| `POST`   | `/api/cut/start`      | ✓    | —                                | Start cut (dust + clamps) |
| `POST`   | `/api/cut/end`        | ✓    | —                                | End cut                   |
| `POST`   | `/api/cut/next`       | ✓    | —                                | Move to next cut          |

### Tools

| Method   | Endpoint          | Auth | Body                   | Description           |
| -------- | ----------------- | ---- | ---------------------- | --------------------- |
| `GET`    | `/api/tools`      | ✓    | —                      | List registered tools |
| `POST`   | `/api/tools`      | ✓    | `{uid, name, kerf_mm}` | Register tool         |
| `DELETE` | `/api/tools/:uid` | ✓    | —                      | Remove tool           |

### WebSocket

`ws://<esp32-ip>/ws?token=<token>` — pushes JSON status at 10 Hz. The token is required; connections without a valid token are rejected immediately.

## State Machine

```
BOOT → WIFI_CONNECT → NEEDS_HOMING → HOMING → IDLE
IDLE → UNLOCKING → MOVING → SETTLING → LOCKING → LOCKED
LOCKED → DUST_SPINUP → CUTTING → LOCKED
LOCKED → UNLOCKING (new position)
ANY → ESTOP (hardware interrupt)
ANY → ERROR (stall, timeout, position mismatch)
```

## Safety

- **Hardware E-Stop**: Wire a normally-closed switch in series with the stepper driver ENABLE line. This cuts motor power independently of firmware.
- **Limit switches**: Use normally-closed wiring to GND. A broken wire reads as triggered (fail-safe).
- **Firmware guards**: Position validation, state timeouts, motion lockout during ESTOP/ERROR.

## Project Structure

```
cnc-stop-block/
├── sim/                         # Mock ESP32 server (dev without hardware)
│   ├── src/
│   │   ├── server.ts            # HTTP + WebSocket entry point
│   │   ├── machine.ts           # State machine (mirrors SystemController.cpp)
│   │   ├── api.ts               # Express routes (mirrors WebAPI.cpp)
│   │   └── types.ts             # Shared types
│   └── README.md
├── firmware/                    # ESP32 PlatformIO project
│   ├── platformio.ini
│   ├── include/
│   │   ├── config.h             # Tuning parameters
│   │   └── pins.h               # GPIO pin assignments
│   └── src/
│       ├── main.cpp             # Entry point
│       ├── SystemController.*   # Central state machine
│       ├── StepperMotion.*      # FastAccelStepper wrapper
│       ├── DROInterface.h       # Abstract position feedback
│       ├── DROStepperOnly.*     # Open-loop DRO fallback
│       ├── ServoLatch.*         # Servo-controlled magnet lock
│       ├── RFIDReader.*         # MFRC522 tool detection
│       ├── DustCollection.*     # Relay with delays
│       ├── IndicatorLights.*    # NeoPixel status patterns
│       ├── PneumaticClamps.*    # Solenoid valve control
│       ├── ButtonPanel.*        # Debounced buttons
│       ├── WebAPI.*             # REST API + WebSocket
│       └── CutList.*            # Cut list persistence
├── ui/                          # React + Vite + TanStack Query
│   ├── package.json
│   ├── vite.config.ts
│   └── src/
│       ├── api/                 # API client, WebSocket, types
│       ├── hooks/               # TanStack Query hooks
│       ├── components/          # React components
│       ├── layouts/             # Page layouts
│       └── styles/              # Global CSS
└── docs/
    └── PLAN.md
```

## Motion Profile

The stop block uses a **two-phase velocity profile** for every move:

**Phase 1 — Cruise:** FastAccelStepper generates a trapezoidal profile automatically. Long moves cruise at `MAX_SPEED_MM_S`; short moves triangle-profile and never reach max speed — a 5mm jog is naturally slower than a 500mm traverse with no special handling.

**Phase 2 — Approach zone:** When within `APPROACH_ZONE_MM` of the target, `StepperMotion::update()` drops speed to `APPROACH_SPEED_MM_S` regardless of move length. The block always arrives at the final position slowly for accurate latch engagement.

```
Velocity
  │        ╭──────────────────╮
  │       ╱   cruise speed     ╲    approach speed
  │──────╱                      ╲──────────────────
  └──────────────────────────────────────────────► Position
                                ▲
                        APPROACH_ZONE_MM from target
```

### Motion parameters

| Parameter             | Default   | Notes                                                  |
| --------------------- | --------- | ------------------------------------------------------ |
| `MAX_SPEED_MM_S`      | 200 mm/s  | ~300 RPM — well within NEMA 17 torque band at 24V      |
| `HOMING_SPEED_MM_S`   | 150 mm/s  | Conservative for reliable switch detection             |
| `ACCELERATION_MM_S2`  | 800 mm/s² | Tuned for a light carriage; reduce if steps are missed |
| `APPROACH_ZONE_MM`    | 30 mm     | Must be ≥ `MAX_SPEED² / (2 × ACCEL)` ≈ 25 mm           |
| `APPROACH_SPEED_MM_S` | 15 mm/s   | Final positioning speed for latch accuracy             |
| `SETTLING_MS`         | 350 ms    | Wait after move stops before latch engages             |

To go faster, increase `MAX_SPEED_MM_S` and `ACCELERATION_MM_S2` together, and ensure `APPROACH_ZONE_MM` stays above `MAX_SPEED² / (2 × ACCEL)`. See `docs/PLAN.md` for the full tuning ladder.

## Configuration

Edit `firmware/include/config.h` to match your hardware:

- `STEPS_PER_REV` / `MICROSTEPS` / `GT2_PULLEY_TEETH` — adjust if using different stepper or pulleys
- `MAX_TRAVEL_MM` — length of your fence rail
- `MAX_SPEED_MM_S` / `ACCELERATION_MM_S2` / `APPROACH_ZONE_MM` / `APPROACH_SPEED_MM_S` — motion profile tuning
- `WIFI_SSID` / `WIFI_PASSWORD` — your network (or create `credentials.h`)
- Servo angles, debounce timing, dust collection delays, etc.

Set `VITE_PROXY_TARGET=http://<esp32-ip>` when running the UI dev server to proxy API requests to the ESP32 (or use `http://localhost:3001` to proxy to the sim).

## License

MIT
