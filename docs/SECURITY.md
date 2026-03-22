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

| Detail             |                                                                                                                                                                                                                         |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :x: **Open**                                                                                                                                                                                                            |
| **Files**          | `firmware/src/WebAPI.cpp`                                                                                                                                                                                               |
| **Impact**         | Anyone on the WiFi network can fully control the CNC: move the stepper, engage clamps, start/stop cuts, modify the cut list, and register/remove tools                                                                  |
| **Recommendation** | Implement API key or Bearer token authentication. Generate a device token on first boot (stored in LittleFS), require it in an `Authorization` header, and validate in WebSocket handshake before accepting connections |

#### 2. Wildcard CORS policy

| Detail             |                                                                                                                                                                                                      |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :warning: **Partially fixed**                                                                                                                                                                        |
| **Files**          | `firmware/src/WebAPI.cpp` — `addCORSHeaders()`                                                                                                                                                       |
| **Impact**         | `Access-Control-Allow-Origin: *` allows any website to make requests to the device. Combined with no authentication, a malicious page can silently control the machine                               |
| **Recommendation** | Restrict to the specific origin serving the UI, or remove CORS headers entirely if the UI is always served from the same origin                                                                      |
| **Current state**  | CORS origin is configurable via `CORS_ORIGIN`, but default behavior is still wildcard (`*`) when the flag is not set. This remains exploitable on shared/trusted networks if configuration is missed |

---

### HIGH

#### 3. WiFi credentials hardcoded in source

| Detail             |                                                                                                                                                                    |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Status**         | :x: **Open**                                                                                                                                                       |
| **Files**          | `firmware/include/config.h`                                                                                                                                        |
| **Impact**         | `WIFI_SSID` and `WIFI_PASSWORD` are compile-time macros with an empty default password. The AP fallback password `"stopblock"` is trivially guessable              |
| **Recommendation** | Store credentials in LittleFS via a first-boot provisioning portal. Randomize the AP password (or derive from MAC address) and display it via serial on first boot |

#### 4. Unbounded POST body accumulation (DoS)

| Detail             |                                                                                                                                                       |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                          |
| **Files**          | `firmware/src/WebAPI.cpp` — `onBodyChunk()`                                                                                                           |
| **Impact**         | `_bodyBuffer` grows without any size limit. A malicious client can send megabytes and exhaust heap memory, crashing the device                        |
| **Recommendation** | Cap at a reasonable maximum (e.g., 16 KB): `if (_bodyBuffer.length() + len > 16384) { return; }`                                                      |
| **Solution**       | Body accumulator capped at `MAX_POST_BODY_BYTES` (16,384 bytes, defined in `config.h`). Overflow sets `_bodyOverflow` flag; endpoints return HTTP 413 |

#### 5. No WebSocket connection limits or rate limiting

| Detail             |                                                                                                                                          |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :warning: **Partially fixed**                                                                                                            |
| **Files**          | `firmware/src/WebAPI.cpp` — `onWebSocketEvent()`                                                                                         |
| **Impact**         | No cap on concurrent WebSocket clients. No per-IP request throttling. Malicious clients can exhaust resources or flood command endpoints |
| **Recommendation** | Reject new WebSocket connections above a threshold (e.g., 4). Add simple per-IP rate limiting on command endpoints                       |
| **Current state**  | WebSocket client cap is implemented (`WS_MAX_CLIENTS`), but per-IP/request rate limiting is still not implemented                        |

#### 7. No HTTP endpoint rate limiting

| Detail             |                                                                                                                                                                              |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :x: **Open**                                                                                                                                                                 |
| **Files**          | `firmware/src/WebAPI.cpp`                                                                                                                                                    |
| **Impact**         | High-frequency POST/DELETE traffic can still flood the control API even with body-size checks and WS client caps, causing CPU starvation and degraded control responsiveness |
| **Recommendation** | Add per-IP throttling for command endpoints (e.g., 100-250ms minimum interval per client/IP), return HTTP 429 when exceeded                                                  |

#### 6. Unencrypted WebSocket transport

| Detail             |                                                                                                                                                     |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :x: **Open** (accepted risk)                                                                                                                        |
| **Files**          | `ui/src/api/websocket.ts`                                                                                                                           |
| **Impact**         | WebSocket URL is derived by replacing `http` with `ws`, so all traffic is plaintext. Machine state and commands are visible to any network observer |
| **Recommendation** | If TLS is added in the future, ensure the UI switches to `wss://`. Document that this is a local-network-only device                                |

