// Electron 主进程：启动 C++ 后端 + 打开 WebView 窗口
import { app, BrowserWindow } from 'electron'
import { spawn, ChildProcess } from 'child_process'
import path from 'path'

let mainWindow: BrowserWindow | null = null
let backendProc: ChildProcess | null = null

// 后端可执行文件路径（C++ 编译产物）
function findBackendPath(): string | null {
  // 开发态：相对于项目根
  const devPath = path.resolve(__dirname, '../../out/build/x64-Debug/Debug/timemachineplus.exe')
  return require('fs').existsSync(devPath) ? devPath : null
}

function startBackend() {
  const exe = findBackendPath()
  if (!exe) {
    console.warn('[Electron] C++ backend not found, UI仍可打开但API不可用')
    return
  }
  try {
    backendProc = spawn(exe, ['ui', '8765'], {
      stdio: 'inherit',
      windowsHide: false,
    })
    console.log('[Electron] backend PID:', backendProc.pid)
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
    webPreferences: {
      contextIsolation: true,
    },
    titleBarStyle: 'hiddenInset',
  })

  if (process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL)
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'))
  }

  mainWindow.on('closed', () => { mainWindow = null })
}

app.whenReady().then(() => {
  startBackend()
  setTimeout(createWindow, 800)  // 等后端起来

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
