#include "SystemController.h"
#include <WiFi.h>
#include <LittleFS.h>

const char* systemStateToString(SystemState state) {
    switch (state) {
        case SystemState::BOOT:         return "BOOT";
        case SystemState::WIFI_CONNECT: return "WIFI_CONNECT";
        case SystemState::NEEDS_HOMING: return "NEEDS_HOMING";
        case SystemState::HOMING:       return "HOMING";
        case SystemState::IDLE:         return "IDLE";
        case SystemState::UNLOCKING:    return "UNLOCKING";
        case SystemState::MOVING:       return "MOVING";
        case SystemState::SETTLING:     return "SETTLING";
        case SystemState::LOCKING:      return "LOCKING";
        case SystemState::LOCKED:       return "LOCKED";
        case SystemState::DUST_SPINUP:  return "DUST_SPINUP";
        case SystemState::CUTTING:      return "CUTTING";
        case SystemState::ERROR:        return "ERROR";
        case SystemState::ESTOP:        return "ESTOP";
        default:                        return "UNKNOWN";
    }
}

SystemController::SystemController() {
    _stepper = new StepperMotion(PIN_STEPPER_STEP, PIN_STEPPER_DIR, PIN_STEPPER_ENABLE,
                                 PIN_LIMIT_HOME, PIN_LIMIT_FAR);
    _dro     = new DROStepperOnly(_stepper);
    _latch   = new ServoLatch(PIN_SERVO_LATCH, SERVO_LOCKED_ANGLE, SERVO_UNLOCKED_ANGLE,
                              SERVO_MOVE_TIME_MS);
    _rfid    = new RFIDReader(PIN_RFID_SS, PIN_RFID_RST);
    _dust    = new DustCollection(PIN_DUST_RELAY, DUST_ON_DELAY_MS, DUST_OFF_DELAY_MS);
    _lights  = new IndicatorLights(PIN_NEOPIXEL, NEOPIXEL_COUNT, LED_BRIGHTNESS);
    _clamps  = new PneumaticClamps(PIN_CLAMP_SOLENOID);
    _buttons = new ButtonPanel(PIN_BTN_GO, PIN_BTN_HOME, PIN_BTN_LOCK, PIN_BTN_ESTOP);
    _cutList = new CutList();
}

SystemController::~SystemController() {
    delete _stepper;
    delete _dro;
    delete _latch;
    delete _rfid;
    delete _dust;
    delete _lights;
    delete _clamps;
    delete _buttons;
    delete _cutList;
}

void SystemController::begin() {
    Serial.println("[SYS] CNC Stop Block starting...");

    LittleFS.begin(true); // Format on first use

    _stepper->begin();
    _dro->begin();
    _latch->begin();
    _rfid->begin();
    _dust->begin();
    _lights->begin();
    _clamps->begin();
    _buttons->begin();
    _cutList->begin();

    transitionTo(SystemState::WIFI_CONNECT);
}

void SystemController::update() {
    // Always update subsystems
    _dro->update();
    _latch->update();
    _rfid->update();
    _dust->update();
    _lights->update();

    // Handle button input (can trigger E-stop from any state)
    handleButtons();

    // State machine
    switch (_state) {
        case SystemState::BOOT:         handleBoot(); break;
        case SystemState::WIFI_CONNECT: handleWifiConnect(); break;
        case SystemState::NEEDS_HOMING: handleNeedsHoming(); break;
        case SystemState::HOMING:       handleHoming(); break;
        case SystemState::IDLE:         handleIdle(); break;
        case SystemState::UNLOCKING:    handleUnlocking(); break;
        case SystemState::MOVING:       handleMoving(); break;
        case SystemState::SETTLING:     handleSettling(); break;
        case SystemState::LOCKING:      handleLocking(); break;
        case SystemState::LOCKED:       handleLocked(); break;
        case SystemState::DUST_SPINUP:  handleDustSpinup(); break;
        case SystemState::CUTTING:      handleCutting(); break;
        case SystemState::ERROR:        handleError(); break;
        case SystemState::ESTOP:        handleEstop(); break;
    }

    // Safety: check far limit switch during motion
    if (_state == SystemState::MOVING && _stepper->isFarSwitchActive()) {
        _stepper->stop();
        _errorMessage = "Far limit switch triggered";
        transitionTo(SystemState::ERROR);
    }
}

// --- Commands ---

void SystemController::commandHome() {
    if (_state == SystemState::NEEDS_HOMING || _state == SystemState::IDLE) {
        _stepper->startHoming();
        transitionTo(SystemState::HOMING);
    }
}

