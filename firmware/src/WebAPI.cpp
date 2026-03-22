#include "WebAPI.h"
#include "SystemController.h"
#include <ArduinoJson.h>
#include <math.h>
#include <esp_random.h>

WebAPI::WebAPI(SystemController* controller, uint16_t port)
    : _server(port), _ws("/ws"), _controller(controller) {}

void WebAPI::begin() {
    loadOrCreateToken();

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

// --- Authentication (Step 1.1 – 1.3) ---

String WebAPI::generateToken() const {
    // 8 × 32-bit random values → 64-character lowercase hex token
    String token;
    token.reserve(64);
    for (int i = 0; i < 8; i++) {
        uint32_t r = esp_random();
        char buf[9];
        snprintf(buf, sizeof(buf), "%08x", r);
        token += buf;
    }
    return token;
}

void WebAPI::loadOrCreateToken() {
    // Mount filesystem (no-op if already mounted by CutList or similar)
    if (!LittleFS.begin(false)) {
        // Filesystem unavailable — generate an ephemeral token for this boot
        _authToken = generateToken();
        Serial.println("[AUTH] WARNING: LittleFS unavailable. Ephemeral token (lost on reboot):");
        Serial.println("[AUTH] " + _authToken);
        return;
    }

    if (LittleFS.exists(AUTH_TOKEN_PATH)) {
        File f = LittleFS.open(AUTH_TOKEN_PATH, "r");
        if (f) {
            String stored = f.readString();
            f.close();
            stored.trim();
            if (stored.length() == 64) {
                _authToken = stored;
                Serial.println("[AUTH] Token loaded from flash.");
                return;
            }
        }
    }

    // First boot — generate and persist a new token
    _authToken = generateToken();
    File f = LittleFS.open(AUTH_TOKEN_PATH, "w");
    if (f) {
        f.print(_authToken);
        f.close();
    }
    Serial.println("[AUTH] *** NEW API TOKEN GENERATED ***");
    Serial.println("[AUTH] Enter this token in the UI pairing screen:");
    Serial.println("[AUTH] " + _authToken);
    Serial.println("[AUTH] ************************************");
}

bool WebAPI::isAuthenticated(AsyncWebServerRequest* request) const {
    // Step 1.2: Optionally exempt GET /api/status for read-only monitoring
    if (AUTH_EXEMPT_STATUS &&
        request->url() == "/api/status" &&
        request->method() == HTTP_GET) {
        return true;
    }

    if (!request->hasHeader("Authorization")) return false;

    String authHeader = request->getHeader("Authorization")->value();
    if (!authHeader.startsWith("Bearer ")) return false;

    String provided = authHeader.substring(7);
    provided.trim();
    return provided == _authToken;
}

// --- Response Helpers ---

void WebAPI::addCORSHeaders(AsyncWebServerResponse* response) {
    // Step 1.4: No wildcard default. Cross-origin access requires an explicit
    // CORS_ORIGIN build flag (e.g. -DCORS_ORIGIN=\"http://192.168.1.100:5173\").
#ifdef CORS_ORIGIN
    response->addHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
#endif
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void WebAPI::addSecurityHeaders(AsyncWebServerResponse* response) {
    // #13: Security response headers
    response->addHeader("X-Content-Type-Options", "nosniff");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader("Cache-Control", "no-store");
}

void WebAPI::sendJSON(AsyncWebServerRequest* request, int code, const String& json) {
    AsyncWebServerResponse* response = request->beginResponse(code, "application/json", json);
    addCORSHeaders(response);
    addSecurityHeaders(response);
    request->send(response);
}

void WebAPI::sendOK(AsyncWebServerRequest* request) {
    sendJSON(request, 200, "{\"ok\":true}");
}

void WebAPI::sendError(AsyncWebServerRequest* request, int code, const String& message) {
    // #9: Build error JSON with ArduinoJson to prevent injection
    JsonDocument doc;
    doc["error"] = message;
    String output;
    serializeJson(doc, output);
    sendJSON(request, code, output);
}

// --- Body Accumulator (#4: size cap) ---

void WebAPI::onBodyChunk(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        _bodyBuffer = "";
        _bodyOverflow = false;
    }
    if (_bodyOverflow) return;
    if (_bodyBuffer.length() + len > MAX_POST_BODY_BYTES) {
        _bodyOverflow = true;
        _bodyBuffer = "";
        return;
    }
    _bodyBuffer += String((char*)data, len);
}

// --- Input Validation Helpers (#7) ---

bool WebAPI::isValidFloat(float v) const {
    return !isnan(v) && !isinf(v);
}

String WebAPI::truncateString(const String& s, int maxLen) const {
    if (s.length() <= (unsigned int)maxLen) return s;
    return s.substring(0, maxLen);
}

// --- Routes ---

void WebAPI::setupRoutes() {
    // --- Status (optionally auth-exempt — see AUTH_EXEMPT_STATUS) ---
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        sendJSON(request, 200, buildStatusJSON());
    });

    // --- Token rotation (Step 1.7) ---
    _server.on("/api/token/rotate", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _authToken = generateToken();
        if (LittleFS.begin(false)) {
            File f = LittleFS.open(AUTH_TOKEN_PATH, "w");
            if (f) { f.print(_authToken); f.close(); }
        }
        Serial.println("[AUTH] Token rotated. New token:");
        Serial.println("[AUTH] " + _authToken);
        sendJSON(request, 200, "{\"token\":\"" + _authToken + "\"}");
    });

    // --- Simple POST commands (no body needed) ---
    _server.on("/api/home", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandHome();
        sendOK(request);
    });

    _server.on("/api/estop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandEStop();
        sendOK(request);
    });

    _server.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandResetError();
        sendOK(request);
    });

    _server.on("/api/lock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandLock();
        sendOK(request);
    });

    _server.on("/api/unlock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandUnlock();
        sendOK(request);
    });

    _server.on("/api/cut/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandStartCut();
        sendOK(request);
    });

    _server.on("/api/cut/end", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandEndCut();
        sendOK(request);
    });

    _server.on("/api/cut/next", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->commandNextCut();
        sendOK(request);
    });

    // --- POST with JSON body: goto (#7: validate type, range, NaN) ---
    _server.on("/api/goto", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
            if (_bodyOverflow) { sendError(request, 413, "Request body too large"); return; }
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || !doc["position_mm"].is<float>()) {
                sendError(request, 400, "Invalid JSON: requires position_mm (number)");
                return;
            }
            float pos = doc["position_mm"].as<float>();
            if (!isValidFloat(pos) || pos < 0 || pos > MAX_TRAVEL_MM) {
                sendError(request, 400, "position_mm out of range (0 - " + String(MAX_TRAVEL_MM, 0) + ")");
                return;
            }
            _controller->commandGoTo(pos);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) { onBodyChunk(r, d, l, i, t); }
    );

    // --- POST with JSON body: jog (#7: validate) ---
    _server.on("/api/jog", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
            if (_bodyOverflow) { sendError(request, 413, "Request body too large"); return; }
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || !doc["distance_mm"].is<float>()) {
                sendError(request, 400, "Invalid JSON: requires distance_mm (number)");
                return;
            }
            float dist = doc["distance_mm"].as<float>();
            if (!isValidFloat(dist) || dist < -MAX_TRAVEL_MM || dist > MAX_TRAVEL_MM) {
                sendError(request, 400, "distance_mm out of range");
                return;
            }
            _controller->commandJog(dist);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) { onBodyChunk(r, d, l, i, t); }
    );

    // --- Cut List CRUD ---
    _server.on("/api/cutlist", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        sendJSON(request, 200, _controller->getCutList().toJSON());
    });

    // Replace entire cut list
    _server.on("/api/cutlist", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
            if (_bodyOverflow) { sendError(request, 413, "Request body too large"); return; }
            _controller->getCutList().fromJSON(_bodyBuffer);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) { onBodyChunk(r, d, l, i, t); }
    );

    // Add single cut (#7: validate, #8: cap size)
    _server.on("/api/cutlist/add", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
            if (_bodyOverflow) { sendError(request, 413, "Request body too large"); return; }

            // #8: Check size cap
            if (_controller->getCutList().size() >= MAX_CUTS) {
                sendError(request, 409, "Cut list full (max " + String(MAX_CUTS) + ")");
                return;
            }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || !doc["length_mm"].is<float>()) {
                sendError(request, 400, "Invalid JSON: requires length_mm (number)");
                return;
            }

            float lengthMM = doc["length_mm"].as<float>();
            if (!isValidFloat(lengthMM) || lengthMM <= 0 || lengthMM > MAX_TRAVEL_MM) {
                sendError(request, 400, "length_mm out of range (0 - " + String(MAX_TRAVEL_MM, 0) + ")");
                return;
            }

            int qty = doc["quantity"] | 1;
            if (qty < 1 || qty > 999) {
                sendError(request, 400, "quantity out of range (1 - 999)");
                return;
            }

            CutEntry entry;
            entry.label = truncateString(doc["label"] | "Untitled", MAX_STRING_LENGTH);
            entry.lengthMM = lengthMM;
            entry.quantity = qty;
            entry.completed = false;
            _controller->getCutList().addEntry(entry);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) { onBodyChunk(r, d, l, i, t); }
    );

    // #10: Guard cutlist mutations during CUTTING state
    _server.on("/api/cutlist/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        if (_controller->getState() == SystemState::CUTTING) {
            sendError(request, 409, "Cannot modify cut list during active cut");
            return;
        }
        _controller->getCutList().clearAll();
        sendOK(request);
    });

    _server.on("/api/cutlist/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        _controller->getCutList().reset();
        sendOK(request);
    });

    // Delete cut by index
    _server.on("^\\/api\\/cutlist\\/(\\d+)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        if (_controller->getState() == SystemState::CUTTING) {
            sendError(request, 409, "Cannot modify cut list during active cut");
            return;
        }
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
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
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

    // Register / update tool (#7: validate, #8: cap size)
    _server.on("/api/tools", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
            if (_bodyOverflow) { sendError(request, 413, "Request body too large"); return; }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _bodyBuffer);
            if (err || !doc["uid"].is<const char*>() || !doc["name"].is<const char*>()) {
                sendError(request, 400, "Invalid JSON: requires uid (string), name (string)");
                return;
            }

            String uid = doc["uid"].as<String>();
            String name = doc["name"].as<String>();

            if (uid.length() == 0 || uid.length() > MAX_STRING_LENGTH) {
                sendError(request, 400, "uid must be 1-" + String(MAX_STRING_LENGTH) + " characters");
                return;
            }

            // #8: Check tool registry cap (allow updates to existing tools)
            auto existing = _controller->getRFIDReader().getAllTools();
            bool isUpdate = false;
            for (const auto& t : existing) {
                if (t.uid == uid) { isUpdate = true; break; }
            }
            if (!isUpdate && (int)existing.size() >= MAX_TOOLS) {
                sendError(request, 409, "Tool registry full (max " + String(MAX_TOOLS) + ")");
                return;
            }

            float kerfMM = doc["kerf_mm"] | 0.0f;
            if (!isValidFloat(kerfMM) || kerfMM < 0 || kerfMM > 20.0f) {
                sendError(request, 400, "kerf_mm out of range (0 - 20)");
                return;
            }

            ToolInfo tool;
            tool.uid = uid;
            tool.name = truncateString(name, MAX_STRING_LENGTH);
            tool.kerfMM = kerfMM;
            _controller->getRFIDReader().registerTool(tool);
            sendOK(request);
        },
        nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) { onBodyChunk(r, d, l, i, t); }
    );

    // Delete tool by UID
    _server.on("^\\/api\\/tools\\/([A-Fa-f0-9]+)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { sendError(request, 401, "Unauthorized"); return; }
        String uid = request->pathArg(0);
        uid.toUpperCase();
        _controller->getRFIDReader().removeTool(uid);
        sendOK(request);
    });
}

// --- WebSocket (#5: connection limit, Step 1.3: token validation) ---

void WebAPI::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT: {
            // Step 1.3: Validate token from query string (?token=<hex>)
            String url = client->url();
            int tokenPos = url.indexOf("token=");
            if (tokenPos < 0) {
                Serial.printf("[WS] Rejecting client #%u — missing token\n", client->id());
                client->close();
                return;
            }
            String provided = url.substring(tokenPos + 6);
            int ampPos = provided.indexOf('&');
            if (ampPos >= 0) provided = provided.substring(0, ampPos);
            provided.trim();
            if (provided != _authToken) {
                Serial.printf("[WS] Rejecting client #%u — invalid token\n", client->id());
                client->close();
                return;
            }

            if (_ws.count() > WS_MAX_CLIENTS) {
                Serial.printf("[WS] Rejecting client #%u — max connections reached\n", client->id());
                client->close();
                return;
            }
            Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                          client->remoteIP().toString().c_str());
            client->text(buildStatusJSON());
            break;
        }

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
