# CNC Stop Block

ESP32-powered CNC stop block for a miter saw station. Automatically positions a stop block along the fence for repeatable cut lengths, locks it in place, and integrates dust collection, RFID tool detection, pneumatic clamps, and a touch-optimized web UI controlled from an iPad Pro.

Built using parts salvaged from an **Ender 3 3D printer** (NEMA 17 stepper, GT2 belt/pulleys, V-slot extrusion, V-wheels, endstops, 24V PSU).

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
- **iPad Pro Web UI** — Touch-optimized dark-theme PWA with real-time WebSocket updates at 10 Hz

## Hardware

### From Ender 3

| Part | Use |
|---|---|
| NEMA 17 stepper (X or Y axis) | Belt-drive motor |
| GT2 belt + 20-tooth pulleys | Linear motion drive |
| 2040 V-slot extrusion | Fence rail / track |
| V-slot wheels + eccentric nuts | Stop block carriage |
| Mechanical endstops (x2) | Home + far-end limit switches |
| 24V PSU | Main power supply |
| TMC2208/2209 driver | Stepper driver |

### Additional Parts

| Part | Purpose |
|---|---|
| ESP32 DevKit V1 | Microcontroller (WiFi + GPIO) |
| 5V buck converter | Power for ESP32 and servo from 24V rail |
| SG90 servo | Magnet latch actuation |
| MFRC522 RFID module | Tool/blade detection |
| NeoPixel strip (8 LEDs) | Status indicators |
| Relay module | Dust collection switching |
| Solenoid valve | Pneumatic clamp control |
| Momentary buttons (x4) | Home, Go, Lock, E-Stop |

### Wiring

All GPIO assignments are in [`include/pins.h`](include/pins.h). Key connections:

| Function | GPIO |
|---|---|
| Stepper STEP / DIR / EN | 25 / 26 / 27 |
| Limit switches (home / far) | 34 / 35 |
| Servo latch | 13 |
| RFID SS / RST | 5 / 22 |
| NeoPixel data | 4 |
| Dust relay | 32 |
| Clamp solenoid | 33 |
| Buttons (Go / Home / Lock / E-Stop) | 14 / 12 / 15 / 2 |

Tuning parameters (steps/mm, speeds, delays) are in [`include/config.h`](include/config.h).

## Building & Flashing

Requires [PlatformIO](https://platformio.org/).

```bash
# Build firmware
pio run

# Upload firmware to ESP32
pio run -t upload

# Upload web UI files to LittleFS
pio run -t uploadfs

# Monitor serial output
pio device monitor
```

## Web UI

Once running, the ESP32 connects to your WiFi network (configured in `config.h`) or starts its own access point (`CNC-StopBlock`).

Open the ESP32's IP address in Safari on iPad Pro and tap **Add to Home Screen** for a full-screen kiosk experience. The UI features:

- Large monospace DRO position display (readable from across the shop)
- Target position and error readout
- Jog controls with selectable step sizes (0.1 / 1 / 10 / 100 mm)
- Direct go-to-position input
- Home, Lock/Unlock, Next Cut, Start/End Cut buttons
- Cut list editor with add/clear/reset
- Real-time status indicators (homed, locked, dust, clamps)
- Dark theme optimized for workshop lighting

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
├── platformio.ini          # Build config and library dependencies
├── include/
│   ├── config.h            # Tuning parameters (steps/mm, speeds, delays)
│   └── pins.h              # GPIO pin assignments
├── src/
│   ├── main.cpp            # Entry point
│   ├── SystemController.*  # Central state machine
│   ├── StepperMotion.*     # FastAccelStepper wrapper (mm-based API)
│   ├── DROInterface.h      # Abstract position feedback interface
│   ├── DROStepperOnly.*    # Open-loop DRO (step counting)
│   ├── ServoLatch.*        # Servo-controlled magnet lock
│   ├── RFIDReader.*        # MFRC522 tool detection + registry
│   ├── DustCollection.*    # Relay with spin-up/down delays
│   ├── IndicatorLights.*   # NeoPixel status patterns
│   ├── PneumaticClamps.*   # Solenoid valve control
│   ├── ButtonPanel.*       # Debounced physical buttons
│   ├── WebUI.*             # AsyncWebServer + WebSocket
│   └── CutList.*           # Cut list with LittleFS persistence
├── data/www/               # Web UI (uploaded to LittleFS)
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   └── manifest.json
└── docs/
    └── PLAN.md             # Detailed implementation plan
```

## Configuration

Edit `include/config.h` to match your hardware:

- `STEPS_PER_REV` / `MICROSTEPS` / `GT2_PULLEY_TEETH` — adjust if using different stepper or pulleys
- `MAX_TRAVEL_MM` — length of your fence rail
- `MAX_SPEED_MM_S` / `ACCELERATION_MM_S2` — motion tuning
- `WIFI_SSID` / `WIFI_PASSWORD` — your network (or create `credentials.h`)
- Servo angles, debounce timing, dust collection delays, etc.

## License

MIT
