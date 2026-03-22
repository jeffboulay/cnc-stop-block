#pragma once

#include <Arduino.h>
#include "config.h"
#include "pins.h"
#include "StepperMotion.h"
#include "DROInterface.h"
#include "DROStepperOnly.h"
#include "ServoLatch.h"
#include "RFIDReader.h"
#include "DustCollection.h"
#include "IndicatorLights.h"
#include "PneumaticClamps.h"
#include "ButtonPanel.h"
#include "CutList.h"
#include "WiFiProvisioning.h"

enum class SystemState {
    BOOT,
    WIFI_CONNECT,
    PROVISIONING,   // AP mode: captive portal waiting for WiFi credentials
    NEEDS_HOMING,
    HOMING,
    IDLE,
    UNLOCKING,
    MOVING,
    SETTLING,
    LOCKING,
    LOCKED,
    DUST_SPINUP,
    CUTTING,
    ERROR,
    ESTOP,
};

const char* systemStateToString(SystemState state);

class SystemController {
public:
    SystemController();
    ~SystemController();

    void begin();
    void update(); // Call from loop()

    // Commands (from WebAPI or ButtonPanel)
    void commandHome();
    void commandGoTo(float positionMM);
    void commandLock();
    void commandUnlock();
    void commandEStop();
    void commandResetError();
    void commandStartCut();
    void commandEndCut();
    void commandJog(float distanceMM);
    void commandNextCut();

    // Status getters (for WebAPI)
    SystemState getState() const;
    float getCurrentPositionMM() const;
    float getTargetPositionMM() const;
    bool  isHomed() const;
    bool  isLocked() const;
    bool  isDustActive() const;
    bool  areClampsEngaged() const;
    ToolInfo getActiveTool() const;
    bool  isToolPresent() const;
    CutList& getCutList();
    RFIDReader& getRFIDReader();

    // Error info
    String getErrorMessage() const;

private:
    SystemState _state = SystemState::BOOT;
    unsigned long _stateEnteredAt = 0;
    float _targetPositionMM = 0;
    String _errorMessage;

    // Subsystems (owned)
    StepperMotion*    _stepper;
    DROInterface*     _dro;
    ServoLatch*       _latch;
    RFIDReader*       _rfid;
    DustCollection*   _dust;
    IndicatorLights*  _lights;
    PneumaticClamps*  _clamps;
    ButtonPanel*      _buttons;
    CutList*          _cutList;
    WiFiProvisioning* _provisioning;

    void transitionTo(SystemState newState);
    void updateLights();
    void handleButtons();
    void checkFactoryReset(); // Checks E-Stop hold during begin()

    // State handlers
    void handleBoot();
    void handleWifiConnect();
    void handleProvisioning();
    void handleNeedsHoming();
    void handleHoming();
    void handleIdle();
    void handleUnlocking();
    void handleMoving();
    void handleSettling();
    void handleLocking();
    void handleLocked();
    void handleDustSpinup();
    void handleCutting();
    void handleError();
    void handleEstop();
};
