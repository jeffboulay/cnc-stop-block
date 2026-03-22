import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    // Proxy API requests to ESP32 during development
    proxy: {
      '/api': {
        target: 'http://192.168.1.100', // Change to your ESP32's IP
        changeOrigin: true,
      },
      '/ws': {
        target: 'ws://192.168.1.100',
        ws: true,
      },
    },
  },
})
