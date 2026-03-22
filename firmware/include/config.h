#pragma once

// =============================================================================
// Configuration — CNC Stop Block
// =============================================================================
// All tuneable parameters in one place. No magic numbers elsewhere.
// Ender 3 defaults: NEMA 17 (200 steps/rev), GT2 belt, 20-tooth pulley.
// =============================================================================

// --- Motion: Stepper + GT2 Belt ---
constexpr int    STEPS_PER_REV        = 200;
constexpr int    MICROSTEPS           = 16;
constexpr int    GT2_PULLEY_TEETH     = 20;
constexpr float  GT2_PITCH_MM         = 2.0f;

// Calculated: (200 * 16) / (20 * 2.0) = 80 steps/mm
constexpr float  STEPS_PER_MM         = (float)(STEPS_PER_REV * MICROSTEPS)
                                        / (GT2_PULLEY_TEETH * GT2_PITCH_MM);

// Travel speed increased from 50 → 200 mm/s. The Ender 3 NEMA 17 comfortably
// handles this on a light stop block carriage (vs. the much heavier heated bed
// it was designed to move). At 200 mm/s the motor runs at ~300 RPM — well
// within the torque band for 24V operation.
//
// Tuning ladder (increase one step at a time, check for missed steps):
//   Conservative: MAX_SPEED=50,  ACCEL=200  (original)
//   Moderate:     MAX_SPEED=200, ACCEL=800  ← current
//   Aggressive:   MAX_SPEED=350, ACCEL=1200 (test accuracy before committing)
//
// If you see position drift after repeated cycles, lower ACCELERATION first.
// Bump SETTLING_MS if the block oscillates before locking at higher speeds.
// Travel speed increased from 50 → 200 mm/s. The Ender 3 NEMA 17 comfortably
// handles this on a light stop block carriage (vs. the much heavier heated bed
// it was designed to move). At 200 mm/s the motor runs at ~300 RPM — well
// within the torque band for 24V operation.
//
// Tuning ladder (increase one step at a time, check for missed steps):
//   Conservative: MAX_SPEED=50,  ACCEL=200  (original)
//   Moderate:     MAX_SPEED=200, ACCEL=800  ← current
//   Aggressive:   MAX_SPEED=350, ACCEL=1200 (test accuracy before committing)
//
// If you see position drift after repeated cycles, lower ACCELERATION first.
// Bump SETTLING_MS if the block oscillates before locking at higher speeds.
constexpr float  MAX_SPEED_MM_S       = 200.0f;    // 16000 steps/s (~300 RPM)
constexpr float  HOMING_SPEED_MM_S    = 15.0f;     // 1200 steps/s (slightly faster home)
constexpr float  ACCELERATION_MM_S2   = 800.0f;    // 64000 steps/s^2
constexpr float  POSITION_TOLERANCE_MM = 0.05f;

// --- Approach Zone ---
// When the block is within APPROACH_ZONE_MM of the target, speed is reduced to
// APPROACH_SPEED_MM_S for final precise positioning.
//
// IMPORTANT: APPROACH_ZONE_MM must be large enough to decelerate from
// MAX_SPEED to APPROACH_SPEED using ACCELERATION:
//   min zone = (MAX_SPEED^2 - APPROACH_SPEED^2) / (2 * ACCELERATION)
//            = (200^2 - 15^2) / (2 * 800) = ~24.9 mm  →  30 mm is safe.
//
// Increasing APPROACH_ZONE_MM trades a little time for more precision.
constexpr float  APPROACH_ZONE_MM     = 30.0f;
constexpr float  APPROACH_SPEED_MM_S  = 15.0f;

// --- Travel Limits ---
constexpr float  MAX_TRAVEL_MM        = 1200.0f;   // Max fence length
constexpr float  HOME_BACKOFF_MM      = 2.0f;      // Back off after hitting home switch

// --- Servo Latch ---
constexpr int    SERVO_LOCKED_ANGLE   = 0;
constexpr int    SERVO_UNLOCKED_ANGLE = 90;
constexpr unsigned long SERVO_MOVE_TIME_MS = 300;

// --- RFID ---
constexpr unsigned long RFID_POLL_INTERVAL_MS = 500;

// --- Dust Collection ---
constexpr unsigned long DUST_ON_DELAY_MS  = 1000;  // Spin-up before cut
constexpr unsigned long DUST_OFF_DELAY_MS = 3000;  // Run-down after cut

// --- Button Debounce ---
constexpr unsigned long DEBOUNCE_MS = 50;

// --- Wi-Fi ---
#ifndef WIFI_SSID
#define WIFI_SSID     "SmartWorkshop"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""  // Override in credentials.h or build flags
#endif
constexpr bool   WIFI_AP_FALLBACK     = true;
#define AP_SSID   "CNC-StopBlock"
#define AP_PASS   "stopblock"

// --- WebSocket ---
constexpr unsigned long WS_UPDATE_INTERVAL_MS = 100; // 10 Hz status broadcast
constexpr int    WS_MAX_CLIENTS              = 4;    // Max concurrent WebSocket connections

// --- API Security ---
constexpr size_t MAX_POST_BODY_BYTES         = 16384; // 16 KB cap on POST body
constexpr int    MAX_CUTS                    = 100;   // Max entries in cut list
constexpr int    MAX_TOOLS                   = 50;    // Max entries in tool registry
constexpr int    MAX_STRING_LENGTH           = 64;    // Max label/name length

// --- NeoPixel ---
constexpr uint8_t LED_BRIGHTNESS = 128;

// --- Safety Timeouts ---
constexpr unsigned long HOMING_TIMEOUT_MS = (unsigned long)((MAX_TRAVEL_MM / HOMING_SPEED_MM_S) * 1500); // 1.5x expected time
constexpr unsigned long MOVE_TIMEOUT_MS   = (unsigned long)((MAX_TRAVEL_MM / MAX_SPEED_MM_S) * 1500);
// Settling increased from 200 → 350 ms to give the carriage more time to
// stop oscillating before the latch engages at higher approach speeds.
constexpr unsigned long SETTLING_MS         = 350;
constexpr unsigned long SETTLING_TIMEOUT_MS = 2000;
