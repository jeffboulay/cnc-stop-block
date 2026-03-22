import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiGet, apiPost, apiDelete } from '../api/client';
import type { CutEntry } from '../api/types';

export function useCutList() {
  return useQuery<CutEntry[]>({
    queryKey: ['cutlist'],
    queryFn: () => apiGet<CutEntry[]>('/api/cutlist'),
  });
}

export function useAddCut() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (entry: Omit<CutEntry, 'completed'>) =>
      apiPost('/api/cutlist/add', entry),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['cutlist'] }),
  });
}

export function useRemoveCut() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (index: number) => apiDelete(`/api/cutlist/${index}`),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['cutlist'] }),
  });
}

export function useUpdateCutList() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (entries: CutEntry[]) => apiPost('/api/cutlist', entries),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['cutlist'] }),
  });
}

export function useClearCutList() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: () => apiPost('/api/cutlist/clear'),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['cutlist'] }),
  });
}

export function useResetCutList() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: () => apiPost('/api/cutlist/reset'),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['cutlist'] }),
  });
}
