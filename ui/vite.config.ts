import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Set VITE_PROXY_TARGET to your ESP32's IP, e.g.:
//   VITE_PROXY_TARGET=http://192.168.1.100 npm run dev
const proxyTarget = process.env.VITE_PROXY_TARGET ?? 'http://192.168.1.100'

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': {
        target: proxyTarget,
        changeOrigin: true,
      },
      '/ws': {
        target: proxyTarget.replace(/^http/, 'ws'),
        ws: true,
      },
    },
  },
})
