import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ReactQueryDevtools } from '@tanstack/react-query-devtools';
import { MainLayout } from './layouts/MainLayout';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      retry: 2,
      staleTime: 1000,
    },
  },
});

export function App() {
  return (
    <QueryClientProvider client={queryClient}>
      <MainLayout />
      <ReactQueryDevtools initialIsOpen={false} />
    </QueryClientProvider>
  );
}
