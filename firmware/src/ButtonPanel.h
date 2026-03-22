#pragma once

#include <Arduino.h>
#include "config.h"

enum class ButtonEvent {
    NONE,
    GO_PRESSED,
    HOME_PRESSED,
    LOCK_PRESSED,
    ESTOP_PRESSED,
    ESTOP_RELEASED,
};

class ButtonPanel {
public:
    ButtonPanel(uint8_t goPin, uint8_t homePin, uint8_t lockPin, uint8_t estopPin);

    void begin();
    ButtonEvent poll(); // Returns highest-priority event since last poll

private:
    struct Button {
        uint8_t pin;
        bool lastStableState;
        bool lastRawState;
        unsigned long lastChangeMs;
    };

    Button _buttons[4];
    static const int BTN_GO    = 0;
    static const int BTN_HOME  = 1;
    static const int BTN_LOCK  = 2;
    static const int BTN_ESTOP = 3;

    bool debounceRead(Button& btn);
};
