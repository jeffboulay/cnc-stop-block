#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class SystemController;

class WebUI {
public:
    WebUI(SystemController* controller, uint16_t port = 80);

    void begin();
    void update();

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    SystemController* _controller;
    unsigned long _lastBroadcastMs = 0;

    void setupRoutes();
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);
    void handleWSMessage(AsyncWebSocketClient* client, const String& message);
    void broadcastStatus();
    String buildStatusJSON();
};
