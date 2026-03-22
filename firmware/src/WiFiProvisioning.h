#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// Forward declarations — avoid pulling heavy headers into every TU
class AsyncWebServer;
class DNSServer;

// ---------------------------------------------------------------------------
// WiFiCredentials
// Persisted to LittleFS at WIFI_CONFIG_PATH as a small JSON object.
// ---------------------------------------------------------------------------
struct WiFiCredentials {
    String ssid;
    String password;
};

// ---------------------------------------------------------------------------
// WiFiProvisioning
//
// Handles two responsibilities:
//   1. Credential storage: load/save/clear/check WiFi credentials in LittleFS.
//      These methods are static so they can be called without an instance
//      (e.g. from SystemController::begin() for factory-reset purposes).
//
//   2. Captive-portal AP: when no credentials exist (or connection fails),
//      start a SoftAP + DNS server + minimal web portal so the user can
//      enter their SSID and password from any phone or laptop.
//
// Usage flow:
//   1. Call hasCredentials() — if false, call beginAP() then loop update().
//   2. Call loadCredentials() — connect to WiFi normally.
//   3. If connection times out, call beginAP() then loop update().
//   4. update() returns true the instant new credentials are saved;
//      the caller (SystemController) should then call ESP.restart().
// ---------------------------------------------------------------------------
class WiFiProvisioning {
public:
    WiFiProvisioning();
    ~WiFiProvisioning();

    // ---- Static helpers (no instance needed) ----

    // Returns true if /wifi_config.json exists and contains a non-empty SSID.
    static bool hasCredentials();

    // Fills |out| from /wifi_config.json. Returns false on any error.
    static bool loadCredentials(WiFiCredentials& out);

    // Writes |creds| to /wifi_config.json. Returns false on write error.
    static bool saveCredentials(const WiFiCredentials& creds);

    // Deletes /wifi_config.json. Safe to call even if the file doesn't exist.
    static void clearCredentials();

    // ---- AP + captive portal ----

    // Start SoftAP (MAC-derived password), scan networks, launch DNS + portal.
    // Call once when entering PROVISIONING state.
    void beginAP();

    // Pump DNS requests and check for reboot trigger.
    // Returns true when credentials have been saved — caller should reboot.
    // Call every loop() tick during PROVISIONING state.
    bool update();

private:
    AsyncWebServer* _server;
    DNSServer*      _dns;
    String          _networkOptionsHTML; // <option> elements built from scan
    volatile bool   _rebootPending;

    void   scanNetworks();
    void   setupPortalRoutes();
    String derivedAPPassword() const;
};
