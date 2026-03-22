// Base URL for the ESP32 API.
// In development, Vite proxy handles /api requests.
// In production (served from a different host), set VITE_API_URL env var.
const BASE_URL = import.meta.env.VITE_API_URL ?? '';

export async function apiGet<T>(path: string): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`);
  if (!response.ok) {
    const body = await response.json().catch(() => ({}));
    throw new Error((body as { error?: string }).error ?? `API error: ${response.status}`);
  }
  return response.json() as Promise<T>;
}

export async function apiPost<T = { ok: boolean }>(
  path: string,
  body?: unknown,
): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  if (!response.ok) {
    const data = await response.json().catch(() => ({}));
    throw new Error((data as { error?: string }).error ?? `API error: ${response.status}`);
  }
  return response.json() as Promise<T>;
}

export async function apiDelete<T = { ok: boolean }>(path: string): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`, { method: 'DELETE' });
  if (!response.ok) {
    const data = await response.json().catch(() => ({}));
    throw new Error((data as { error?: string }).error ?? `API error: ${response.status}`);
  }
  return response.json() as Promise<T>;
}
