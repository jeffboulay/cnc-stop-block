#include "IndicatorLights.h"

IndicatorLights::IndicatorLights(uint8_t pin, uint16_t count, uint8_t brightness)
    : _strip(count, pin, NEO_GRB + NEO_KHZ800) {
    _strip.setBrightness(brightness);
}

void IndicatorLights::begin() {
    _strip.begin();
    _strip.clear();
    _strip.show();
}

void IndicatorLights::setPattern(LightPattern pattern) {
    if (_pattern == pattern) return;
    _pattern = pattern;
    _animFrame = 0;
    _lastAnimMs = millis();
}

void IndicatorLights::update() {
    switch (_pattern) {
        case LightPattern::OFF:
            setSolid(0);
            break;
        case LightPattern::SOLID_GREEN:
            setSolid(_strip.Color(0, 255, 0));
            break;
        case LightPattern::SOLID_BLUE:
            setSolid(_strip.Color(0, 0, 255));
            break;
        case LightPattern::SOLID_RED:
            setSolid(_strip.Color(255, 0, 0));
            break;
        case LightPattern::SOLID_ORANGE:
            setSolid(_strip.Color(255, 140, 0));
            break;
        case LightPattern::PULSE_YELLOW:
            animatePulse(255, 200, 0);
            break;
        case LightPattern::PULSE_GREEN:
            animatePulse(0, 255, 0);
            break;
        case LightPattern::FLASH_RED:
            animateFlash(255, 0, 0);
            break;
        case LightPattern::CHASE_WHITE:
            animateChase(255, 255, 255);
            break;
        case LightPattern::SLOW_PULSE_BLUE:
            animateSlowPulse(0, 100, 255);
            break;
    }
}

void IndicatorLights::setSolid(uint32_t color) {
    for (uint16_t i = 0; i < _strip.numPixels(); i++) {
        _strip.setPixelColor(i, color);
    }
    _strip.show();
}

void IndicatorLights::animatePulse(uint8_t r, uint8_t g, uint8_t b) {
    if (millis() - _lastAnimMs < 20) return;
    _lastAnimMs = millis();

    _animFrame = (_animFrame + 1) % 200;
    // Triangle wave: 0-100-0 over 200 frames
    uint8_t brightness = _animFrame < 100 ? _animFrame : 200 - _animFrame;
    float scale = brightness / 100.0f;

    for (uint16_t i = 0; i < _strip.numPixels(); i++) {
        _strip.setPixelColor(i, (uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale));
    }
    _strip.show();
}

void IndicatorLights::animateSlowPulse(uint8_t r, uint8_t g, uint8_t b) {
    // ~2 second triangle-wave cycle (50 ms ticks × 200 frames = 10 s — too slow)
    // Use 40 ms ticks × 100 frames → 4 s, which reads as "waiting" not "busy"
    if (millis() - _lastAnimMs < 40) return;
    _lastAnimMs = millis();

    _animFrame = (_animFrame + 1) % 100;
    // Triangle wave: ramps 0→100 then 100→0 over 100 frames
    uint8_t brightness = _animFrame < 50 ? _animFrame * 2 : (100 - _animFrame) * 2;
    float scale = brightness / 100.0f;

    for (uint16_t i = 0; i < _strip.numPixels(); i++) {
        _strip.setPixelColor(i,
            (uint8_t)(r * scale),
            (uint8_t)(g * scale),
            (uint8_t)(b * scale));
    }
    _strip.show();
}

void IndicatorLights::animateFlash(uint8_t r, uint8_t g, uint8_t b) {
    if (millis() - _lastAnimMs < 250) return;
    _lastAnimMs = millis();

    _animFrame = (_animFrame + 1) % 2;
    uint32_t color = _animFrame ? _strip.Color(r, g, b) : 0;
    setSolid(color);
}

void IndicatorLights::animateChase(uint8_t r, uint8_t g, uint8_t b) {
    if (millis() - _lastAnimMs < 100) return;
    _lastAnimMs = millis();

    _animFrame = (_animFrame + 1) % _strip.numPixels();
    _strip.clear();
    _strip.setPixelColor(_animFrame, r, g, b);
    _strip.show();
}
