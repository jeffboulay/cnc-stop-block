#include "RFIDReader.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* TOOLS_FILE = "/tools.json";

RFIDReader::RFIDReader(uint8_t ssPin, uint8_t rstPin)
    : _rfid(ssPin, rstPin) {}

void RFIDReader::begin() {
    SPI.begin();
    _rfid.PCD_Init();
    loadRegistry();
}

void RFIDReader::update() {
    if (millis() - _lastPollMs < RFID_POLL_INTERVAL_MS) return;
    _lastPollMs = millis();

    if (!_rfid.PICC_IsNewCardPresent() || !_rfid.PICC_ReadCardSerial()) {
        // No card detected — mark tool as absent after a few missed polls
        // Keep current tool info for one extra cycle to avoid flicker
        _toolPresent = false;
        return;
    }

    String uid = uidToString(_rfid.uid.uidByte, _rfid.uid.size);
    _rfid.PICC_HaltA();

    ToolInfo* known = findTool(uid);
    if (known) {
        _currentTool = *known;
    } else {
        _currentTool = { uid, "Unknown Tool", 0.0f };
    }
    _toolPresent = true;
}

bool RFIDReader::isToolPresent() const {
    return _toolPresent;
}

ToolInfo RFIDReader::getCurrentTool() const {
    return _currentTool;
}

void RFIDReader::registerTool(const ToolInfo& tool) {
    // Update existing or add new
    for (auto& t : _toolRegistry) {
        if (t.uid == tool.uid) {
            t.name = tool.name;
            t.kerfMM = tool.kerfMM;
            saveRegistry();
            return;
        }
    }
    _toolRegistry.push_back(tool);
    saveRegistry();
}

void RFIDReader::removeTool(const String& uid) {
    for (auto it = _toolRegistry.begin(); it != _toolRegistry.end(); ++it) {
        if (it->uid == uid) {
            _toolRegistry.erase(it);
            saveRegistry();
            return;
        }
    }
}

std::vector<ToolInfo> RFIDReader::getAllTools() const {
    return _toolRegistry;
}

void RFIDReader::loadRegistry() {
    _toolRegistry.clear();

    File file = LittleFS.open(TOOLS_FILE, "r");
    if (!file) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        ToolInfo tool;
        tool.uid = obj["uid"].as<String>();
        tool.name = obj["name"].as<String>();
        tool.kerfMM = obj["kerf_mm"].as<float>();
        _toolRegistry.push_back(tool);
    }
}

void RFIDReader::saveRegistry() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& tool : _toolRegistry) {
        JsonObject obj = arr.add<JsonObject>();
        obj["uid"] = tool.uid;
        obj["name"] = tool.name;
        obj["kerf_mm"] = tool.kerfMM;
    }

    File file = LittleFS.open(TOOLS_FILE, "w");
    if (!file) return;
    serializeJson(doc, file);
    file.close();
}

String RFIDReader::uidToString(byte* uid, byte size) {
    String s;
    for (byte i = 0; i < size; i++) {
        if (uid[i] < 0x10) s += "0";
        s += String(uid[i], HEX);
    }
    s.toUpperCase();
    return s;
}

ToolInfo* RFIDReader::findTool(const String& uid) {
    for (auto& t : _toolRegistry) {
        if (t.uid == uid) return &t;
    }
    return nullptr;
}
