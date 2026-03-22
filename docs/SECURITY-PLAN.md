# Security Remediation Plan

Created: March 22, 2026
Tracks: 7 open findings from `docs/SECURITY.md`

---

## Overview

This plan addresses the 7 open security findings in four phases, ordered by severity and dependency. Phase 1 removes the two critical issues first. Phases 2 through 4 address the remaining high- and low-priority items. Each phase is intended to be independently testable and deployable.

## Open Findings

| # | Severity | Finding | Phase | Status |
|---|----------|---------|-------|--------|
| 1 | CRITICAL | No API authentication | Phase 1 | ✅ Complete |
| 2 | CRITICAL | CORS wildcard default | Phase 1 | ✅ Complete |
| 3 | HIGH | WiFi credentials hardcoded | Phase 2 | ✅ Complete |
| 5 | HIGH | No per-IP rate limiting (partial) | Phase 3 | ✅ Complete |
| 7 | HIGH | No HTTP endpoint rate limiting | Phase 3 | ✅ Complete |
| 6 | HIGH | Unencrypted transport (accepted risk) | Phase 4 | Open |
| 16 | LOW | Error messages shown directly | Phase 4 | Open |

---

## Phase 1: API Authentication and CORS Lockdown ✅ Complete

Priority: Critical — shipped in `feat/api-auth-phase1` (PR #7)

This phase has no blockers and should ship first. It removes unauthenticated machine control and the unsafe default cross-origin policy.

### Step 1.1: Token generation and storage ✅

- Add `#include <LittleFS.h>` to `firmware/src/WebAPI.cpp`
- Add private method `WebAPI::loadOrCreateToken()` and call it from `begin()`
- If `/auth_token.txt` does not exist in LittleFS, generate a 32-byte random hex token using `esp_random()`, save it, and print it to Serial
- Store the current token in a new `String _authToken` member
- Add `constexpr const char* AUTH_TOKEN_PATH = "/auth_token.txt";` to `firmware/include/config.h`

Files:
- `firmware/src/WebAPI.h`
- `firmware/src/WebAPI.cpp`
- `firmware/include/config.h`

### Step 1.2: HTTP authentication middleware ✅

- Add private method `bool WebAPI::isAuthenticated(AsyncWebServerRequest* request)`
- Require `Authorization: Bearer <token>` on API requests
- Return HTTP 401 with `{"error":"Unauthorized"}` when missing or invalid
- Apply auth to all route handlers except OPTIONS preflight
- Exempt `GET /api/status` by default for read-only monitoring via `AUTH_EXEMPT_STATUS`

Files:
- `firmware/src/WebAPI.h`
- `firmware/src/WebAPI.cpp`
- `firmware/include/config.h`

### Step 1.3: WebSocket authentication ✅

- In the `WS_EVT_CONNECT` branch of `onWebSocketEvent()`, parse `token=` from the handshake URL
- Compare it to `_authToken`
- Reject the connection if the token is missing or invalid

Files:
- `firmware/src/WebAPI.cpp`

### Step 1.4: CORS default lockdown ✅

- Change the fallback branch in `addCORSHeaders()` so it does not send `Access-Control-Allow-Origin` by default
- Keep cross-origin access opt-in via `-DCORS_ORIGIN=\"...\"` in `firmware/platformio.ini`

Files:
- `firmware/src/WebAPI.cpp`

### Step 1.5: UI token storage and request wiring ✅

- Add `ui/src/api/auth.ts` with `getToken()` and `setToken()` helpers using `localStorage`
- Add the bearer token to all HTTP requests in `ui/src/api/client.ts`
- Append `?token=<token>` to the WebSocket URL in `ui/src/api/websocket.ts`
- On HTTP 401, clear the stored token and return the UI to setup flow

Files:
- `ui/src/api/auth.ts`
- `ui/src/api/client.ts`
- `ui/src/api/websocket.ts`

### Step 1.6: UI pairing screen ✅

- Add `ui/src/components/TokenSetup.tsx`
- Show it from `ui/src/App.tsx` when no token is present
- Persist the token on submit and reload the app state

Files:
- `ui/src/components/TokenSetup.tsx`
- `ui/src/App.tsx`

### Step 1.7: Token rotation ✅

- Add authenticated endpoint `POST /api/token/rotate`
- Generate and persist a new token
- Return `{"token":"<new>"}` and invalidate the old token immediately

Files:
- `firmware/src/WebAPI.cpp`

### Verification

- [x] `pio run` succeeds
- [x] Unauthenticated API requests return 401
- [x] Authenticated API requests succeed
- [x] WebSocket rejects missing or invalid tokens
- [x] Cross-origin requests fail unless `CORS_ORIGIN` is explicitly configured
- [x] UI shows a pairing screen until a valid token is entered

---

## Phase 2: WiFi Credential Provisioning ✅ Complete

Priority: High — shipped in `feat/api-auth-phase1` (PR #7)
Dependency: Phase 1 should be complete before provisioning is exposed.

This phase removes hardcoded credentials and adds a first-boot provisioning flow.

### Step 2.1: Credential storage in LittleFS ✅

- Add `firmware/src/WiFiProvisioning.h` and `firmware/src/WiFiProvisioning.cpp`
- Implement `loadCredentials()`, `saveCredentials()`, `clearCredentials()`, and `hasCredentials()`
- Store credentials in `/wifi_config.json`
- Remove default `WIFI_SSID` and `WIFI_PASSWORD` values from `config.h`, keeping only build-time overrides if needed

Files:
- `firmware/src/WiFiProvisioning.h`
- `firmware/src/WiFiProvisioning.cpp`
- `firmware/include/config.h`

### Step 2.2: AP mode with device-derived password ✅

- If no credentials are stored, start AP mode
- Use SSID `CNC-StopBlock`
- Derive the password from the last 8 hex characters of the MAC address
- Print AP credentials to Serial
- Add a provisioning-mode LED pattern

Files:
- `firmware/src/WiFiProvisioning.cpp`
- `firmware/src/SystemController.cpp`
- `firmware/src/IndicatorLights.cpp`

### Step 2.3: Captive portal ✅

- Serve a minimal provisioning page at `192.168.4.1`
- Scan nearby WiFi networks and show them in a dropdown
- Accept SSID and password, save them, and reboot

Files:
- `firmware/src/WiFiProvisioning.cpp`

### Step 2.4: Factory reset via E-Stop hold ✅

- During boot, if the E-Stop button is held for 10 seconds, wipe saved credentials and the auth token
- Reboot into AP provisioning mode
- Print `FACTORY RESET` to Serial

Files:
- `firmware/src/main.cpp`
- `firmware/src/WiFiProvisioning.cpp`

### Verification

- [x] Device boots into AP mode when no credentials are stored
- [x] Provisioning UI lists nearby networks
- [x] Device connects successfully after provisioning
- [x] E-Stop hold performs a factory reset
- [x] `pio run` succeeds

---

## Phase 3: Rate Limiting ✅ Complete

Priority: High — shipped in `feat/rate-limiting-phase3` (PR #9)
Dependency: None. This can proceed in parallel with Phase 2.

This phase limits command flooding against the control API.

### Step 3.1: Per-IP rate limiter ✅

- Add `RateLimitEntry { IPAddress ip; unsigned long lastCommandMs; }` in `firmware/src/WebAPI.h`
- Maintain a fixed-size entry list with simple eviction
- Add `WebAPI::isRateLimited(AsyncWebServerRequest* request)`
- Reject repeated commands from the same IP within `RATE_LIMIT_MS` with HTTP 429 and `{"error":"Too many requests"}`

Files:
- `firmware/src/WebAPI.h`
- `firmware/src/WebAPI.cpp`
- `firmware/include/config.h`

### Step 3.2: Apply limits to command endpoints ✅

- Apply rate limiting to POST and DELETE handlers
- Exempt read-only GET routes, OPTIONS preflight, and WebSocket broadcasts

Files:
- `firmware/src/WebAPI.cpp`

### Design notes

- `POST /api/estop` is **exempt** from rate limiting — safety-critical and must never be throttled
- Rate limiting is checked **before** auth so unauthenticated flood attempts are also blocked
- The table tracks up to `RATE_LIMIT_TABLE_SIZE` (8) IPs concurrently; the oldest slot is evicted when full
- `RATE_LIMIT_MS` (200 ms) allows comfortable interactive use while blocking automated flooding

### Verification

- [x] Rapid repeated POST or DELETE requests from the same IP receive HTTP 429
- [x] `POST /api/estop` is never rate-limited
- [x] Normal UI interaction remains unaffected (button taps are well over 200 ms apart)
- [x] Read-only GET routes and WebSocket broadcasts are not rate-limited
- [x] `pio run` succeeds

---

## Phase 4: Polish and Accepted-Risk Documentation

Priority: Low
Dependency: None. This can proceed in parallel with Phases 2 and 3.

### Step 4.1: Error code mapping

- Add an `error_code` field to firmware status responses alongside the raw error string
- Add `ui/src/api/errorMessages.ts` to map codes to user-facing messages
- Update `ui/src/components/ErrorBanner.tsx` to show friendly text by default and raw details optionally

Files:
- `firmware/src/WebAPI.cpp`
- `firmware/src/SystemController.cpp`
- `ui/src/api/errorMessages.ts`
- `ui/src/api/types.ts`
- `ui/src/components/ErrorBanner.tsx`

### Step 4.2: Document accepted risk for no TLS

- Update `docs/SECURITY.md` to explicitly document plaintext HTTP and WebSocket on the local network as an accepted risk
- Note the ESP32 RAM cost of HTTPS/TLS under Arduino
- Recommend network isolation as the mitigation strategy

Files:
- `docs/SECURITY.md`

### Verification

- Error banners show user-friendly messages
- Raw diagnostic details remain available when needed
- TLS risk documentation is present and accurate

---

## Design Decisions

- Use bearer token authentication instead of username/password because the system is a single-device workshop controller
- Store auth tokens and WiFi configuration in LittleFS for simpler reset behavior
- Rate-limit by client IP instead of token so unauthenticated abuse is also covered
- Keep `GET /api/status` auth-exempt by default for read-only monitoring, but make it configurable
- Treat TLS as an accepted risk and mitigate operationally with network isolation

## Completion Criteria

The plan is complete when:

1. All seven open findings in `docs/SECURITY.md` are resolved or explicitly documented as accepted risk
2. Firmware builds cleanly with `pio run`
3. UI continues to function end-to-end with the new auth flow
4. The security review and remediation docs remain consistent with implementation state