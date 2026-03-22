#pragma once

#include <Arduino.h>

// Abstract position feedback interface.
// Swap implementations without changing any other code.
class DROInterface {
public:
    virtual ~DROInterface() = default;
    virtual void begin() = 0;
    virtual void update() = 0;
    virtual float getPositionMM() const = 0;
    virtual void  setOrigin() = 0;
    virtual void  setPositionMM(float mm) = 0;
    virtual bool  isAvailable() const = 0;
    virtual const char* getName() const = 0;
};
