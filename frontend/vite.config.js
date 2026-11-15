import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// In development the Vite server (5173) proxies /api to the C++ backend (8080).
// In production the C++ server itself serves the built files from frontend/dist.
export default defineConfig({
  plugins: [react()],
  server: { port: 5173, proxy: { '/api': 'http://127.0.0.1:8080' } },
  build: { outDir: 'dist', sourcemap: false },
});
