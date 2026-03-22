// Base URL for the ESP32 API.
// In development, Vite proxy handles /api requests.
// In production (served from a different host), set VITE_API_URL env var.
const BASE_URL = import.meta.env.VITE_API_URL ?? '';

import { authHeader, clearToken } from './auth';

/** Handle a non-OK fetch response — throw with the server's error message. */
async function handleError(response: Response): Promise<never> {
  // 401: token is wrong or missing — clear it so the pairing screen shows
  if (response.status === 401) {
    clearToken();
    window.dispatchEvent(new Event('cnc:unauthorized'));
  }
  const body = await response.json().catch(() => ({}));
  throw new Error((body as { error?: string }).error ?? `API error: ${response.status}`);
}

export async function apiGet<T>(path: string): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`, {
    headers: authHeader(),
  });
  if (!response.ok) return handleError(response);
  return response.json() as Promise<T>;
}

export async function apiPost<T = { ok: boolean }>(
  path: string,
  body?: unknown,
): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: {
      ...authHeader(),
      ...(body ? { 'Content-Type': 'application/json' } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  if (!response.ok) return handleError(response);
  return response.json() as Promise<T>;
}

export async function apiDelete<T = { ok: boolean }>(path: string): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`, {
    method: 'DELETE',
    headers: authHeader(),
  });
  if (!response.ok) return handleError(response);
  return response.json() as Promise<T>;
}
