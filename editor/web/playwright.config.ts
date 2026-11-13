import { defineConfig, devices } from '@playwright/test'

// Tests run against the REAL ote-editor helper started by global-setup on a
// temp copy of games/ and editor/templates/ (never the repo's own games).
// OTE_EDITOR_BIN points at the binary (default ../../build/ote-editor).
export default defineConfig({
  testDir: 'tests/e2e',
  globalSetup: './tests/e2e/global-setup.ts',
  fullyParallel: true,
  workers: 3,
  retries: 0,
  timeout: 30_000,
  expect: { timeout: 7_000 },
  reporter: [['list']],
  use: { trace: 'off', screenshot: 'off', locale: 'en-US' },
  projects: [
    { name: 'chromium', use: { ...devices['Desktop Chrome'] } },
    // The owner uses Firefox: it runs the scenarios tagged @smoke.
    { name: 'firefox', use: { ...devices['Desktop Firefox'] }, grep: /@smoke/ },
  ],
})
