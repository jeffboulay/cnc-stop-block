# Security Review — CNC Stop Block

Reviewed: March 22, 2026

## Threat Model

This is a WiFi-connected ESP32 device on a local network controlling a miter saw stop block with stepper motor, pneumatic clamps, and dust collection. The UI is a React PWA accessed from an iPad Pro.

**Assumptions:**
- Device operates on a trusted local network
- No internet exposure
- Physical access to the workshop is controlled

**Attack surface:**
- REST API (HTTP, no TLS)
- WebSocket (WS, no TLS)
- WiFi (STA + AP fallback)
- Physical buttons and E-Stop

---

## Findings

### CRITICAL

#### 1. No authentication on any API endpoint

| Detail | |
|---|---|
| **Files** | `firmware/src/WebAPI.cpp` |
| **Impact** | Anyone on the WiFi network can fully control the CNC: move the stepper, engage clamps, start/stop cuts, modify the cut list, and register/remove tools |
| **Recommendation** | Implement API key or Bearer token authentication. Generate a device token on first boot (stored in LittleFS), require it in an `Authorization` header, and validate in WebSocket handshake before accepting connections |

#### 2. Wildcard CORS policy

| Detail | |
|---|---|
| **Files** | `firmware/src/WebAPI.cpp` — `addCORSHeaders()` |
| **Impact** | `Access-Control-Allow-Origin: *` allows any website to make requests to the device. Combined with no authentication, a malicious page can silently control the machine |
| **Recommendation** | Restrict to the specific origin serving the UI, or remove CORS headers entirely if the UI is always served from the same origin |

---

### HIGH

#### 3. WiFi credentials hardcoded in source

| Detail | |
|---|---|
| **Files** | `firmware/include/config.h` |
| **Impact** | `WIFI_SSID` and `WIFI_PASSWORD` are compile-time macros with an empty default password. The AP fallback password `"stopblock"` is trivially guessable |
| **Recommendation** | Store credentials in LittleFS via a first-boot provisioning portal. Randomize the AP password (or derive from MAC address) and display it via serial on first boot |

#### 4. Unbounded POST body accumulation (DoS)

| Detail | |
|---|---|
| **Files** | `firmware/src/WebAPI.cpp` — POST body callbacks |
| **Impact** | `_bodyBuffer` grows without any size limit. A malicious client can send megabytes and exhaust heap memory, crashing the device |
| **Recommendation** | Cap at a reasonable maximum (e.g., 16 KB): `if (_bodyBuffer.length() + len > 16384) { return; }` |

#### 5. No WebSocket connection limits or rate limiting

| Detail | |
|---|---|
| **Files** | `firmware/src/WebAPI.cpp` |
| **Impact** | No cap on concurrent WebSocket clients. No per-IP request throttling. Malicious clients can exhaust resources or flood command endpoints |
| **Recommendation** | Reject new WebSocket connections above a threshold (e.g., 4). Add simple per-IP rate limiting on command endpoints |

#### 6. Unencrypted WebSocket transport

| Detail | |
|---|---|
| **Files** | `ui/src/api/websocket.ts` |
| **Impact** | WebSocket URL is derived by replacing `http` with `ws`, so all traffic is plaintext. Machine state and commands are visible to any network observer |
| **Recommendation** | If TLS is added in the future, ensure the UI switches to `wss://`. Document that this is a local-network-only device |

---

### MEDIUM

#### 7. Insufficient input validation on API endpoints

| Detail | |
|---|---|
| **Files** | `firmware/src/WebAPI.cpp` — `/api/goto`, `/api/jog`, `/api/cutlist/add`, `/api/tools` |
| **Impact** | Endpoints check for key presence but not type, range, NaN, or Infinity. Cut entry labels and tool names have no length limit |
| **Recommendation** | Validate type (`doc["position_mm"].is<float>()`), reject NaN/Infinity, enforce range (0 to `MAX_TRAVEL_MM`), cap string lengths to 64 characters |

#### 8. Unbounded cut list and tool registry growth

| Detail | |
|---|---|
| **Files** | `firmware/src/CutList.cpp`, `firmware/src/RFIDReader.cpp` |
| **Impact** | No cap on `_entries` or `_toolRegistry` vector sizes. Heap exhaustion possible via repeated additions |
| **Recommendation** | Define `MAX_CUTS` (e.g., 100) and `MAX_TOOLS` (e.g., 50); reject additions beyond the limit |

