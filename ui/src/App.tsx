import { useState, useEffect } from 'react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ReactQueryDevtools } from '@tanstack/react-query-devtools';
import { MainLayout } from './layouts/MainLayout';
import { TokenSetup } from './components/TokenSetup';
import { getToken } from './api/auth';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      retry: 2,
      staleTime: 1000,
    },
  },
});

export function App() {
  const [paired, setPaired] = useState(() => getToken() !== null);

  // Listen for 401 events fired by apiClient when the token is rejected
  useEffect(() => {
    const handleUnauthorized = () => setPaired(false);
    window.addEventListener('cnc:unauthorized', handleUnauthorized);
    return () => window.removeEventListener('cnc:unauthorized', handleUnauthorized);
  }, []);

  if (!paired) {
    return (
      <TokenSetup
        onPaired={() => {
          setPaired(true);
          // Reconnect WebSocket and refetch status with the new token
          queryClient.invalidateQueries();
        }}
      />
    );
  }

  return (
    <QueryClientProvider client={queryClient}>
      <MainLayout />
      <ReactQueryDevtools initialIsOpen={false} />
    </QueryClientProvider>
  );
}
