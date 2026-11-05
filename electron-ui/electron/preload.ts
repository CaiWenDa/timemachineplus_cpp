// preload.ts：在 contextIsolation=true 下安全地暴露 IPC 能力给渲染进程
import { contextBridge, ipcRenderer } from 'electron'

contextBridge.exposeInMainWorld('tmb', {
  invoke: (method: string, params?: any) =>
    ipcRenderer.invoke('tmb:invoke', method, params ?? {}),
})
