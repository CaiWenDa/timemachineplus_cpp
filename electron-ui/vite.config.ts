import vue from '@vitejs/plugin-vue'
import electron from 'vite-plugin-electron/simple'
import { defineConfig } from 'vite'

// vite-plugin-electron v1 simple 模式
// main/preload 都强制输出 CJS 格式（.cjs），因为 Electron preload 只稳定支持 CJS
// vite-plugin-electron 内部用 vite lib mode 编译 main，需要用 vite.build.lib 覆盖格式
export default defineConfig({
  base: './',
  plugins: [
    vue(),
    electron({
      main: {
        entry: 'electron/main.ts',
        vite: {
          build: {
            minify: false,
            lib: {
              entry: 'electron/main.ts',
              formats: ['cjs'],
              fileName: () => 'main.cjs',
            },
            rollupOptions: {
              output: {
                format: 'cjs',
                entryFileNames: 'main.cjs',
              },
            },
          },
        },
      },
      preload: {
        input: {
          preload: 'electron/preload.ts',
        },
        vite: {
          build: {
            minify: false,
            rollupOptions: {
              output: { format: 'cjs', entryFileNames: 'preload.cjs' },
            },
          },
        },
      },
    }),
  ],
  server: {
    port: 5173,
  },
})
