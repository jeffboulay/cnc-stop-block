#pragma once

#include <Arduino.h>
#include "config.h"

class DustCollection {
public:
    DustCollection(uint8_t relayPin, unsigned long onDelayMs, unsigned long offDelayMs);

    void begin();
    void activate();
    void deactivate();
    void forceOff();
    void update();

    bool isActive() const;
    bool isSpunUp() const;

private:
    enum class DustState { OFF, SPINNING_UP, RUNNING, SPINNING_DOWN };

    uint8_t _pin;
    unsigned long _onDelayMs;
    unsigned long _offDelayMs;
    DustState _state = DustState::OFF;
    unsigned long _stateChangedAt = 0;
};
