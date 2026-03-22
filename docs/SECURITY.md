# Security Review — CNC Stop Block

Reviewed: March 22, 2026

Implementation plan: [SECURITY-PLAN.md](SECURITY-PLAN.md)

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
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                                                                            |
| **Files**          | `firmware/src/WebAPI.cpp`, `firmware/src/WebAPI.h`, `firmware/include/config.h`, `ui/src/api/auth.ts`, `ui/src/api/client.ts`, `ui/src/api/websocket.ts`, `ui/src/components/TokenSetup.tsx`, `ui/src/App.tsx`         |
| **Impact**         | Anyone on the WiFi network can fully control the CNC: move the stepper, engage clamps, start/stop cuts, modify the cut list, and register/remove tools                                                                  |
| **Recommendation** | Implement API key or Bearer token authentication. Generate a device token on first boot (stored in LittleFS), require it in an `Authorization` header, and validate in WebSocket handshake before accepting connections |
| **Solution**       | 32-byte random hex token generated via `esp_random()` on first boot, stored in `/auth_token.txt` (LittleFS), printed to Serial. All `/api/*` endpoints require `Authorization: Bearer <token>`; 401 returned otherwise. `GET /api/status` exempt when `AUTH_EXEMPT_STATUS=true`. WebSocket validates `?token=` query parameter on connect. UI stores token in `localStorage`, shows pairing screen until token is entered, sends header on all requests, clears token on 401. `POST /api/token/rotate` allows authenticated rotation. Sim mirrors the same auth flow with a per-run random token (or fixed via `SIM_TOKEN` env) |

#### 2. Wildcard CORS policy

| Detail             |                                                                                                                                                                                                      |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                                                         |
| **Files**          | `firmware/src/WebAPI.cpp` — `addCORSHeaders()`                                                                                                                                                       |
| **Impact**         | `Access-Control-Allow-Origin: *` allows any website to make requests to the device. Combined with no authentication, a malicious page can silently control the machine                               |
| **Recommendation** | Restrict to the specific origin serving the UI, or remove CORS headers entirely if the UI is always served from the same origin                                                                      |
| **Solution**       | Wildcard default removed. `addCORSHeaders()` only sends `Access-Control-Allow-Origin` when the `-DCORS_ORIGIN="..."` build flag is explicitly set in `platformio.ini`. Cross-origin access is now opt-in, not opt-out |

---

### HIGH

#### 3. WiFi credentials hardcoded in source

| Detail             |                                                                                                                                                                    |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                       |
| **Files**          | `firmware/src/WiFiProvisioning.h/.cpp`, `firmware/include/config.h`, `firmware/src/SystemController.cpp`                                                          |
| **Impact**         | `WIFI_SSID` and `WIFI_PASSWORD` are compile-time macros with an empty default password. The AP fallback password `"stopblock"` is trivially guessable              |
| **Recommendation** | Store credentials in LittleFS via a first-boot provisioning portal. Randomize the AP password (or derive from MAC address) and display it via serial on first boot |
| **Solution**       | Hardcoded credentials removed entirely. On first boot (or after factory reset), device starts `CNC-StopBlock` SoftAP with a password derived from the last 4 MAC bytes (8 hex chars, unique per device). A captive-portal web page at `192.168.4.1` scans nearby networks and accepts SSID + password, which are saved to `/wifi_config.json` in LittleFS. If the saved credentials fail to connect (15 s timeout), the device falls back to AP provisioning automatically. E-Stop held for 10 s during boot triggers a factory reset that wipes credentials and the auth token. |

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
| **Status**         | :white_check_mark: **Fixed**                                                                                                             |
| **Files**          | `firmware/src/WebAPI.cpp`, `firmware/src/WebAPI.h`, `firmware/include/config.h`                                                          |
| **Impact**         | No cap on concurrent WebSocket clients. No per-IP request throttling. Malicious clients can exhaust resources or flood command endpoints |
| **Recommendation** | Reject new WebSocket connections above a threshold (e.g., 4). Add simple per-IP rate limiting on command endpoints                       |
| **Solution**       | WebSocket client cap (`WS_MAX_CLIENTS = 4`) rejects connections above the threshold. Per-IP rate limiting added via `isRateLimited()` on all POST and DELETE endpoints (200 ms minimum gap per IP, HTTP 429 on violation). `POST /api/estop` is exempt. Rate limiting is checked before authentication to also block unauthenticated flood attempts. |

#### 7. No HTTP endpoint rate limiting

| Detail             |                                                                                                                                                                              |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed** — see finding #5 above                                                                                                                         |
| **Files**          | `firmware/src/WebAPI.cpp`, `firmware/src/WebAPI.h`, `firmware/include/config.h`                                                                                             |
| **Impact**         | High-frequency POST/DELETE traffic can still flood the control API even with body-size checks and WS client caps, causing CPU starvation and degraded control responsiveness |
| **Recommendation** | Add per-IP throttling for command endpoints (e.g., 100-250ms minimum interval per client/IP), return HTTP 429 when exceeded                                                  |
| **Solution**       | Addressed together with finding #5. Fixed-size `RateLimitEntry` table (8 slots, LRU eviction) tracks last command timestamp per client IPv4. See finding #5 for full details. |

