import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiGet, apiPost, apiDelete } from '../api/client';
import type { ToolInfo } from '../api/types';

export function useTools() {
  return useQuery<ToolInfo[]>({
    queryKey: ['tools'],
    queryFn: () => apiGet<ToolInfo[]>('/api/tools'),
  });
}

export function useRegisterTool() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (tool: ToolInfo) => apiPost('/api/tools', tool),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['tools'] }),
  });
}

export function useRemoveTool() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (uid: string) => apiDelete(`/api/tools/${uid}`),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['tools'] }),
  });
}
