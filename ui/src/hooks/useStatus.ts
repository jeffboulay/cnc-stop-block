import { useEffect, useState } from 'react';
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

/**
 * Returns true when the device is reachable — either the WebSocket is open
 * or we have received a status update (via WS or REST fallback) within the
 * last 5 seconds.
 *
 * Using data freshness rather than raw WS socket state means the indicator
 * stays green during brief WebSocket reconnects as long as REST polling is
 * keeping data current.
 */
export function useIsConnected() {
  const { dataUpdatedAt } = useStatus();

  const { data: wsConnected } = useQuery<boolean>({
    queryKey: ['ws_connected'],
    queryFn: () => Promise.resolve(false),
    initialData: false,
    staleTime: Infinity,
    refetchOnWindowFocus: false,
    refetchOnMount: false,
    refetchOnReconnect: false,
  });

  // Re-evaluate once per second so the indicator reacts promptly when data
  // goes stale (device goes offline) even without a new render trigger.
  const [now, setNow] = useState(Date.now);
  useEffect(() => {
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, []);

  const dataFresh = dataUpdatedAt > 0 && now - dataUpdatedAt < 5000;
  return { data: wsConnected || dataFresh };
}
