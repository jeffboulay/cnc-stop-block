#pragma once

// =============================================================================
// GPIO Pin Assignments — CNC Stop Block
// =============================================================================
// Single source of truth for all pin assignments.
// Ender 3 parts: NEMA 17 stepper, GT2 belt, endstops, 24V PSU.
// Avoids ESP32 strapping pins (GPIO 0, 2, 5, 12, 15) for critical outputs.
// =============================================================================

// --- Stepper Motor (FastAccelStepper via hardware timer) ---
#define PIN_STEPPER_STEP     25
#define PIN_STEPPER_DIR      26
#define PIN_STEPPER_ENABLE   27  // Active LOW on most drivers (A4988/TMC2209)

// --- Limit Switches (from Ender 3 mechanical endstops) ---
// Input-only pins, NC switches wired to GND (fail-safe: broken wire = triggered)
#define PIN_LIMIT_HOME       34
#define PIN_LIMIT_FAR        35

// --- Servo Latch (magnet lock/unlock) ---
#define PIN_SERVO_LATCH      13

// --- RFID Reader (MFRC522 via VSPI) ---
#define PIN_RFID_SS          5
#define PIN_RFID_RST         22
// VSPI defaults: SCK=18, MISO=19, MOSI=23

// --- DRO Encoder (quadrature — optional, active when encoder installed) ---
// Input-only pins for clean signal
#define PIN_DRO_ENC_A        36
#define PIN_DRO_ENC_B        39

// --- NeoPixel Indicator Strip ---
#define PIN_NEOPIXEL         4
#define NEOPIXEL_COUNT       8

// --- Dust Collection Relay ---
#define PIN_DUST_RELAY       32

// --- Pneumatic Clamp Solenoid Valve ---
#define PIN_CLAMP_SOLENOID   33

// --- Physical Buttons ---
#define PIN_BTN_GO           14
#define PIN_BTN_HOME         12
#define PIN_BTN_LOCK         15
#define PIN_BTN_ESTOP        2   // Also has hardware interrupt for immediate response

// --- I2C (optional OLED display) ---
#define PIN_I2C_SDA          21
#define PIN_I2C_SCL          22  // Shared with RFID RST (safe: RST is one-shot at init)
