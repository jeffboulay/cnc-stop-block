#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class SystemController;

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

    // Collected body for JSON POST requests
    String _bodyBuffer;

    void setupRoutes();
    void addCORSHeaders(AsyncWebServerResponse* response);
    void sendJSON(AsyncWebServerRequest* request, int code, const String& json);
    void sendOK(AsyncWebServerRequest* request);
    void sendError(AsyncWebServerRequest* request, int code, const String& message);

    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);
    void broadcastStatus();
    String buildStatusJSON();
};
