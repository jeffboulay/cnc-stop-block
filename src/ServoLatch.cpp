#include "ServoLatch.h"

ServoLatch::ServoLatch(uint8_t servoPin, int lockedAngle, int unlockedAngle,
                       unsigned long moveTimeMs)
    : _pin(servoPin), _lockedAngle(lockedAngle), _unlockedAngle(unlockedAngle),
      _moveTimeMs(moveTimeMs) {}

void ServoLatch::begin() {
    _servo.attach(_pin);
    _servo.write(_unlockedAngle);
    _state = LatchState::UNLOCKED;
}

void ServoLatch::lock() {
    if (_state == LatchState::LOCKED || _state == LatchState::LOCKING) return;
    _servo.write(_lockedAngle);
    _state = LatchState::LOCKING;
    _moveStartedAt = millis();
}

void ServoLatch::unlock() {
    if (_state == LatchState::UNLOCKED || _state == LatchState::UNLOCKING) return;
    _servo.write(_unlockedAngle);
    _state = LatchState::UNLOCKING;
    _moveStartedAt = millis();
}

void ServoLatch::update() {
    if (_state == LatchState::LOCKING && millis() - _moveStartedAt >= _moveTimeMs) {
        _state = LatchState::LOCKED;
    }
    if (_state == LatchState::UNLOCKING && millis() - _moveStartedAt >= _moveTimeMs) {
        _state = LatchState::UNLOCKED;
    }
}

bool ServoLatch::isLocked() const {
    return _state == LatchState::LOCKED;
}

bool ServoLatch::isUnlocked() const {
    return _state == LatchState::UNLOCKED;
}

bool ServoLatch::isMoving() const {
    return _state == LatchState::LOCKING || _state == LatchState::UNLOCKING;
}
