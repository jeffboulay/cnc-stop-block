#pragma once

#include <Arduino.h>
#include <FastAccelStepper.h>
#include "config.h"
#include "pins.h"

class StepperMotion {
public:
    StepperMotion(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin,
                  uint8_t homeSwitchPin, uint8_t farSwitchPin);

    void begin();

    // Motion commands
    void moveToMM(float positionMM);
    void jogMM(float distanceMM);
    void startHoming();
    void stop();          // Controlled deceleration stop
    void emergencyStop(); // Immediate disable

    // Status
    bool  isMoving() const;
    bool  isHomed() const;
    float getCurrentPositionMM() const;
    long  getCurrentPositionSteps() const;
    void  setCurrentPositionMM(float mm);

    // Limit switches
    bool isHomeSwitchActive() const;
    bool isFarSwitchActive() const;

    // Call from loop() to manage homing sequence
    enum class HomingPhase { IDLE, SEEKING, BACKING_OFF, COMPLETE, FAILED };
    HomingPhase updateHoming();

private:
    uint8_t _stepPin, _dirPin, _enablePin;
    uint8_t _homeSwitchPin, _farSwitchPin;

    FastAccelStepperEngine _engine;
    FastAccelStepper* _stepper = nullptr;

    bool _homed = false;
    HomingPhase _homingPhase = HomingPhase::IDLE;
    unsigned long _homingStartMs = 0;

    long  mmToSteps(float mm) const;
    float stepsToMM(long steps) const;
};
