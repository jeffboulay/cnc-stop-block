/**
 * Token storage helpers — persists the ESP32 API bearer token in localStorage.
 *
 * The token is generated on the ESP32 at first boot, printed to Serial, and
 * entered once on the UI pairing screen. It survives page refreshes and app
 * updates until the user explicitly clears it or rotates the token on the device.
 */

const TOKEN_KEY = 'cnc_api_token';

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
