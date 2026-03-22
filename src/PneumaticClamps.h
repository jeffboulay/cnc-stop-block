#pragma once

#include <Arduino.h>

class PneumaticClamps {
public:
    explicit PneumaticClamps(uint8_t solenoidPin);

    void begin();
    void engage();
    void release();
    bool isEngaged() const;

private:
    uint8_t _pin;
    bool _engaged = false;
};
