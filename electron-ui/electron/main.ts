// Electron 主进程：启动 C++ 后端（stdio JSON 行协议） + IPC 桥接
import { app, BrowserWindow, ipcMain } from 'electron'
import { spawn, ChildProcess } from 'child_process'
import path from 'path'
import fs from 'fs'
import os from 'os'
import { EventEmitter } from 'events'
import { fileURLToPath } from 'url'

// ESM 里没有 __dirname，做一个 CJS/ESM 双模式兼容
// eslint-disable-next-line @typescript-eslint/naming-convention
const __dirname = (() => {
  // @ts-ignore CJS 模式下存在 __dirname
  if (typeof __dirname !== 'undefined') return __dirname
  return path.dirname(fileURLToPath(import.meta.url))
})()

// --- 沙箱规避 ---
const REDIRECT_DATA = process.env.ELECTRON_REDIRECT_DATA !== '0'
const writableBase = path.join(os.tmpdir(), 'timemachineplus-electron-data')
if (REDIRECT_DATA) {
  for (const dir of ['userData', 'appData', 'sessionData', 'logs']) {
    const full = path.join(writableBase, dir)
    fs.mkdirSync(full, { recursive: true })
    app.setPath(dir, full)
  }
  console.log('[Electron] redirected data dir ->', writableBase)
}

// ==================== 后端 stdio 协议客户端 ====================

interface PendingCall {
  resolve: (v: any) => void
  reject: (e: Error) => void
  timeout: NodeJS.Timeout
}

class BackendClient extends EventEmitter {
  private proc: ChildProcess | null = null
  private nextId = 1
  private pending = new Map<number, PendingCall>()
  private buffer = ''
  private projectRoot = ''
  private shuttingDown = false  // 幂等保护：防止 stop() 被重复调用

  get ready() { return this.proc !== null && !this.proc.killed && !this.shuttingDown }

  start(): boolean {
    const devPath = path.resolve(__dirname, '../../out/build/x64-Debug/Debug/timemachineplus.exe')
    const exe = fs.existsSync(devPath) ? devPath : null
    if (!exe) {
      console.warn('[Electron] C++ backend not found at', devPath)
      this.emit('error', new Error('backend exe not found'))
      return false
    }

    this.projectRoot = path.resolve(__dirname, '../../')
    const env = {
      ...process.env,
      TIMEMACHINE_DB: path.join(this.projectRoot, 'timemachine.db'),
      TIMEMACHINE_SCHEMA: path.join(this.projectRoot, 'timemachine.sql'),
    }

    this.proc = spawn(exe, ['--stdio'], {
      cwd: this.projectRoot,
      stdio: ['pipe', 'pipe', 'inherit'],  // stdin/stdout pipe, stderr inherit
      windowsHide: false,
      env,
    })

    // 后端 stdout：逐行解析 JSON 响应
    this.proc.stdout!.on('data', (chunk: Buffer) => {
      this.buffer += chunk.toString('utf8')
      let idx: number
      while ((idx = this.buffer.indexOf('\n')) !== -1) {
        const line = this.buffer.slice(0, idx)
        this.buffer = this.buffer.slice(idx + 1)
        if (!line.trim()) continue
        this.handleResponse(line)
      }
    })

    this.proc.on('exit', (code) => {
      console.log('[Electron] backend exited with code', code)
      this.proc = null
      // 如果是我们主动 stop() 引起的退出，resolve pending 为 shutdown 标记，
      // 而不是 reject 成 "backend exited" —— 避免渲染进程误弹窗
      const graceful = this.shuttingDown
      for (const [, p] of this.pending) {
        clearTimeout(p.timeout)
        if (graceful) {
          p.resolve({ __shutdown: true })
        } else {
          p.reject(new Error('backend exited unexpectedly'))
        }
      }
      this.pending.clear()
      this.emit('exit', code)
    })

    this.proc.on('error', (err) => {
      console.error('[Electron] backend spawn error:', err)
      this.emit('error', err)
    })

    console.log('[Electron] backend PID:', this.proc.pid)
    return true
  }

  private handleResponse(line: string) {
    let resp: any
    try { resp = JSON.parse(line) }
    catch (e) {
      console.error('[Electron] failed to parse response:', line.slice(0, 200))
      return
    }

    const id = resp.id
    const pending = this.pending.get(id)
    if (!pending) {
      console.warn('[Electron] orphan response id=', id)
      return
    }
    this.pending.delete(id)
    clearTimeout(pending.timeout)

    if (resp.error) {
      pending.reject(new Error(resp.error.message || `code ${resp.error.code}`))
    } else {
      pending.resolve(resp.result)
    }
  }

