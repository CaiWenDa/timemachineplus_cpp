// C++ 后端 API 封装
const BASE = 'http://127.0.0.1:8765'

async function fetchJson<T>(path: string, opts?: RequestInit): Promise<T> {
  const res = await fetch(BASE + path, {
    headers: { 'Content-Type': 'application/json', ...opts?.headers },
    ...opts,
  })
  if (!res.ok) {
    // 优先使用后端返回的可读错误信息
    let msg = `${res.status} ${res.statusText}`
    try {
      const j = await res.json()
      if (j?.error) msg = j.error
    } catch { /* 无 JSON body 时用状态码 */ }
    throw new Error(msg)
  }
  // 204 No Content（如删除成功）没有响应体
  if (res.status === 204) return undefined as T
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

// 某个备份版本（时间点）下的文件条目
export interface BackupFile {
  historyId: number
  filepath: string
  filesize: number
  motifytime: number
  md5: string
}

// 跨版本搜索结果条目（每条标注所属备份版本）
export interface SearchResult {
  historyId: number
  filepath: string
  backupId: number
  backupBeginTime: string | null
  filesize: number
  motifytime: number
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
  deleteTarget: (id: number) =>
    fetchJson(`/api/targets/${id}`, { method: 'DELETE' }),
  triggerBackup: () =>
    fetchJson('/api/backup', { method: 'POST' }),
  backupFiles: (backupId: number) =>
    fetchJson<{ backupId: number; files: BackupFile[] }>(
      `/api/backups/${backupId}/files`
    ),
  search: (name: string) =>
    fetchJson<{ keyword: string; results: SearchResult[] }>(
      `/api/search?name=${encodeURIComponent(name)}`
    ),
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
