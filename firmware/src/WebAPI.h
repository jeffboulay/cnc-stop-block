#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "config.h"

class SystemController;

// ---------------------------------------------------------------------------
// RateLimitEntry
// One slot in the fixed-size IP tracking table.  Tracks the timestamp of the
// last accepted mutating request from a given IP address.
// ---------------------------------------------------------------------------
struct RateLimitEntry {
    uint32_t      ipv4;          // Packed IPv4 (0 = empty slot)
    unsigned long lastCommandMs; // millis() of last accepted command
};

class WebAPI {
public:
    WebAPI(SystemController* controller, uint16_t port = 80);

    void begin();
    void update();

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    SystemController* _controller;
    unsigned long _lastBroadcastMs = 0;

    // --- Authentication (Step 1.1 – 1.3) ---
    String _authToken;

    void   loadOrCreateToken();
    String generateToken() const;
    bool   isAuthenticated(AsyncWebServerRequest* request) const;

    // --- Rate Limiting (Step 3.1 – 3.2) ---
    // Fixed-size table; oldest entry evicted when full (LRU approximation).
    RateLimitEntry _rateTable[RATE_LIMIT_TABLE_SIZE] = {};

    // Returns true and sends HTTP 429 if |request| exceeds the rate limit.
    // Returns false (allowed) otherwise and records the request timestamp.
    bool isRateLimited(AsyncWebServerRequest* request);

    // Collected body for JSON POST requests (capped at MAX_POST_BODY_BYTES)
    String _bodyBuffer;
    bool   _bodyOverflow = false;

    void setupRoutes();
    void addCORSHeaders(AsyncWebServerResponse* response);
    void addSecurityHeaders(AsyncWebServerResponse* response);
    void sendJSON(AsyncWebServerRequest* request, int code, const String& json);
    void sendOK(AsyncWebServerRequest* request);
    void sendError(AsyncWebServerRequest* request, int code, const String& message);

    // Body accumulator with size cap
    void onBodyChunk(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);
    void broadcastStatus();
    String buildStatusJSON();

    // Input validation helpers
    bool isValidFloat(float v) const;
    String truncateString(const String& s, int maxLen) const;
};
