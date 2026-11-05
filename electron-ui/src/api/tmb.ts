// C++ 后端 API 封装 —— 通过 Electron preload 暴露的 window.tmb.invoke 走 stdio JSON 行协议
// 不再需要 HTTP fetch，没有端口、没有跨域、没有 health check 轮询

// 声明 window.tmb 的类型（preload.ts 注入）
declare global {
  interface Window {
    tmb: {
      invoke: (method: string, params?: any) => Promise<any>
    }
  }
}

async function invoke<T>(method: string, params?: any): Promise<T> {
  if (!window.tmb?.invoke) {
    throw new Error('window.tmb 未就绪（Electron preload 未加载？）')
  }
  return window.tmb.invoke(method, params)
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
  health: () => invoke<{ status: string }>('health'),

  status: () => invoke<{ sources: Source[]; targets: Target[] }>('status'),

  backups: () => invoke<{ backups: BackupTime[] }>('backups'),

  // sources / targets 从 status 拆包，后端 status 一次性返回两者
  sources: async () => {
    const r = await invoke<{ sources: Source[] }>('status')
    return { sources: r.sources }
  },
  addSource: (path: string) => invoke<void>('add_source', { path }),
  deleteSource: (id: number) => invoke<void>('delete_source', { id }),

  targets: async () => {
    const r = await invoke<{ targets: Target[] }>('status')
    return { targets: r.targets }
  },
  addTarget: (path: string) => invoke<void>('add_target', { path }),
  deleteTarget: (id: number) => invoke<void>('delete_target', { id }),

  triggerBackup: () => invoke<{ status: string }>('trigger_backup'),

  backupFiles: (backupId: number) =>
    invoke<{ backupId: number; files: BackupFile[] }>('backup_files', { backupId }),

  search: (name: string) =>
    invoke<{ keyword: string; results: SearchResult[] }>('search', { name }),

  fileHistory: (path: string) =>
    invoke<{ filepath: string; fileId: number; history: FileHistoryItem[] }>('file_history', { path }),

  restore: (historyId: number, destPath: string) =>
    invoke<{ ok: boolean; restoredTo: string }>('restore', { historyId, destPath }),
}