---

### MEDIUM

#### 7. Insufficient input validation on API endpoints

| Detail             |                                                                                                                                                                                                                                                      |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                                                                                                         |
| **Files**          | `firmware/src/WebAPI.cpp` — `/api/goto`, `/api/jog`, `/api/cutlist/add`, `/api/tools`                                                                                                                                                                |
| **Impact**         | Endpoints check for key presence but not type, range, NaN, or Infinity. Cut entry labels and tool names have no length limit                                                                                                                         |
| **Recommendation** | Validate type (`doc["position_mm"].is<float>()`), reject NaN/Infinity, enforce range (0 to `MAX_TRAVEL_MM`), cap string lengths to 64 characters                                                                                                     |
| **Solution**       | All endpoints now use `.is<float>()` type checks, `isValidFloat()` to reject NaN/Inf, range enforcement (0–`MAX_TRAVEL_MM`), and `truncateString()` to cap labels/names at `MAX_STRING_LENGTH` (64). Kerf capped at 0–20mm. Quantity capped at 1–999 |

#### 8. Unbounded cut list and tool registry growth

| Detail             |                                                                                                                                                         |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                            |
| **Files**          | `firmware/src/WebAPI.cpp`, `firmware/include/config.h`                                                                                                  |
| **Impact**         | No cap on `_entries` or `_toolRegistry` vector sizes. Heap exhaustion possible via repeated additions                                                   |
| **Recommendation** | Define `MAX_CUTS` (e.g., 100) and `MAX_TOOLS` (e.g., 50); reject additions beyond the limit                                                             |
| **Solution**       | `MAX_CUTS` (100) and `MAX_TOOLS` (50) defined in `config.h`. API returns HTTP 409 when limits are reached. Tool updates to existing UIDs bypass the cap |

#### 9. JSON injection in error responses

| Detail             |                                                                                                                                            |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------ |
| **Status**         | :white_check_mark: **Fixed**                                                                                                               |
| **Files**          | `firmware/src/WebAPI.cpp` — `sendError()`                                                                                                  |
| **Impact**         | Error messages are concatenated into JSON strings without escaping. If a message contains quotes or backslashes, the response is malformed |
| **Recommendation** | Build error JSON with ArduinoJson: `JsonDocument doc; doc["error"] = message; serializeJson(doc, output);`                                 |
| **Solution**       | `sendError()` now uses `JsonDocument` + `serializeJson()` for proper escaping                                                              |

#### 10. State machine allows unsafe concurrent operations

| Detail             |                                                                                                                                                                    |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                       |
| **Files**          | `firmware/src/WebAPI.cpp`, `firmware/src/SystemController.cpp`                                                                                                     |
| **Impact**         | Cut list can be modified during an active cut. No command sequencing enforcement                                                                                   |
| **Recommendation** | Guard command handlers with explicit state checks. Lock cut list mutations during CUTTING state                                                                    |
| **Solution**       | Cut list clear and delete endpoints return HTTP 409 during `CUTTING` state. `commandGoTo` and `commandJog` already guard against invalid states (IDLE/LOCKED only) |

#### 11. No frontend input validation

| Detail             |                                                                                                                                                                                                               |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                                                                  |
| **Files**          | `ui/src/components/AddCutModal.tsx`, `ui/src/components/GoToInput.tsx`                                                                                                                                        |
| **Impact**         | Cut labels have no max length. Quantity has no upper bound. Position values have no range check against `MAX_TRAVEL_MM`                                                                                       |
| **Recommendation** | Add client-side validation: max label length (64 chars), quantity range (1–999), position range (0–MAX_TRAVEL_MM)                                                                                             |
| **Solution**       | `AddCutModal`: `maxLength={64}` on label input, quantity capped 1–999, length validated 0–1200, `isFinite()` check, label truncated on submit. `GoToInput`: position validated 0–1200 with `isFinite()` check |

---

### LOW / INFO

