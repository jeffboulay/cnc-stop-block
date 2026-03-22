import { useQuery } from '@tanstack/react-query';
import { apiGet } from '../api/client';
import type { SystemStatus } from '../api/types';

/**
 * Returns live system status. Data is primarily updated via WebSocket
 * (see useWebSocket), but this query also polls as a fallback every 2s.
 */
export function useStatus() {
  return useQuery<SystemStatus>({
    queryKey: ['status'],
    queryFn: () => apiGet<SystemStatus>('/api/status'),
    refetchInterval: 2000, // Fallback polling if WebSocket disconnects
  });
}

export function useIsConnected() {
  return useQuery<boolean>({
    queryKey: ['ws_connected'],
    // queryFn never actually runs — the WS hook sets this via setQueryData.
    // staleTime: Infinity prevents TanStack Query from refetching and
    // overwriting the WS-managed value with false.
    queryFn: () => Promise.resolve(false),
    initialData: false,
    staleTime: Infinity,
    refetchOnWindowFocus: false,
    refetchOnMount: false,
    refetchOnReconnect: false,
  });
}