#### 6. Unencrypted WebSocket transport

| Detail             |                                                                                                                                                     |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Accepted risk — documented**                                                                                                   |
| **Files**          | `ui/src/api/websocket.ts`                                                                                                                           |
| **Impact**         | WebSocket URL is derived by replacing `http` with `ws`, so all traffic is plaintext. Machine state and commands are visible to any network observer |
| **Recommendation** | If TLS is added in the future, ensure the UI switches to `wss://`. Document that this is a local-network-only device                                |
| **Accepted risk**  | TLS (HTTPS/WSS) on the ESP32 Arduino framework requires mbedTLS, which consumes approximately 50–80 KB of RAM in addition to TLS session memory. The current firmware uses 17.5% of available RAM (57 KB of 328 KB). Adding TLS would push RAM usage to ~40–50% and introduce non-trivial maintenance complexity. **Mitigations in place:** (1) bearer token authentication means commands cannot be replayed without the token; (2) the device is intended to operate on a local workshop network, not exposed to the internet; (3) if network isolation is a concern, place the ESP32 on a dedicated IoT VLAN. This risk is accepted for a v1 workshop controller and should be revisited if the device is ever used on a shared or untrusted network. |

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
| **Solution**       | `AddCutModal`: `maxLength={64}` on label input, quantity capped 1–999, length validated 0–1200, `isFinite()` check, label truncated on submit. `GoToInput`: position validated 0–1200 with `isFinite()` check, error message shown inline on validation failure, button shows loading state during pending mutation, disabled during in-flight request |

#### 18. WebSocket connection status always reported as disconnected

| Detail             |                                                                                                                                                                                                               |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | :white_check_mark: **Fixed**                                                                                                                                                                                  |
| **Files**          | `ui/src/hooks/useStatus.ts` — `useIsConnected()`                                                                                                                                                              |
| **Impact**         | `useIsConnected` used a TanStack Query with `staleTime: 0` (default). TanStack Query continuously refetched the query, and the `queryFn: () => Promise.resolve(false)` overwrote the `true` value set by the WebSocket `onopen` handler. Result: the UI always showed "Disconnected" even when the WebSocket was active, suppressing real-time DRO updates |
| **Solution**       | Set `staleTime: Infinity`, `refetchOnWindowFocus: false`, `refetchOnMount: false`, `refetchOnReconnect: false` on the `ws_connected` query so TanStack Query never refetches it — the value is exclusively managed by `queryClient.setQueryData()` in the WebSocket hook |

---

### LOW / INFO

| #   | Severity | Finding                       | Location                            | Status                   | Solution                                                                                                               |
| --- | -------- | ----------------------------- | ----------------------------------- | ------------------------ | ---------------------------------------------------------------------------------------------------------------------- |
| 12  | LOW      | No watchdog timer configured  | `firmware/src/main.cpp`             | :white_check_mark: Fixed | `esp_task_wdt_init()` with 10s timeout; panics and reboots on stall                                                    |
| 13  | LOW      | No security response headers  | `firmware/src/WebAPI.cpp`           | :white_check_mark: Fixed | `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Cache-Control: no-store` added via `addSecurityHeaders()` |
| 14  | LOW      | No Content Security Policy    | `ui/index.html`                     | :white_check_mark: Fixed | CSP meta tag restricts `script-src 'self'`, allows `connect-src` for WS/HTTP to ESP32                                  |
| 15  | LOW      | Hardcoded dev proxy IP        | `ui/vite.config.ts`                 | :white_check_mark: Fixed | `VITE_PROXY_TARGET` env var (defaults to `http://192.168.1.100`)                                                       |
| 16  | LOW      | Error messages shown directly | `ui/src/components/ErrorBanner.tsx` | :white_check_mark: Fixed | Firmware now emits a stable `error_code` field alongside the raw error string. `ui/src/api/errorMessages.ts` maps codes to user-friendly title + detail text. `ErrorBanner` shows the friendly message by default with a collapsible "Details" section revealing the raw string for debugging. |
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

| Severity  | Total  | Fixed | Accepted risk | Open |
| --------- | ------ | ----- | ------------- | ---- |
| Critical  | 2      | 2     | —             | 0    |
| High      | 5      | 4     | 1 (#6 TLS)    | 0    |
| Medium    | 7      | 7     | —             | 0    |
| Low       | 5      | 5     | —             | 0    |
| Info      | 1      | —     | —             | —    |
| **Total** | **19** | **18** | **1**        | **0** |

---

## Remediation Status

All 19 findings have been resolved or explicitly accepted. The security remediation plan is complete.

| Phase | PR | Status | Findings addressed |
|---|---|---|---|
| P0 — API authentication + CORS | #7 | ✅ Complete | #1, #2 |
| P1 — WiFi provisioning | #8 | ✅ Complete | #3 |
| P2 — Rate limiting | #9 | ✅ Complete | #5, #7 |
| P3 — Error message mapping + TLS documentation | #10 | ✅ Complete | #6 (accepted risk), #16 |

See `docs/SECURITY-PLAN.md` for the full implementation details of each phase.
