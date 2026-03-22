#include "WiFiProvisioning.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

static constexpr uint8_t DNS_PORT = 53;

// ---------------------------------------------------------------------------
// Captive-portal HTML
// Stored in PROGMEM to save RAM.  Split at the network <option> injection
// point so we can insert the scan results without building one giant String.
// ---------------------------------------------------------------------------

static const char PORTAL_HTML_HEAD[] PROGMEM =
    "<!DOCTYPE html>"
    "<html lang='en'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CNC Stop Block \xe2\x80\x94 WiFi Setup</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:16px;color:#222}"
    "h1{font-size:1.3em;margin-bottom:2px}"
    "p.sub{color:#666;font-size:.9em;margin:0 0 20px}"
    "label{display:block;margin-top:14px;font-weight:600;font-size:.9em}"
    "select,input[type=password]{"
      "width:100%;padding:9px 8px;margin-top:5px;box-sizing:border-box;"
      "font-size:1em;border:1px solid #bbb;border-radius:5px;background:#fff}"
    "button{"
      "display:block;margin-top:20px;width:100%;padding:12px;"
      "background:#0066cc;color:#fff;border:none;border-radius:6px;"
      "font-size:1em;font-weight:600;cursor:pointer}"
    "button:active{background:#004fa3}"
    ".note{margin-top:22px;padding-top:14px;border-top:1px solid #e0e0e0;"
      "font-size:.8em;color:#888}"
    "</style>"
    "</head><body>"
    "<h1>&#128296; CNC Stop Block</h1>"
    "<p class='sub'>First-time WiFi setup</p>"
    "<form method='POST' action='/provision'>"
    "<label>Network (SSID)</label>"
    "<select name='ssid'>";

// Scan results are inserted here

static const char PORTAL_HTML_TAIL[] PROGMEM =
    "</select>"
    "<label>Password</label>"
    "<input type='password' name='password' placeholder='WiFi password (leave blank if open)'>"
    "<button type='submit'>Connect &amp; Save</button>"
    "</form>"
    "<p class='note'>"
    "After saving, the device reboots and joins your network.<br>"
    "Check your router for the device IP, then open the CNC Stop Block app.<br>"
    "Your API token will be printed to the serial console on first boot."
    "</p>"
    "</body></html>";

static const char PORTAL_SUCCESS_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Saved</title></head>"
    "<body style='font-family:sans-serif;max-width:420px;margin:60px auto;"
      "padding:16px;text-align:center'>"
    "<h1 style='color:#0a0'>&#10003; Credentials saved</h1>"
    "<p>The device is rebooting and will connect to your WiFi network.</p>"
    "<p>Reconnect your device to your normal WiFi, then open the CNC Stop Block app.</p>"
    "</body></html>";

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

WiFiProvisioning::WiFiProvisioning()
    : _server(nullptr), _dns(nullptr), _rebootPending(false) {}

WiFiProvisioning::~WiFiProvisioning() {
    delete _server;
    delete _dns;
}

// ---------------------------------------------------------------------------
// Static credential helpers
// ---------------------------------------------------------------------------

bool WiFiProvisioning::hasCredentials() {
    return LittleFS.exists(WIFI_CONFIG_PATH);
}

bool WiFiProvisioning::loadCredentials(WiFiCredentials& out) {
    if (!LittleFS.exists(WIFI_CONFIG_PATH)) return false;

    File f = LittleFS.open(WIFI_CONFIG_PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err || !doc["ssid"].is<const char*>()) return false;

    out.ssid     = doc["ssid"].as<String>();
    out.password = doc["password"] | "";
    return out.ssid.length() > 0;
}

bool WiFiProvisioning::saveCredentials(const WiFiCredentials& creds) {
    File f = LittleFS.open(WIFI_CONFIG_PATH, "w");
    if (!f) return false;

    JsonDocument doc;
    doc["ssid"]     = creds.ssid;
    doc["password"] = creds.password;
    serializeJson(doc, f);
    f.close();
    return true;
}

void WiFiProvisioning::clearCredentials() {
    if (LittleFS.exists(WIFI_CONFIG_PATH)) {
        LittleFS.remove(WIFI_CONFIG_PATH);
        Serial.println("[PROV] WiFi credentials cleared");
    }
}

// ---------------------------------------------------------------------------
// AP + captive portal
// ---------------------------------------------------------------------------

