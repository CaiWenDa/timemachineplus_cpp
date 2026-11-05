// Electron 主进程：启动 C++ 后端 + 打开 WebView 窗口
import { app, BrowserWindow } from 'electron'
import { spawn } from 'child_process'
import path from 'path'
import fs from 'fs'
import os from 'os'

// --- 沙箱规避：把 Electron userData / appData / logs 重定向到可写目录 ---
// 受限环境（如 IDE sandbox）下 %APPDATA% 可能不可写，导致 Electron 启动即 Access Violation
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

// vite 输出 CJS 时 __dirname 天然可用
let mainWindow: BrowserWindow | null = null
let backendProc: ReturnType<typeof spawn> | null = null

// 后端可执行文件路径（C++ 编译产物）
function findBackendPath(): string | null {
  const devPath = path.resolve(__dirname, '../../out/build/x64-Debug/Debug/timemachineplus.exe')
  return fs.existsSync(devPath) ? devPath : null
}

function startBackend() {
  const exe = findBackendPath()
  if (!exe) {
    console.warn('[Electron] C++ backend not found, UI仍可打开但API不可用')
    return
  }
  // C++ 后端期望在项目根目录下运行（找 timemachine.sql / timemachine.db）
  // 显式传 TIMEMACHINE_DB / TIMEMACHINE_SCHEMA 环境变量避免 cwd 歧义
  const projectRoot = path.resolve(__dirname, '../../')
  const env = {
    ...process.env,
    TIMEMACHINE_DB: path.join(projectRoot, 'timemachine.db'),
    TIMEMACHINE_SCHEMA: path.join(projectRoot, 'timemachine.sql'),
  }
  try {
    backendProc = spawn(exe, ['ui', '8765'], {
      cwd: projectRoot,
      stdio: 'inherit',
      windowsHide: false,
      env,
    })
    console.log('[Electron] backend PID:', backendProc.pid)
    console.log('[Electron] backend cwd  :', projectRoot)
    console.log('[Electron] TIMEMACHINE_DB   :', env.TIMEMACHINE_DB)
    console.log('[Electron] TIMEMACHINE_SCHEMA:', env.TIMEMACHINE_SCHEMA)
  } catch (e) {
    console.error('[Electron] failed to start backend:', e)
  }
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 820,
    minWidth: 960,
    minHeight: 600,
    backgroundColor: '#1a1a2e',
    webPreferences: { contextIsolation: true },
    titleBarStyle: 'hiddenInset',
  })

  if (process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL)
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'))
  }

  mainWindow.on('closed', () => { mainWindow = null })
}

// 等后端真的起来再开窗口——避免前端 fetch 127.0.0.1:8765 空等
async function waitForBackend(port = 8765, maxAttempts = 30) {
  for (let i = 0; i < maxAttempts; i++) {
    try {
      const res = await fetch(`http://127.0.0.1:${port}/api/health`)
      if (res.ok) {
        console.log(`[Electron] backend ready after ${(i + 1) * 200}ms`)
        return true
      }
    } catch {}
    await new Promise(r => setTimeout(r, 200))
  }
  console.warn('[Electron] backend health check timeout, opening window anyway')
  return false
}

app.whenReady().then(async () => {
  startBackend()
  await waitForBackend(8765)
  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow()
  })
})

app.on('window-all-closed', () => {
  if (backendProc && backendProc.pid) {
    try { process.kill(backendProc.pid); console.log('[Electron] backend killed') } catch {}
  }
  if (process.platform !== 'darwin') app.quit()
})

app.on('before-quit', () => {
  if (backendProc && backendProc.pid) {
    try { process.kill(backendProc.pid) } catch {}
  }
})