| #   | Severity | Finding                       | Location                            | Status                   | Solution                                                                                                               |
| --- | -------- | ----------------------------- | ----------------------------------- | ------------------------ | ---------------------------------------------------------------------------------------------------------------------- |
| 12  | LOW      | No watchdog timer configured  | `firmware/src/main.cpp`             | :white_check_mark: Fixed | `esp_task_wdt_init()` with 10s timeout; panics and reboots on stall                                                    |
| 13  | LOW      | No security response headers  | `firmware/src/WebAPI.cpp`           | :white_check_mark: Fixed | `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Cache-Control: no-store` added via `addSecurityHeaders()` |
| 14  | LOW      | No Content Security Policy    | `ui/index.html`                     | :white_check_mark: Fixed | CSP meta tag restricts `script-src 'self'`, allows `connect-src` for WS/HTTP to ESP32                                  |
| 15  | LOW      | Hardcoded dev proxy IP        | `ui/vite.config.ts`                 | :white_check_mark: Fixed | `VITE_PROXY_TARGET` env var (defaults to `http://192.168.1.100`)                                                       |
| 16  | LOW      | Error messages shown directly | `ui/src/components/ErrorBanner.tsx` | :x: Open                 | Map internal error codes to user-friendly messages                                                                     |
| 17  | INFO     | No OTA update mechanism       | N/A                                 | :white_check_mark: N/A   | Good — eliminates an attack surface. If added later, require signed firmware images                                    |

---

## Positive Findings

- **No XSS vectors** — React default escaping used throughout; no `dangerouslySetInnerHTML` or `innerHTML`
- **No hardcoded secrets in UI** — API URL configured via environment variable
- **Strict TypeScript** — `strict: true` enabled, no unsafe `any` types
- **No OTA/remote update** — reduces attack surface
- **Fail-safe limit switches** — NC wiring means a broken wire triggers the switch (hardware safety)
- **Hardware E-Stop** — wired in series with stepper driver ENABLE, independent of firmware

---

## Resolution Summary

| Severity  | Total  | Fixed  | Open  |
| --------- | ------ | ------ | ----- |
| Critical  | 2      | 0      | 2     |
| High      | 5      | 1      | 4     |
| Medium    | 5      | 5      | 0     |
| Low       | 5      | 4      | 1     |
| Info      | 1      | —      | —     |
| **Total** | **17** | **10** | **7** |

---

## Next Steps

### P0 — API Key Authentication (#1)

The highest-priority remaining finding. Proposed approach:

1. **Token generation**: On first boot (or when no token exists in LittleFS), generate a random 32-byte hex token, store it at `/token.txt` in LittleFS, and print it to Serial
2. **HTTP enforcement**: Require `Authorization: Bearer <token>` header on all `/api/*` endpoints. Return 401 on missing/invalid token. Exempt `/api/status` for read-only monitoring (or make this configurable)
3. **WebSocket enforcement**: Validate token as a query parameter on the WebSocket handshake (`ws://<ip>/ws?token=<token>`). Reject connections without a valid token
4. **UI integration**: Store token in `localStorage` after initial pairing. Add a one-time setup screen that accepts the token (displayed on Serial or a physical LCD)
5. **Token rotation**: Provide a `/api/token/rotate` endpoint (authenticated) that generates a new token and invalidates the old one

### P1 — WiFi Credential Provisioning (#3)

Replace hardcoded WiFi credentials with a captive portal provisioning flow:

1. **First-boot AP mode**: If no credentials are stored, start in AP mode with a password derived from the device MAC address (e.g., last 6 hex digits). Print the AP name and password to Serial
2. **Captive portal**: Serve a minimal HTML form from LittleFS at `192.168.4.1` that scans nearby networks and accepts SSID + password
3. **Credential storage**: Save credentials to LittleFS (not NVS, to allow factory reset by formatting the filesystem)
4. **Factory reset**: Hold the E-Stop button for 10 seconds during boot to wipe stored credentials and return to AP mode

### P2 — Per-IP Rate Limiting (#5 partial)

Add simple rate limiting to command endpoints to prevent flooding:

1. Track last-command timestamp per client IP (small fixed-size array of 4–8 entries)
2. Reject commands arriving faster than 100ms from the same IP
3. Exempt `/api/status` GET and WebSocket broadcasts

### P3 — Error Message Mapping (#16)

Map firmware error strings to user-friendly messages in the React UI to avoid leaking internal details:

1. Define an error code enum in the firmware status JSON (e.g., `"error_code": "MOVE_TIMEOUT"`)
2. Map codes to user-facing messages in the React UI (e.g., "The stop block took too long to reach position. Check for obstructions.")
3. Keep raw error string available in a collapsible "Details" section for debugging
