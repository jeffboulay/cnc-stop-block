#include "PneumaticClamps.h"

PneumaticClamps::PneumaticClamps(uint8_t solenoidPin)
    : _pin(solenoidPin) {}

void PneumaticClamps::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void PneumaticClamps::engage() {
    digitalWrite(_pin, HIGH);
    _engaged = true;
}

void PneumaticClamps::release() {
    digitalWrite(_pin, LOW);
    _engaged = false;
}

bool PneumaticClamps::isEngaged() const {
    return _engaged;
}