  invoke(method: string, params: any = {}): Promise<any> {
    return new Promise((resolve, reject) => {
      if (!this.proc || this.proc.killed || this.shuttingDown) {
        reject(new Error('backend not ready'))
        return
      }
      const id = this.nextId++
      const req = JSON.stringify({ id, method, params })
      const timeout = setTimeout(() => {
        this.pending.delete(id)
        reject(new Error(`backend timeout (${method})`))
      }, 30_000)
      this.pending.set(id, { resolve, reject, timeout })
      this.proc!.stdin!.write(req + '\n')
    })
  }

  async healthCheck(): Promise<boolean> {
    try {
      const r = await this.invoke('health')
      return r?.status === 'ok'
    } catch { return false }
  }

  // 幂等关闭：只执行一次
  async stop(): Promise<void> {
    if (this.shuttingDown) return
    this.shuttingDown = true

    const proc = this.proc
    if (!proc || proc.killed) {
      this.proc = null
      return
    }

    try {
      // 1. 先发 shutdown 指令让后端优雅退出（如果 stdin 还能写）
      if (proc.stdin && !proc.stdin.destroyed) {
        try {
          proc.stdin.write(JSON.stringify({ id: -1, method: 'shutdown', params: {} }) + '\n')
          proc.stdin.end()  // 关闭 stdin，让后端 getline 读到 EOF
        } catch {}
      }

      // 2. 等最多 1.5 秒让后端优雅退出
      await new Promise<void>((resolve) => {
        const graceful = setTimeout(resolve, 1500)
        proc.once('exit', () => {
          clearTimeout(graceful)
          resolve()
        })
      })
    } catch {}

    // 3. 还没死就强杀（Windows 上用 taskkill /F，跨平台用 proc.kill('SIGKILL')）
    if (!proc.killed) {
      try {
        if (process.platform === 'win32') {
          // Windows: taskkill /F 确保强制终止子进程树
          require('child_process').spawnSync('taskkill', ['/F', '/T', '/PID', String(proc.pid)])
        } else {
          proc.kill('SIGKILL')
        }
      } catch {}
    }

    this.proc = null
    console.log('[Electron] backend stopped')
  }
}

// ==================== Electron 生命周期 ====================

let mainWindow: BrowserWindow | null = null
const backend = new BackendClient()
let appQuitting = false  // 防止 window-all-closed 里重复触发 app.quit()

async function waitForBackend(maxAttempts = 30): Promise<boolean> {
  for (let i = 0; i < maxAttempts; i++) {
    if (await backend.healthCheck()) {
      console.log(`[Electron] backend ready after ${(i + 1) * 200}ms`)
      return true
    }
    await new Promise(r => setTimeout(r, 200))
  }
  console.warn('[Electron] backend health check timeout')
  return false
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 960,
    height: 600,
    minWidth: 960,
    minHeight: 600,
    backgroundColor: '#1a1a2e',
    webPreferences: {
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.cjs'),
    },
    titleBarStyle: 'hiddenInset',
    autoHideMenuBar: true,  // 隐藏默认菜单栏（按 Alt 可临时显示）
  })

  // vite-plugin-electron 在 dev 模式注入 VITE_DEV_SERVER_URL
  if (process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL)
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'))
  }

  mainWindow.on('closed', () => { mainWindow = null })
}

// IPC 桥接：渲染进程通过 window.tmb.invoke(method, params) 调用
ipcMain.handle('tmb:invoke', async (_event, method: string, params?: any) => {
  if (backend['shuttingDown']) {
    throw new Error('应用正在关闭，请稍候')
  }
  return backend.invoke(method, params || {})
})

// —— 优雅退出序列 ——
// Electron 退出顺序：
//   1. 用户点关闭 → mainWindow.on('closed') → mainWindow = null
//   2. window-all-closed 触发 → backend.stop() → app.quit()
//   3. before-quit 触发（app.quit() 自动触发，这里不能再 stop）
// 所以把 backend.stop() 只放在 window-all-closed，before-quit 里不做事
app.on('window-all-closed', async () => {
  if (appQuitting) return
  appQuitting = true
  console.log('[Electron] window-all-closed, shutting down backend...')
  await backend.stop()
  if (process.platform !== 'darwin') app.quit()
})

// before-quit 里只打日志，不再 stop —— 避免重复调用和竞态
app.on('before-quit', () => {
  console.log('[Electron] before-quit')
})

app.whenReady().then(async () => {
  backend.start()
  await waitForBackend()
  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow()
  })
})