#### 9. JSON injection in error responses

| Detail | |
|---|---|
| **Files** | `firmware/src/WebAPI.cpp` — `sendError()` |
| **Impact** | Error messages are concatenated into JSON strings without escaping. If a message contains quotes or backslashes, the response is malformed |
| **Recommendation** | Build error JSON with ArduinoJson: `JsonDocument doc; doc["error"] = message; serializeJson(doc, output);` |

#### 10. State machine allows unsafe concurrent operations

| Detail | |
|---|---|
| **Files** | `firmware/src/SystemController.cpp` |
| **Impact** | `commandGoTo` can be called during MOVING or CUTTING states. Cut list can be modified during an active cut. No command sequencing enforcement |
| **Recommendation** | Guard command handlers with explicit state checks. Lock cut list mutations during CUTTING state |

#### 11. No frontend input validation

| Detail | |
|---|---|
| **Files** | `ui/src/components/AddCutModal.tsx`, `ui/src/components/GoToInput.tsx` |
| **Impact** | Cut labels have no max length. Quantity has no upper bound. Position values have no range check against `MAX_TRAVEL_MM` |
| **Recommendation** | Add client-side validation: max label length (64 chars), quantity range (1–999), position range (0–MAX_TRAVEL_MM) |

---

### LOW / INFO

| # | Severity | Finding | Location | Recommendation |
|---|----------|---------|----------|----------------|
| 12 | LOW | No watchdog timer configured | `firmware/src/main.cpp` | Add `esp_task_wdt_init()` for crash recovery |
| 13 | LOW | No security response headers | `firmware/src/WebAPI.cpp` | Add `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY` |
| 14 | LOW | No Content Security Policy | `ui/index.html` | Add CSP meta tag restricting script/connect sources |
| 15 | LOW | Hardcoded dev proxy IP | `ui/vite.config.ts` | Use `VITE_PROXY_TARGET` env var |
| 16 | LOW | Error messages shown directly | `ui/src/components/ErrorBanner.tsx` | Map internal error codes to user-friendly messages |
| 17 | INFO | No OTA update mechanism | N/A | Good — eliminates an attack surface. If added later, require signed firmware images |

---

## Positive Findings

- **No XSS vectors** — React default escaping used throughout; no `dangerouslySetInnerHTML` or `innerHTML`
- **No hardcoded secrets in UI** — API URL configured via environment variable
- **Strict TypeScript** — `strict: true` enabled, no unsafe `any` types
- **No OTA/remote update** — reduces attack surface
- **Fail-safe limit switches** — NC wiring means a broken wire triggers the switch (hardware safety)
- **Hardware E-Stop** — wired in series with stepper driver ENABLE, independent of firmware

---

## Priority Action Plan

| Priority | Action | Status |
|----------|--------|--------|
| **P0** | Add API key authentication to all endpoints | **TODO** |
| **P0** | Restrict CORS to specific UI origin | **FIXED** — configurable via `CORS_ORIGIN` build flag |
| **P1** | Cap POST body size at 16 KB | **FIXED** — `MAX_POST_BODY_BYTES` in config.h, returns 413 |
| **P1** | Add WebSocket connection limit | **FIXED** — `WS_MAX_CLIENTS` (default 4) |
| **P1** | Move WiFi credentials to LittleFS provisioning | **TODO** |
| **P2** | Add input type/range validation on firmware endpoints | **FIXED** — type checks, NaN/Inf rejection, range enforcement |
| **P2** | Cap cut list and tool registry sizes | **FIXED** — `MAX_CUTS` (100), `MAX_TOOLS` (50) |
| **P2** | Fix JSON escaping in `sendError()` | **FIXED** — uses ArduinoJson serializer |
| **P2** | Add state machine guards against unsafe command sequences | **FIXED** — cutlist mutations blocked during CUTTING |
| **P2** | Add frontend input validation | **FIXED** — label length, quantity range, position range |
| **P3** | Add watchdog timer | **FIXED** — `esp_task_wdt_init()` with 10s timeout |
| **P3** | Add security response headers | **FIXED** — `X-Content-Type-Options`, `X-Frame-Options`, `Cache-Control` |
| **P3** | Add CSP meta tag | **FIXED** — restricts script/connect sources |
| **P3** | Use env var for dev proxy target | **FIXED** — `VITE_PROXY_TARGET` env var |
