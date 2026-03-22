#include "StepperMotion.h"

StepperMotion::StepperMotion(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin,
                             uint8_t homeSwitchPin, uint8_t farSwitchPin)
    : _stepPin(stepPin), _dirPin(dirPin), _enablePin(enablePin),
      _homeSwitchPin(homeSwitchPin), _farSwitchPin(farSwitchPin) {}

void StepperMotion::begin() {
    pinMode(_homeSwitchPin, INPUT_PULLUP);
    pinMode(_farSwitchPin, INPUT_PULLUP);

    _engine.init();
    _stepper = _engine.stepperConnectToPin(_stepPin);

    if (_stepper) {
        _stepper->setDirectionPin(_dirPin);
        _stepper->setEnablePin(_enablePin);
        _stepper->setAutoEnable(true);

        _stepper->setSpeedInHz((uint32_t)(MAX_SPEED_MM_S * STEPS_PER_MM));
        _stepper->setAcceleration((uint32_t)(ACCELERATION_MM_S2 * STEPS_PER_MM));
    }
}

void StepperMotion::moveToMM(float positionMM) {
    if (!_stepper || !_homed) return;
    if (positionMM < 0 || positionMM > MAX_TRAVEL_MM) return;

    _stepper->moveTo(mmToSteps(positionMM));
}

void StepperMotion::jogMM(float distanceMM) {
    if (!_stepper || !_homed) return;

    long currentSteps = _stepper->getCurrentPosition();
    long targetSteps = currentSteps + mmToSteps(distanceMM);

    // Clamp to travel limits
    long maxSteps = mmToSteps(MAX_TRAVEL_MM);
    if (targetSteps < 0) targetSteps = 0;
    if (targetSteps > maxSteps) targetSteps = maxSteps;

    _stepper->moveTo(targetSteps);
}

void StepperMotion::startHoming() {
    if (!_stepper) return;

    _homed = false;
    _homingPhase = HomingPhase::SEEKING;
    _homingStartMs = millis();

    // Move toward home at homing speed
    _stepper->setSpeedInHz((uint32_t)(HOMING_SPEED_MM_S * STEPS_PER_MM));
    _stepper->setAcceleration((uint32_t)(ACCELERATION_MM_S2 * STEPS_PER_MM));

    // Move a large negative distance (toward home)
    _stepper->move(mmToSteps(-MAX_TRAVEL_MM - 50));
}

StepperMotion::HomingPhase StepperMotion::updateHoming() {
    if (_homingPhase == HomingPhase::IDLE ||
        _homingPhase == HomingPhase::COMPLETE ||
        _homingPhase == HomingPhase::FAILED) {
        return _homingPhase;
    }

    // Timeout check
    if (millis() - _homingStartMs > HOMING_TIMEOUT_MS) {
        _stepper->forceStop();
        _homingPhase = HomingPhase::FAILED;
        return _homingPhase;
    }

    switch (_homingPhase) {
        case HomingPhase::SEEKING:
            if (isHomeSwitchActive()) {
                _stepper->forceStop();
                delay(50); // Brief settle

                // Back off the switch
                _stepper->setCurrentPosition(0);
                _stepper->setSpeedInHz((uint32_t)(HOMING_SPEED_MM_S * STEPS_PER_MM * 0.5f));
                _stepper->move(mmToSteps(HOME_BACKOFF_MM));
                _homingPhase = HomingPhase::BACKING_OFF;
            }
            break;

        case HomingPhase::BACKING_OFF:
            if (!_stepper->isRunning()) {
                // Set this as the zero position
                _stepper->setCurrentPosition(0);

                // Restore full speed
                _stepper->setSpeedInHz((uint32_t)(MAX_SPEED_MM_S * STEPS_PER_MM));
                _stepper->setAcceleration((uint32_t)(ACCELERATION_MM_S2 * STEPS_PER_MM));

                _homed = true;
                _homingPhase = HomingPhase::COMPLETE;
            }
            break;

        default:
            break;
    }

    return _homingPhase;
}

void StepperMotion::stop() {
    if (_stepper) {
        _stepper->forceStop();
    }
}

void StepperMotion::emergencyStop() {
    if (_stepper) {
        _stepper->forceStopAndNewPosition(_stepper->getCurrentPosition());
        _stepper->disableOutputs();
    }
    _homed = false;
    _homingPhase = HomingPhase::IDLE;
}

bool StepperMotion::isMoving() const {
    return _stepper && _stepper->isRunning();
}

bool StepperMotion::isHomed() const {
    return _homed;
}

float StepperMotion::getCurrentPositionMM() const {
    if (!_stepper) return 0;
    return stepsToMM(_stepper->getCurrentPosition());
}

long StepperMotion::getCurrentPositionSteps() const {
    if (!_stepper) return 0;
    return _stepper->getCurrentPosition();
}

void StepperMotion::setCurrentPositionMM(float mm) {
    if (_stepper) {
        _stepper->setCurrentPosition(mmToSteps(mm));
    }
}

bool StepperMotion::isHomeSwitchActive() const {
    return digitalRead(_homeSwitchPin) == LOW; // NC switch: LOW = triggered
}

bool StepperMotion::isFarSwitchActive() const {
    return digitalRead(_farSwitchPin) == LOW;
}

long StepperMotion::mmToSteps(float mm) const {
    return (long)(mm * STEPS_PER_MM);
}

float StepperMotion::stepsToMM(long steps) const {
    return (float)steps / STEPS_PER_MM;
}
