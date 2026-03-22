import { useMutation, useQueryClient } from '@tanstack/react-query';
import { apiPost } from '../api/client';

function useCommand(path: string) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: () => apiPost(path),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['status'] }),
  });
}

function useCommandWithBody<T>(path: string) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (body: T) => apiPost(path, body),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['status'] }),
  });
}

export function useHome() {
  return useCommand('/api/home');
}

export function useGoTo() {
  return useCommandWithBody<{ position_mm: number }>('/api/goto');
}

export function useJog() {
  return useCommandWithBody<{ distance_mm: number }>('/api/jog');
}

export function useLock() {
  return useCommand('/api/lock');
}

export function useUnlock() {
  return useCommand('/api/unlock');
}

export function useEStop() {
  return useCommand('/api/estop');
}

export function useReset() {
  return useCommand('/api/reset');
}

export function useStartCut() {
  return useCommand('/api/cut/start');
}

export function useEndCut() {
  return useCommand('/api/cut/end');
}

export function useNextCut() {
  return useCommand('/api/cut/next');
}
