import { useEffect, useRef } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import type { SystemStatus } from './types';

/**
 * Connects to the ESP32 WebSocket and pushes status updates
 * directly into the TanStack Query cache at key ["status"].
 *
 * Reconnects automatically with exponential backoff.
 */
export function useWebSocket() {
  const queryClient = useQueryClient();
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectDelayRef = useRef(1000);

  useEffect(() => {
    let mounted = true;
    let timeoutId: ReturnType<typeof setTimeout>;

    function connect() {
      if (!mounted) return;

      const baseUrl = import.meta.env.VITE_API_URL ?? window.location.origin;
      const wsUrl = baseUrl.replace(/^http/, 'ws') + '/ws';

      const ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        reconnectDelayRef.current = 1000;
        queryClient.setQueryData(['ws_connected'], true);
      };

      ws.onmessage = (event: MessageEvent) => {
        try {
          const data = JSON.parse(event.data as string) as SystemStatus;
          queryClient.setQueryData(['status'], data);
        } catch {
          // Ignore malformed messages
        }
      };

      ws.onclose = () => {
        queryClient.setQueryData(['ws_connected'], false);
        if (mounted) {
          timeoutId = setTimeout(connect, reconnectDelayRef.current);
          reconnectDelayRef.current = Math.min(reconnectDelayRef.current * 2, 10000);
        }
      };

      ws.onerror = () => {
        ws.close();
      };
    }

    connect();

    return () => {
      mounted = false;
      clearTimeout(timeoutId);
      wsRef.current?.close();
    };
  }, [queryClient]);
}
