import type { ArchiveRecord, JobConfig, JobSnapshot, NativeEvent, PositionState } from '../shared/types';

async function request<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetch(url, { ...init, headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) } });
  const body = response.status === 204 ? undefined : await response.json();
  if (!response.ok) throw new Error(body?.error ?? `Request failed (${response.status})`);
  return body as T;
}

export const api = {
  get: <T>(url: string) => request<T>(url),
  post: <T>(url: string, body: unknown) => request<T>(url, { method: 'POST', body: JSON.stringify(body) }),
  patch: <T>(url: string, body: unknown) => request<T>(url, { method: 'PATCH', body: JSON.stringify(body) }),
  delete: (url: string) => request<void>(url, { method: 'DELETE' }),
  inspect: (moves: number[]) => request<PositionState>('/api/positions/inspect', { method: 'POST', body: JSON.stringify({ moves }) }),
  archive: (name: string, kind: 'position' | 'play', payload: unknown) =>
    request<ArchiveRecord>('/api/archive', { method: 'POST', body: JSON.stringify({ name, kind, payload }) })
};

export function streamJob(config: JobConfig, onEvent: (event: NativeEvent) => void,
                          onSnapshot: (snapshot: JobSnapshot) => void) {
  let source: EventSource | undefined;
  let cancelled = false;
  let rejectJob: ((reason: Error) => void) | undefined;
  const promise = api.post<JobSnapshot>('/api/jobs', config).then((created) => {
    onSnapshot(created);
    sessionStorage.setItem('connect4-active-job', created.id);
    return new Promise<unknown>((resolve, reject) => {
      rejectJob = reject;
      source = new EventSource(`/api/jobs/${created.id}/events`);
      source.addEventListener('snapshot', (raw) => {
        const snapshot = JSON.parse((raw as MessageEvent).data) as JobSnapshot;
        onSnapshot(snapshot);
        if (snapshot.status === 'complete') {
          source?.close(); sessionStorage.removeItem('connect4-active-job'); resolve(snapshot.result);
        } else if (snapshot.status === 'failed' || snapshot.status === 'cancelled') {
          source?.close(); sessionStorage.removeItem('connect4-active-job'); reject(new Error(snapshot.error ?? `Job ${snapshot.status}`));
        }
      });
      source.addEventListener('native', (raw) => {
        const event = JSON.parse((raw as MessageEvent).data) as NativeEvent;
        onEvent(event);
        if (event.type === 'complete') {
          source?.close(); sessionStorage.removeItem('connect4-active-job'); resolve(event.result);
        } else if (event.type === 'error') {
          source?.close(); sessionStorage.removeItem('connect4-active-job'); reject(new Error(event.message));
        }
      });
      source.onerror = async () => {
        source?.close();
        try {
          const snapshot = await api.get<JobSnapshot>(`/api/jobs/${created.id}`);
          onSnapshot(snapshot);
          if (snapshot.status === 'complete') resolve(snapshot.result);
          else if (snapshot.status === 'cancelled') reject(new Error('Job cancelled'));
          else reject(new Error(snapshot.error ?? 'Job stream disconnected'));
        } catch (error) { reject(error); }
      };
      if (cancelled) void api.delete(`/api/jobs/${created.id}`);
    });
  });
  return {
    promise,
    cancel: async () => {
      cancelled = true; source?.close();
      const id = sessionStorage.getItem('connect4-active-job');
      if (id) await api.delete(`/api/jobs/${id}`);
      sessionStorage.removeItem('connect4-active-job');
      rejectJob?.(new Error('Job cancelled'));
    }
  };
}

