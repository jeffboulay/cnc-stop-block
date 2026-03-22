#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum class LightPattern {
    OFF,
    SOLID_GREEN,     // IDLE / LOCKED / ready
    SOLID_BLUE,      // HOMING
    PULSE_YELLOW,    // MOVING
    SOLID_RED,       // ERROR
    FLASH_RED,       // ESTOP
    CHASE_WHITE,     // BOOT / WIFI_CONNECT
    PULSE_GREEN,     // SETTLING / LOCKING
    SOLID_ORANGE,    // DUST_SPINUP / CUTTING
};

class IndicatorLights {
public:
    IndicatorLights(uint8_t pin, uint16_t count, uint8_t brightness);

    void begin();
    void setPattern(LightPattern pattern);
    void update(); // Call from loop() to animate

private:
    Adafruit_NeoPixel _strip;
    LightPattern _pattern = LightPattern::OFF;
    unsigned long _lastAnimMs = 0;
    uint8_t _animFrame = 0;

    void setSolid(uint32_t color);
    void animatePulse(uint8_t r, uint8_t g, uint8_t b);
    void animateFlash(uint8_t r, uint8_t g, uint8_t b);
    void animateChase(uint8_t r, uint8_t g, uint8_t b);
};
