// Electron 主进程 + preload 打包配置 —— 强制 CJS 格式
import { defineConfig } from 'vite'
import { resolve } from 'path'

export default defineConfig({
  build: {
    outDir: 'dist-electron',
    emptyOutDir: true,
    target: 'node18',
    rollupOptions: {
      input: {
        main: resolve(__dirname, 'electron/main.ts'),
        preload: resolve(__dirname, 'electron/preload.ts'),
      },
      output: {
        format: 'cjs',
        entryFileNames: '[name].cjs',
      },
      external: [
        'electron',
        'child_process',
        'fs', 'path', 'url', 'os', 'crypto',
        'http', 'https', 'stream', 'util', 'events',
        'querystring', 'zlib', 'net', 'buffer',
        'module', 'process', 'assert', 'constants',
        'string_decoder', 'perf_hooks', 'async_hooks',
      ],
    },
    minify: false,
  },
})
