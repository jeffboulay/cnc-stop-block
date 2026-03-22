#include "ButtonPanel.h"

ButtonPanel::ButtonPanel(uint8_t goPin, uint8_t homePin, uint8_t lockPin, uint8_t estopPin) {
    _buttons[BTN_GO]    = { goPin,    true, true, 0 };
    _buttons[BTN_HOME]  = { homePin,  true, true, 0 };
    _buttons[BTN_LOCK]  = { lockPin,  true, true, 0 };
    _buttons[BTN_ESTOP] = { estopPin, true, true, 0 };
}

void ButtonPanel::begin() {
    for (int i = 0; i < 4; i++) {
        pinMode(_buttons[i].pin, INPUT_PULLUP);
        _buttons[i].lastStableState = digitalRead(_buttons[i].pin);
        _buttons[i].lastRawState = _buttons[i].lastStableState;
        _buttons[i].lastChangeMs = millis();
    }
}

bool ButtonPanel::debounceRead(Button& btn) {
    bool rawState = digitalRead(btn.pin);

    if (rawState != btn.lastRawState) {
        btn.lastChangeMs = millis();
        btn.lastRawState = rawState;
    }

    if (millis() - btn.lastChangeMs >= DEBOUNCE_MS) {
        if (rawState != btn.lastStableState) {
            btn.lastStableState = rawState;
            return true; // State changed
        }
    }
    return false; // No change
}

ButtonEvent ButtonPanel::poll() {
    // E-Stop has highest priority
    if (debounceRead(_buttons[BTN_ESTOP])) {
        if (_buttons[BTN_ESTOP].lastStableState == LOW) {
            return ButtonEvent::ESTOP_PRESSED;
        } else {
            return ButtonEvent::ESTOP_RELEASED;
        }
    }

    // Other buttons: trigger on press (HIGH -> LOW transition)
    if (debounceRead(_buttons[BTN_GO]) && _buttons[BTN_GO].lastStableState == LOW) {
        return ButtonEvent::GO_PRESSED;
    }
    if (debounceRead(_buttons[BTN_HOME]) && _buttons[BTN_HOME].lastStableState == LOW) {
        return ButtonEvent::HOME_PRESSED;
    }
    if (debounceRead(_buttons[BTN_LOCK]) && _buttons[BTN_LOCK].lastStableState == LOW) {
        return ButtonEvent::LOCK_PRESSED;
    }

    return ButtonEvent::NONE;
}
