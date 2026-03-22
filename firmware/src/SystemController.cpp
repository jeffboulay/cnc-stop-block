#include "SystemController.h"
#include <WiFi.h>
#include <LittleFS.h>

const char* systemStateToString(SystemState state) {
    switch (state) {
        case SystemState::BOOT:         return "BOOT";
        case SystemState::WIFI_CONNECT: return "WIFI_CONNECT";
        case SystemState::PROVISIONING: return "PROVISIONING";
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
    _buttons      = new ButtonPanel(PIN_BTN_GO, PIN_BTN_HOME, PIN_BTN_LOCK, PIN_BTN_ESTOP);
    _cutList      = new CutList();
    _provisioning = new WiFiProvisioning();
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
    delete _provisioning;
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

    // Must run after lights and LittleFS are ready, before WiFi connects
    checkFactoryReset();

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
        case SystemState::PROVISIONING: handleProvisioning(); break;
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
        case SystemState::PROVISIONING:
            _lights->setPattern(LightPattern::SLOW_PULSE_BLUE);
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

void SystemController::checkFactoryReset() {
    // E-Stop is INPUT_PULLUP (set by ButtonPanel::begin()). Active LOW.
    // Hold for FACTORY_RESET_HOLD_MS to wipe credentials and auth token.
    if (digitalRead(PIN_BTN_ESTOP) != LOW) return;

    Serial.printf("[SYS] E-Stop held — factory reset in %.0f s (release to cancel)\n",
                  FACTORY_RESET_HOLD_MS / 1000.0f);

    _lights->setPattern(LightPattern::FLASH_RED);
    unsigned long holdStart = millis();

    while (digitalRead(PIN_BTN_ESTOP) == LOW) {
        _lights->update();

        if (millis() - holdStart >= FACTORY_RESET_HOLD_MS) {
            Serial.println("[SYS] *** FACTORY RESET TRIGGERED ***");
            WiFiProvisioning::clearCredentials();
            if (LittleFS.exists(AUTH_TOKEN_PATH)) {
                LittleFS.remove(AUTH_TOKEN_PATH);
                Serial.println("[SYS] Auth token cleared");
            }
            // Brief confirmation flash before reboot
            for (int i = 0; i < 6; i++) {
                _lights->setPattern(i % 2 ? LightPattern::SOLID_RED : LightPattern::OFF);
                _lights->update();
                delay(250);
            }
            Serial.println("[SYS] Factory reset complete — rebooting");
            ESP.restart();
        }
        delay(20);
    }

    Serial.println("[SYS] Factory reset cancelled (E-Stop released)");
    _lights->setPattern(LightPattern::CHASE_WHITE);
}

void SystemController::handleWifiConnect() {
    static bool wifiStarted = false;

    if (!wifiStarted) {
        // If no credentials are stored, go straight to AP provisioning
        if (!WiFiProvisioning::hasCredentials()) {
            Serial.println("[WIFI] No credentials stored — entering provisioning mode");
            wifiStarted = false;
            _provisioning->beginAP();
            transitionTo(SystemState::PROVISIONING);
            return;
        }

        // Load credentials and start STA connection
        WiFiCredentials creds;
        if (!WiFiProvisioning::loadCredentials(creds)) {
            Serial.println("[WIFI] Failed to read credentials — entering provisioning mode");
            _provisioning->beginAP();
            transitionTo(SystemState::PROVISIONING);
            return;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
        wifiStarted = true;
        Serial.printf("[WIFI] Connecting to %s...\n", creds.ssid.c_str());
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected: %s\n", WiFi.localIP().toString().c_str());
        wifiStarted = false;
        transitionTo(SystemState::NEEDS_HOMING);
    } else if (millis() - _stateEnteredAt > WIFI_CONNECT_TIMEOUT_MS) {
        // Connection timed out — fall back to provisioning so the user can
        // correct their credentials without needing a serial console
        Serial.println("[WIFI] Connection timed out — entering provisioning mode");
        WiFi.disconnect();
        wifiStarted = false;
        _provisioning->beginAP();
        transitionTo(SystemState::PROVISIONING);
    }
}

void SystemController::handleProvisioning() {
    // Pumps the DNS server; triggers ESP.restart() when credentials are saved
    _provisioning->update();
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
    // Tick approach zone — drops to APPROACH_SPEED_MM_S within APPROACH_ZONE_MM
    _stepper->update();

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
