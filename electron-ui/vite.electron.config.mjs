// 独立的 Electron 主进程打包配置 —— 强制 CJS 格式
// Electron 主进程在 package.json 无 type:module 时需要 CJS 入口
import { defineConfig } from 'vite'
import { resolve } from 'path'

export default defineConfig({
  build: {
    outDir: 'dist-electron',
    emptyOutDir: true,
    target: 'node18',
    lib: {
      entry: resolve(__dirname, 'electron/main.ts'),
      formats: ['cjs'],
      fileName: () => 'main.cjs',
    },
    rollupOptions: {
      output: {
        format: 'cjs',
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
