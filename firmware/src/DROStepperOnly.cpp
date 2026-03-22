#include "DROStepperOnly.h"

DROStepperOnly::DROStepperOnly(StepperMotion* stepper)
    : _stepper(stepper) {}

void DROStepperOnly::begin() {
    // Nothing to initialize — piggybacks on stepper
}

void DROStepperOnly::update() {
    // Nothing to poll — reads stepper position directly
}

float DROStepperOnly::getPositionMM() const {
    return _stepper->getCurrentPositionMM();
}

void DROStepperOnly::setOrigin() {
    _stepper->setCurrentPositionMM(0);
}

void DROStepperOnly::setPositionMM(float mm) {
    _stepper->setCurrentPositionMM(mm);
}

bool DROStepperOnly::isAvailable() const {
    return true; // Always available as long as stepper exists
}

const char* DROStepperOnly::getName() const {
    return "Stepper (open-loop)";
}