String WiFiProvisioning::derivedAPPassword() const {
    // Last 4 bytes of MAC in hex → 8-char password unique to each device
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char pw[9];
    snprintf(pw, sizeof(pw), "%02x%02x%02x%02x", mac[2], mac[3], mac[4], mac[5]);
    return String(pw);
}

void WiFiProvisioning::scanNetworks() {
    Serial.println("[PROV] Scanning for WiFi networks...");
    int n = WiFi.scanNetworks();
    _networkOptionsHTML = "";

    if (n <= 0) {
        _networkOptionsHTML = "<option value=''>-- No networks found --</option>";
        Serial.println("[PROV] No networks found");
        return;
    }

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        // Basic HTML escaping
        ssid.replace("&", "&amp;");
        ssid.replace("<", "&lt;");
        ssid.replace(">", "&gt;");
        ssid.replace("\"", "&quot;");
        _networkOptionsHTML += "<option value=\"" + ssid + "\">";
        _networkOptionsHTML += ssid;
        _networkOptionsHTML += " (" + String(rssi) + " dBm)</option>";
    }

    WiFi.scanDelete();
    Serial.printf("[PROV] Found %d networks\n", n);
}

void WiFiProvisioning::setupPortalRoutes() {
    // Redirect all OS connectivity-check URLs to the portal
    auto redirect = [](AsyncWebServerRequest* req) {
        req->redirect("http://192.168.4.1/");
    };
    _server->on("/generate_204",        HTTP_GET, redirect); // Android Chrome
    _server->on("/connecttest.txt",     HTTP_GET, redirect); // Windows
    _server->on("/hotspot-detect.html", HTTP_GET, redirect); // iOS / macOS
    _server->on("/canonical.html",      HTTP_GET, redirect);
    _server->on("/success.txt",         HTTP_GET, redirect);
    _server->on("/ncsi.txt",            HTTP_GET, redirect); // Windows NCSI
    _server->on("/redirect",            HTTP_GET, redirect);

    // Main portal page
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        String html = FPSTR(PORTAL_HTML_HEAD);
        html += _networkOptionsHTML;
        html += FPSTR(PORTAL_HTML_TAIL);
        req->send(200, "text/html", html);
    });

    // Credential submission
    _server->on("/provision", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!req->hasParam("ssid", true) ||
            req->getParam("ssid", true)->value().isEmpty()) {
            req->send(400, "text/plain", "SSID is required");
            return;
        }

        WiFiCredentials creds;
        creds.ssid     = req->getParam("ssid", true)->value();
        creds.password = req->hasParam("password", true)
                         ? req->getParam("password", true)->value()
                         : "";

        if (saveCredentials(creds)) {
            req->send(200, "text/html", FPSTR(PORTAL_SUCCESS_HTML));
            Serial.printf("[PROV] Saved credentials for SSID: %s\n", creds.ssid.c_str());
            _rebootPending = true;
        } else {
            req->send(500, "text/plain", "Failed to write credentials — check LittleFS");
        }
    });

    // Catch-all: redirect to portal
    _server->onNotFound(redirect);

    _server->begin();
    Serial.println("[PROV] Captive portal started");
}

void WiFiProvisioning::beginAP() {
    String apPass = derivedAPPassword();

    Serial.println("[PROV] =============================================");
    Serial.println("[PROV]  *** WIFI PROVISIONING MODE ***");
    Serial.printf( "[PROV]  SSID     : %s\n", WIFI_AP_SSID);
    Serial.printf( "[PROV]  Password : %s\n", apPass.c_str());
    Serial.println("[PROV]  Connect and open http://192.168.4.1");
    Serial.println("[PROV] =============================================");

    // Scan first (STA mode), then switch to AP
    WiFi.mode(WIFI_STA);
    scanNetworks();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, apPass.c_str());

    Serial.printf("[PROV] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // DNS: redirect every query to 192.168.4.1 (captive portal trigger)
    IPAddress apIP(192, 168, 4, 1);
    _dns = new DNSServer();
    _dns->start(DNS_PORT, "*", apIP);

    // Web portal
    _server = new AsyncWebServer(80);
    setupPortalRoutes();
}

bool WiFiProvisioning::update() {
    if (_dns) _dns->processNextRequest();

    if (_rebootPending) {
        Serial.println("[PROV] Rebooting to apply new credentials...");
        delay(1500); // Let the browser receive the success page
        ESP.restart();
    }

    return _rebootPending;
}
