#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

class ServoLatch {
public:
    ServoLatch(uint8_t servoPin, int lockedAngle, int unlockedAngle, unsigned long moveTimeMs);

    void begin();
    void lock();
    void unlock();
    void update();

    bool isLocked() const;
    bool isUnlocked() const;
    bool isMoving() const;

private:
    enum class LatchState { LOCKED, UNLOCKING, UNLOCKED, LOCKING };

    Servo _servo;
    uint8_t _pin;
    int _lockedAngle;
    int _unlockedAngle;
    unsigned long _moveTimeMs;
    LatchState _state = LatchState::UNLOCKED;
    unsigned long _moveStartedAt = 0;
};
