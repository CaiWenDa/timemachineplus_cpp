// C++ 后端 API 封装
const BASE = 'http://127.0.0.1:8765'

async function fetchJson<T>(path: string, opts?: RequestInit): Promise<T> {
  const res = await fetch(BASE + path, {
    headers: { 'Content-Type': 'application/json', ...opts?.headers },
    ...opts,
  })
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`)
  return res.json()
}

export interface BackupTime {
  id: number
  begintime: string | null
  endtime: string | null
  filecopycount: number | null
  datacopycount: number | null
}

export interface Source { id: number; path: string }
export interface Target { id: number; path: string; dir: string; spaceRemain: number }

export interface FileHistoryItem {
  historyId: number
  motifytime: number
  filesize: number
  md5: string
  backupBeginTime: string | null
  fullBackupPath: string
  backupFileExists: boolean
}

export const tmb = {
  health: () => fetchJson<{ status: string }>('/api/health'),
  status: () => fetchJson<{ sources: Source[]; targets: Target[] }>('/api/status'),
  backups: () => fetchJson<{ backups: BackupTime[] }>('/api/backups'),
  sources: () => fetchJson<{ sources: Source[] }>('/api/sources'),
  addSource: (path: string) =>
    fetchJson('/api/sources', { method: 'POST', body: JSON.stringify({ path }) }),
  deleteSource: (id: number) =>
    fetchJson(`/api/sources/${id}`, { method: 'DELETE' }),
  targets: () => fetchJson<{ targets: Target[] }>('/api/targets'),
  addTarget: (path: string) =>
    fetchJson('/api/targets', { method: 'POST', body: JSON.stringify({ path }) }),
  triggerBackup: () =>
    fetchJson('/api/backup', { method: 'POST' }),
  fileHistory: (path: string) =>
    fetchJson<{ filepath: string; fileId: number; history: FileHistoryItem[] }>(
      `/api/files?path=${encodeURIComponent(path)}`
    ),
  restore: (historyId: number, destPath: string) =>
    fetchJson('/api/restore', {
      method: 'POST',
      body: JSON.stringify({ historyId, destPath }),
    }),
}
