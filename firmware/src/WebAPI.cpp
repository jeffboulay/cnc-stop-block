#include "WebAPI.h"
#include "SystemController.h"
#include <ArduinoJson.h>

WebAPI::WebAPI(SystemController* controller, uint16_t port)
    : _server(port), _ws("/ws"), _controller(controller) {}

void WebAPI::begin() {
    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        onWebSocketEvent(server, client, type, arg, data, len);
    });

    _server.addHandler(&_ws);
    setupRoutes();

    // Handle CORS preflight for all routes
    _server.onNotFound([this](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse* response = request->beginResponse(204);
            addCORSHeaders(response);
            request->send(response);
        } else {
            sendError(request, 404, "Not found");
        }
    });

    _server.begin();
    Serial.println("[API] Server started on port 80");
}

void WebAPI::update() {
    _ws.cleanupClients();

    if (millis() - _lastBroadcastMs >= WS_UPDATE_INTERVAL_MS) {
        _lastBroadcastMs = millis();
        broadcastStatus();
    }
}

void WebAPI::addCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void WebAPI::sendJSON(AsyncWebServerRequest* request, int code, const String& json) {
    AsyncWebServerResponse* response = request->beginResponse(code, "application/json", json);
    addCORSHeaders(response);
    request->send(response);
}

void WebAPI::sendOK(AsyncWebServerRequest* request) {
    sendJSON(request, 200, "{\"ok\":true}");
}

void WebAPI::sendError(AsyncWebServerRequest* request, int code, const String& message) {
    sendJSON(request, code, "{\"error\":\"" + message + "\"}");
}

void WebAPI::setupRoutes() {
    // --- Status ---
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        sendJSON(request, 200, buildStatusJSON());
    });

    // --- Simple POST commands (no body needed) ---
    _server.on("/api/home", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandHome();
        sendOK(request);
    });

    _server.on("/api/estop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandEStop();
        sendOK(request);
    });

    _server.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandResetError();
        sendOK(request);
    });

    _server.on("/api/lock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandLock();
        sendOK(request);
    });

    _server.on("/api/unlock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandUnlock();
        sendOK(request);
    });

    _server.on("/api/cut/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandStartCut();
        sendOK(request);
    });

    _server.on("/api/cut/end", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandEndCut();
        sendOK(request);
    });

    _server.on("/api/cut/next", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->commandNextCut();
        sendOK(request);
    });

    // --- POST with JSON body: goto ---
    _server.on("/api/goto", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // Response handler — called after body is received
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || doc["position_mm"].isNull()) {
                sendError(request, 400, "Invalid JSON: requires position_mm");
                return;
            }
            _controller->commandGoTo(doc["position_mm"].as<float>());
            sendOK(request);
        },
        nullptr, // upload handler
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) _bodyBuffer = "";
            _bodyBuffer += String((char*)data, len);
        }
    );

    // --- POST with JSON body: jog ---
    _server.on("/api/jog", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || doc["distance_mm"].isNull()) {
                sendError(request, 400, "Invalid JSON: requires distance_mm");
                return;
            }
            _controller->commandJog(doc["distance_mm"].as<float>());
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) _bodyBuffer = "";
            _bodyBuffer += String((char*)data, len);
        }
    );

    // --- Cut List CRUD ---
    _server.on("/api/cutlist", HTTP_GET, [this](AsyncWebServerRequest* request) {
        sendJSON(request, 200, _controller->getCutList().toJSON());
    });

    // Replace entire cut list
    _server.on("/api/cutlist", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            _controller->getCutList().fromJSON(_bodyBuffer);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) _bodyBuffer = "";
            _bodyBuffer += String((char*)data, len);
        }
    );

    // Add single cut
    _server.on("/api/cutlist/add", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || doc["length_mm"].isNull()) {
                sendError(request, 400, "Invalid JSON: requires length_mm");
                return;
            }
            CutEntry entry;
            entry.label = doc["label"] | "Untitled";
            entry.lengthMM = doc["length_mm"].as<float>();
            entry.quantity = doc["quantity"] | 1;
            entry.completed = false;
            _controller->getCutList().addEntry(entry);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) _bodyBuffer = "";
            _bodyBuffer += String((char*)data, len);
        }
    );

    _server.on("/api/cutlist/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->getCutList().clearAll();
        sendOK(request);
    });

    _server.on("/api/cutlist/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _controller->getCutList().reset();
        sendOK(request);
    });

    // Delete cut by index: /api/cutlist/0, /api/cutlist/1, etc.
    // ESPAsyncWebServer doesn't support path params, so we use a catch-all pattern
    _server.on("^\\/api\\/cutlist\\/(\\d+)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        int index = request->pathArg(0).toInt();
        if (index < 0 || index >= _controller->getCutList().size()) {
            sendError(request, 404, "Cut index out of range");
            return;
        }
        _controller->getCutList().removeEntry(index);
        sendOK(request);
    });

    // --- Tool Registry ---
    _server.on("/api/tools", HTTP_GET, [this](AsyncWebServerRequest* request) {
        auto tools = _controller->getRFIDReader().getAllTools();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& t : tools) {
            JsonObject obj = arr.add<JsonObject>();
            obj["uid"] = t.uid;
            obj["name"] = t.name;
            obj["kerf_mm"] = t.kerfMM;
        }
        String output;
        serializeJson(doc, output);
        sendJSON(request, 200, output);
    });

    // Register / update tool
    _server.on("/api/tools", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || doc["uid"].isNull() || doc["name"].isNull()) {
                sendError(request, 400, "Invalid JSON: requires uid, name");
                return;
            }
            ToolInfo tool;
            tool.uid = doc["uid"].as<String>();
            tool.name = doc["name"].as<String>();
            tool.kerfMM = doc["kerf_mm"] | 0.0f;
            _controller->getRFIDReader().registerTool(tool);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) _bodyBuffer = "";
            _bodyBuffer += String((char*)data, len);
        }
    );

    // Delete tool by UID
    _server.on("^\\/api\\/tools\\/([A-Fa-f0-9]+)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        String uid = request->pathArg(0);
        uid.toUpperCase();
        _controller->getRFIDReader().removeTool(uid);
        sendOK(request);
    });
}

// --- WebSocket ---

void WebAPI::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                          client->remoteIP().toString().c_str());
            client->text(buildStatusJSON());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            break;

        default:
            break;
    }
}

void WebAPI::broadcastStatus() {
    if (_ws.count() == 0) return;
    _ws.textAll(buildStatusJSON());
}

String WebAPI::buildStatusJSON() {
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