void SystemController::commandGoTo(float positionMM) {
    // #10: Only allow GoTo from IDLE or LOCKED — block during MOVING, CUTTING, etc.
    if (_state != SystemState::IDLE && _state != SystemState::LOCKED) return;
    if (positionMM < 0 || positionMM > MAX_TRAVEL_MM) return;

    _targetPositionMM = positionMM;

    if (_state == SystemState::LOCKED) {
        // Need to unlock first
        _latch->unlock();
        transitionTo(SystemState::UNLOCKING);
    } else {
        // Already idle/unlocked, start moving
        _stepper->moveToMM(positionMM);
        transitionTo(SystemState::MOVING);
    }
}

void SystemController::commandLock() {
    if (_state == SystemState::IDLE) {
        _latch->lock();
        transitionTo(SystemState::LOCKING);
    }
}

void SystemController::commandUnlock() {
    if (_state == SystemState::LOCKED) {
        _latch->unlock();
        _clamps->release();
        transitionTo(SystemState::UNLOCKING);
    }
}

void SystemController::commandEStop() {
    _stepper->emergencyStop();
    _dust->forceOff();
    _clamps->release();
    transitionTo(SystemState::ESTOP);
}

void SystemController::commandResetError() {
    if (_state == SystemState::ERROR || _state == SystemState::ESTOP) {
        _errorMessage = "";
        transitionTo(SystemState::NEEDS_HOMING);
    }
}

void SystemController::commandStartCut() {
    if (_state == SystemState::LOCKED) {
        _dust->activate();
        _clamps->engage();
        transitionTo(SystemState::DUST_SPINUP);
    }
}

void SystemController::commandEndCut() {
    if (_state == SystemState::CUTTING) {
        _dust->deactivate();
        _clamps->release();

        // Mark current cut as completed
        int idx = _cutList->getNextIndex();
        if (idx >= 0) {
            _cutList->markCompleted(idx);
        }

        transitionTo(SystemState::LOCKED);
    }
}

void SystemController::commandJog(float distanceMM) {
    if (_state == SystemState::IDLE) {
        _stepper->jogMM(distanceMM);
        transitionTo(SystemState::MOVING);
    }
}

void SystemController::commandNextCut() {
    if (!_cutList->hasNext()) return;

    CutEntry next = _cutList->getNext();
    float targetMM = next.lengthMM;

    // Apply kerf offset if tool is detected
    if (_rfid->isToolPresent()) {
        targetMM += _rfid->getCurrentTool().kerfMM;
    }

    commandGoTo(targetMM);
}

// --- Status Getters ---

SystemState SystemController::getState() const { return _state; }

float SystemController::getCurrentPositionMM() const {
    return _dro->getPositionMM();
}

float SystemController::getTargetPositionMM() const { return _targetPositionMM; }
bool  SystemController::isHomed() const { return _stepper->isHomed(); }
bool  SystemController::isLocked() const { return _latch->isLocked(); }
bool  SystemController::isDustActive() const { return _dust->isActive(); }
bool  SystemController::areClampsEngaged() const { return _clamps->isEngaged(); }
ToolInfo SystemController::getActiveTool() const { return _rfid->getCurrentTool(); }
bool  SystemController::isToolPresent() const { return _rfid->isToolPresent(); }
CutList& SystemController::getCutList() { return *_cutList; }
RFIDReader& SystemController::getRFIDReader() { return *_rfid; }
String SystemController::getErrorMessage() const { return _errorMessage; }

// --- State Machine ---

void SystemController::transitionTo(SystemState newState) {
    Serial.printf("[SYS] %s -> %s\n", systemStateToString(_state), systemStateToString(newState));
    _state = newState;
    _stateEnteredAt = millis();
    updateLights();
}

void SystemController::updateLights() {
    switch (_state) {
        case SystemState::BOOT:
        case SystemState::WIFI_CONNECT:
            _lights->setPattern(LightPattern::CHASE_WHITE);
            break;
        case SystemState::NEEDS_HOMING:
            _lights->setPattern(LightPattern::PULSE_YELLOW);
            break;
        case SystemState::HOMING:
            _lights->setPattern(LightPattern::SOLID_BLUE);
            break;
        case SystemState::IDLE:
            _lights->setPattern(LightPattern::SOLID_GREEN);
            break;
        case SystemState::UNLOCKING:
        case SystemState::SETTLING:
        case SystemState::LOCKING:
            _lights->setPattern(LightPattern::PULSE_GREEN);
            break;
        case SystemState::MOVING:
            _lights->setPattern(LightPattern::PULSE_YELLOW);
            break;
        case SystemState::LOCKED:
            _lights->setPattern(LightPattern::SOLID_GREEN);
            break;
        case SystemState::DUST_SPINUP:
        case SystemState::CUTTING:
            _lights->setPattern(LightPattern::SOLID_ORANGE);
            break;
        case SystemState::ERROR:
            _lights->setPattern(LightPattern::SOLID_RED);
            break;
        case SystemState::ESTOP:
            _lights->setPattern(LightPattern::FLASH_RED);
            break;
    }
}

