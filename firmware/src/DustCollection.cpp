#include "DustCollection.h"

DustCollection::DustCollection(uint8_t relayPin, unsigned long onDelayMs, unsigned long offDelayMs)
    : _pin(relayPin), _onDelayMs(onDelayMs), _offDelayMs(offDelayMs) {}

void DustCollection::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void DustCollection::activate() {
    if (_state == DustState::RUNNING || _state == DustState::SPINNING_UP) return;

    digitalWrite(_pin, HIGH);
    _state = DustState::SPINNING_UP;
    _stateChangedAt = millis();
}

void DustCollection::deactivate() {
    if (_state == DustState::OFF || _state == DustState::SPINNING_DOWN) return;

    _state = DustState::SPINNING_DOWN;
    _stateChangedAt = millis();
    // Relay stays ON during spin-down
}

void DustCollection::forceOff() {
    digitalWrite(_pin, LOW);
    _state = DustState::OFF;
}

void DustCollection::update() {
    switch (_state) {
        case DustState::SPINNING_UP:
            if (millis() - _stateChangedAt >= _onDelayMs) {
                _state = DustState::RUNNING;
            }
            break;

        case DustState::SPINNING_DOWN:
            if (millis() - _stateChangedAt >= _offDelayMs) {
                digitalWrite(_pin, LOW);
                _state = DustState::OFF;
            }
            break;

        default:
            break;
    }
}

bool DustCollection::isActive() const {
    return _state != DustState::OFF;
}

bool DustCollection::isSpunUp() const {
    return _state == DustState::RUNNING;
}
