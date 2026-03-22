#include "WebUI.h"
#include "SystemController.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

WebUI::WebUI(SystemController* controller, uint16_t port)
    : _server(port), _ws("/ws"), _controller(controller) {}

void WebUI::begin() {
    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        onWebSocketEvent(server, client, type, arg, data, len);
    });

    _server.addHandler(&_ws);
    setupRoutes();
    _server.begin();
    Serial.println("[WEB] Server started on port 80");
}

void WebUI::update() {
    _ws.cleanupClients();

    if (millis() - _lastBroadcastMs >= WS_UPDATE_INTERVAL_MS) {
        _lastBroadcastMs = millis();
        broadcastStatus();
    }
}

void WebUI::setupRoutes() {
    // Serve static files from LittleFS
    _server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    // REST API
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json", buildStatusJSON());
    });

    _server.on("/api/home", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandHome();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/estop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandEStop();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandResetError();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/lock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandLock();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/unlock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandUnlock();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/cut/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandStartCut();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/cut/end", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandEndCut();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/next", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandNextCut();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/cutlist", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json", _controller->getCutList().toJSON());
    });

    // JSON body handlers need AsyncCallbackJsonWebHandler or manual body handling
    // For goto, jog, cutlist POST, and tool registration we use WebSocket commands
}

void WebUI::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                              AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                          client->remoteIP().toString().c_str());
            // Send immediate status
            client->text(buildStatusJSON());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
                String message = String((char*)data, len);
                handleWSMessage(client, message);
            }
            break;
        }

        default:
            break;
    }
}

void WebUI::handleWSMessage(AsyncWebSocketClient* client, const String& message) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, message);
    if (err) {
        client->text("{\"error\":\"Invalid JSON\"}");
        return;
    }

    String cmd = doc["cmd"].as<String>();

    if (cmd == "goto") {
        float pos = doc["position_mm"].as<float>();
        _controller->commandGoTo(pos);
    } else if (cmd == "jog") {
        float dist = doc["distance_mm"].as<float>();
        _controller->commandJog(dist);
    } else if (cmd == "home") {
        _controller->commandHome();
    } else if (cmd == "lock") {
        _controller->commandLock();
    } else if (cmd == "unlock") {
        _controller->commandUnlock();
    } else if (cmd == "estop") {
        _controller->commandEStop();
    } else if (cmd == "reset") {
        _controller->commandResetError();
    } else if (cmd == "cut_start") {
        _controller->commandStartCut();
    } else if (cmd == "cut_end") {
        _controller->commandEndCut();
    } else if (cmd == "next_cut") {
        _controller->commandNextCut();
    } else if (cmd == "cutlist_update") {
        String json = doc["data"].as<String>();
        _controller->getCutList().fromJSON(json);
    } else if (cmd == "cutlist_clear") {
        _controller->getCutList().clearAll();
    } else if (cmd == "cutlist_reset") {
        _controller->getCutList().reset();
    } else {
        client->text("{\"error\":\"Unknown command\"}");
    }
}

void WebUI::broadcastStatus() {
    if (_ws.count() == 0) return;
    _ws.textAll(buildStatusJSON());
}

String WebUI::buildStatusJSON() {
    JsonDocument doc;

    doc["state"] = systemStateToString(_controller->getState());
    doc["pos_mm"] = serialized(String(_controller->getCurrentPositionMM(), 2));
    doc["target_mm"] = serialized(String(_controller->getTargetPositionMM(), 2));
    doc["homed"] = _controller->isHomed();
    doc["locked"] = _controller->isLocked();
    doc["dust"] = _controller->isDustActive();
    doc["clamps"] = _controller->areClampsEngaged();

    if (_controller->isToolPresent()) {
        JsonObject tool = doc["tool"].to<JsonObject>();
        ToolInfo t = _controller->getActiveTool();
        tool["name"] = t.name;
        tool["kerf_mm"] = t.kerfMM;
        tool["uid"] = t.uid;
    } else {
        doc["tool"] = nullptr;
    }

    int nextIdx = _controller->getCutList().getNextIndex();
    doc["cutlist_idx"] = nextIdx;
    doc["cutlist_size"] = _controller->getCutList().size();

    if (_controller->getState() == SystemState::ERROR) {
        doc["error"] = _controller->getErrorMessage();
    }

    doc["uptime_s"] = millis() / 1000;

    String output;
    serializeJson(doc, output);
    return output;
}