void SystemController::handleButtons() {
    ButtonEvent event = _buttons->poll();

    switch (event) {
        case ButtonEvent::ESTOP_PRESSED:
            commandEStop();
            break;
        case ButtonEvent::HOME_PRESSED:
            commandHome();
            break;
        case ButtonEvent::GO_PRESSED:
            if (_state == SystemState::IDLE || _state == SystemState::LOCKED) {
                commandNextCut();
            }
            break;
        case ButtonEvent::LOCK_PRESSED:
            if (_state == SystemState::IDLE) {
                commandLock();
            } else if (_state == SystemState::LOCKED) {
                commandUnlock();
            }
            break;
        default:
            break;
    }
}

// --- State Handlers ---

void SystemController::handleBoot() {
    // Handled in begin()
}

void SystemController::handleWifiConnect() {
    static bool wifiStarted = false;

    if (!wifiStarted) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        wifiStarted = true;
        Serial.println("[WIFI] Connecting...");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected: %s\n", WiFi.localIP().toString().c_str());
        wifiStarted = false;
        transitionTo(SystemState::NEEDS_HOMING);
    } else if (millis() - _stateEnteredAt > 10000 && WIFI_AP_FALLBACK) {
        // Fallback to AP mode after 10s
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        Serial.printf("[WIFI] AP mode: %s\n", WiFi.softAPIP().toString().c_str());
        wifiStarted = false;
        transitionTo(SystemState::NEEDS_HOMING);
    }
}

void SystemController::handleNeedsHoming() {
    // Waiting for home command from button or WebUI
}

void SystemController::handleHoming() {
    auto phase = _stepper->updateHoming();

    if (phase == StepperMotion::HomingPhase::COMPLETE) {
        Serial.println("[SYS] Homing complete");
        transitionTo(SystemState::IDLE);
    } else if (phase == StepperMotion::HomingPhase::FAILED) {
        _errorMessage = "Homing failed: limit switch not found";
        transitionTo(SystemState::ERROR);
    }
}

void SystemController::handleIdle() {
    // Waiting for commands
}

void SystemController::handleUnlocking() {
    if (_latch->isUnlocked()) {
        // If we have a pending target, start moving
        if (_targetPositionMM > 0) {
            _stepper->moveToMM(_targetPositionMM);
            transitionTo(SystemState::MOVING);
        } else {
            transitionTo(SystemState::IDLE);
        }
    }
}

void SystemController::handleMoving() {
    if (!_stepper->isMoving()) {
        transitionTo(SystemState::SETTLING);
    }

    // Timeout guard
    if (millis() - _stateEnteredAt > MOVE_TIMEOUT_MS) {
        _stepper->stop();
        _errorMessage = "Move timeout";
        transitionTo(SystemState::ERROR);
    }
}

void SystemController::handleSettling() {
    // Check if DRO position matches target within tolerance
    float currentPos = _dro->getPositionMM();
    float error = abs(currentPos - _targetPositionMM);

    if (error <= POSITION_TOLERANCE_MM) {
        // Position confirmed — lock it
        _latch->lock();
        transitionTo(SystemState::LOCKING);
    } else if (millis() - _stateEnteredAt > SETTLING_TIMEOUT_MS) {
        _errorMessage = String("Position error: ") + String(error, 2) + "mm";
        transitionTo(SystemState::ERROR);
    }
}

void SystemController::handleLocking() {
    if (_latch->isLocked()) {
        Serial.printf("[SYS] Locked at %.2fmm\n", _dro->getPositionMM());
        transitionTo(SystemState::LOCKED);
    }
}

void SystemController::handleLocked() {
    // Waiting for cut command, new position, or unlock
}

void SystemController::handleDustSpinup() {
    if (_dust->isSpunUp()) {
        transitionTo(SystemState::CUTTING);
    }
}

void SystemController::handleCutting() {
    // Waiting for cut-complete signal from user
}

void SystemController::handleError() {
    // Waiting for reset command
}

void SystemController::handleEstop() {
    // Waiting for E-stop release and reset
}
