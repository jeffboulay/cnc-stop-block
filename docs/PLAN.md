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
- **Control UI**: iPad Pro via responsive web interface (touch-optimized)

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

## Project Structure
```
cnc-stop-block/
├── platformio.ini
├── include/
│   ├── config.h              # Tuning params (steps/mm, accel, etc.)
│   └── pins.h                # All GPIO pin assignments
├── src/
│   ├── main.cpp              # setup() + loop(), trivially simple
│   ├── SystemController.h/cpp # Central state machine
│   ├── StepperMotion.h/cpp   # FastAccelStepper wrapper (mm-based API)
│   ├── DROInterface.h        # Abstract position feedback (pure virtual)
│   ├── DROStepperOnly.h/cpp  # Fallback: open-loop step counting
│   ├── DRORotaryEncoder.h/cpp # Future: quadrature encoder
│   ├── DROLinearScale.h/cpp  # Future: iGaging scale
│   ├── ServoLatch.h/cpp      # Servo-controlled magnet lock
│   ├── RFIDReader.h/cpp      # MFRC522 tool detection + registry
│   ├── DustCollection.h/cpp  # Relay control with spin-up/down delays
│   ├── IndicatorLights.h/cpp # NeoPixel status patterns
│   ├── PneumaticClamps.h/cpp # Solenoid valve control
│   ├── ButtonPanel.h/cpp     # Debounced physical buttons
│   ├── WebUI.h/cpp           # AsyncWebServer + WebSocket
│   └── CutList.h/cpp         # Cut list persistence (LittleFS)
├── data/                     # Web UI (uploaded to LittleFS)
│   ├── index.html            # PWA-ready SPA, touch-optimized for iPad Pro
│   ├── manifest.json         # PWA manifest (add-to-homescreen as kiosk app)
│   ├── style.css             # Dark theme, large touch targets, responsive
│   └── app.js                # WebSocket client, touch gesture handling
└── test/
    └── test_main.cpp
```

## Key Libraries (platformio.ini)
- `FastAccelStepper` — hardware-timer step generation (won't stutter under web server load)
- `ESP32Servo` — servo control via LEDC
- `MFRC522` — RFID reader
- `ESPAsyncWebServer` + `AsyncTCP` — non-blocking web server
- `Adafruit NeoPixel` — LED status indicators
- `ArduinoJson` — WebSocket message serialization

## State Machine
```
BOOT → WIFI_CONNECT → NEEDS_HOMING → HOMING → IDLE
IDLE → UNLOCKING → MOVING → SETTLING → LOCKING → LOCKED
LOCKED → DUST_SPINUP → CUTTING → LOCKED
LOCKED → UNLOCKING (new position)
ANY → ESTOP (hardware interrupt, requires manual reset)
ANY → ERROR (stall, timeout, position mismatch)
```

## Safety Design
- **Hardware E-Stop**: NC switch in series with stepper driver ENABLE (works even if ESP32 crashes)
- **Limit switches**: NC wiring (broken wire = triggered = fail-safe)
- **Firmware**: watchdog timer, position validation, state timeouts, motion lockout during ESTOP/ERROR

## Implementation Phases

### Phase 1 — Skeleton & Motion
1. Create `platformio.ini`, `pins.h`, `config.h`
2. Implement `StepperMotion` (FastAccelStepper wrapper)
3. Implement `DROStepperOnly` (open-loop fallback)
4. Implement `ButtonPanel` (Home, Go, E-Stop)
5. Minimal `SystemController` (BOOT → NEEDS_HOMING → HOMING → IDLE → MOVING → ESTOP)
6. Wire up in `main.cpp`

### Phase 2 — Latch & Indicators
7. Implement `ServoLatch`
8. Add UNLOCKING, SETTLING, LOCKING, LOCKED states
9. Implement `IndicatorLights` (NeoPixel patterns per state)
10. Limit switch handling + timeout guards

### Phase 3 — Web Interface (iPad Pro Touch UI)
11. Implement `WebUI` (AsyncWebServer + WebSocket)
12. Build touch-optimized web UI for iPad Pro:
    - Large touch targets (min 44px), workshop-glove friendly
    - Responsive layout optimized for iPad Pro landscape (1024x768+)
    - Large DRO-style position display (easy to read from distance)
    - Touch-friendly jog buttons with selectable step sizes
    - Swipeable cut list with drag-to-reorder
    - Full-screen "kiosk" mode (add-to-homescreen PWA)
    - Dark theme (high contrast for workshop lighting)
    - Haptic-style visual feedback on button presses
    - No pinch/zoom needed — everything fits on screen
13. Implement `CutList` with LittleFS persistence
14. REST API + WebSocket command/status protocol

### Phase 4 — Peripherals
15. Implement `RFIDReader` with tool registry (LittleFS JSON)
16. Implement `DustCollection` (relay + delays)
17. Implement `PneumaticClamps` (solenoid)
18. Add DUST_SPINUP and CUTTING states

### Phase 5 — DRO & Polish
19. Implement concrete DRO classes (rotary encoder / linear scale)
20. Closed-loop position verification in SETTLING state
21. OTA firmware updates
22. TMC2209 UART mode (StealthChop / StallGuard)

## Verification
- **Phase 1**: Stepper moves to target positions via serial commands, homes correctly
- **Phase 2**: Servo locks/unlocks, LEDs reflect state, E-stop cuts power
- **Phase 3**: Web UI connects, shows live position, sends commands
- **Phase 4**: RFID detects tags, dust relay toggles, clamp solenoid fires
- **Phase 5**: DRO matches stepper position within 0.05mm tolerance
