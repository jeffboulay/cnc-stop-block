/**
 * Token storage helpers — persists the ESP32 API bearer token in localStorage.
 *
 * The token is generated on the ESP32 at first boot, printed to Serial, and
 * entered once on the UI pairing screen. It survives page refreshes and app
 * updates until the user explicitly clears it or rotates the token on the device.
 *
 * Dev mode: set VITE_DEV_TOKEN in ui/.env.local to skip the pairing screen
 * when running against the simulator. The env var is never bundled into
 * production builds (Vite strips unknown VITE_ vars at build time when
 * mode !== 'development').
 */

const TOKEN_KEY = 'cnc_api_token';

/**
 * In development, auto-seed localStorage from VITE_DEV_TOKEN so the pairing
 * screen is bypassed when working against the simulator. This runs once on
 * module load — it will not overwrite a token that was already manually set.
 */
if (import.meta.env.DEV && import.meta.env.VITE_DEV_TOKEN) {
  const devToken = String(import.meta.env.VITE_DEV_TOKEN).trim();
  if (/^[0-9a-fA-F]{64}$/.test(devToken) && !localStorage.getItem(TOKEN_KEY)) {
    localStorage.setItem(TOKEN_KEY, devToken);
  }
}

/** Return the stored token, or null if not yet paired. */
export function getToken(): string | null {
  return localStorage.getItem(TOKEN_KEY);
}

/** Persist a new token after successful pairing. */
export function setToken(token: string): void {
  localStorage.setItem(TOKEN_KEY, token.trim());
}

/** Clear the stored token and return the UI to the pairing screen. */
export function clearToken(): void {
  localStorage.removeItem(TOKEN_KEY);
}

/** Return the Authorization header value for the current token. */
export function authHeader(): Record<string, string> {
  const token = getToken();
  if (!token) return {};
  return { Authorization: `Bearer ${token}` };
}
