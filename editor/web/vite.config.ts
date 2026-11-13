import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'

// The helper (ote-editor) always serves the built files from "/" and falls
// back to index.html for unknown paths, so absolute asset paths are fine.
const helper = 'http://127.0.0.1:8765'

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: { '/api': helper, '/assets': helper, '/player': helper },
  },
  // Not Vite's default "assets": the helper serves its own /assets/ (game fonts) there.
  build: { outDir: 'dist', emptyOutDir: true, sourcemap: false, assetsDir: 'app' },
  test: { include: ['tests/unit/**/*.test.ts'], environment: 'node' },
})
