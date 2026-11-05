import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

export default defineConfig({
  // base 用相对路径，Electron loadFile() 走 file:// 协议时不会变成 file:///assets/...
  // （绝对 /assets 在 file:// 下被解释为驱动器根路径）
  base: './',
  plugins: [vue()],
  server: {
    port: 5173,
  },
})
