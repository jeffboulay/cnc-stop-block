#pragma once

#include <Arduino.h>
#include <MFRC522.h>
#include <vector>
#include "config.h"

struct ToolInfo {
    String uid;
    String name;
    float  kerfMM;
};

class RFIDReader {
public:
    RFIDReader(uint8_t ssPin, uint8_t rstPin);

    void begin();
    void update();

    bool isToolPresent() const;
    ToolInfo getCurrentTool() const;
    void registerTool(const ToolInfo& tool);
    void removeTool(const String& uid);
    std::vector<ToolInfo> getAllTools() const;

private:
    MFRC522 _rfid;
    unsigned long _lastPollMs = 0;
    ToolInfo _currentTool;
    bool _toolPresent = false;
    std::vector<ToolInfo> _toolRegistry;

    void loadRegistry();
    void saveRegistry();
    String uidToString(byte* uid, byte size);
    ToolInfo* findTool(const String& uid);
};
