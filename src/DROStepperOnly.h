#pragma once

#include "DROInterface.h"
#include "StepperMotion.h"

// Open-loop DRO: reads position directly from stepper step count.
// Use as fallback before a physical encoder is installed.
class DROStepperOnly : public DROInterface {
public:
    explicit DROStepperOnly(StepperMotion* stepper);

    void begin() override;
    void update() override;
    float getPositionMM() const override;
    void  setOrigin() override;
    void  setPositionMM(float mm) override;
    bool  isAvailable() const override;
    const char* getName() const override;

private:
    StepperMotion* _stepper;
};
