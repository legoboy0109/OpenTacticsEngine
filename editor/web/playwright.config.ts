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
    // The test browsers are MUTED: the Sound screen's previews and the web player (Play) really play, and a headless
    // browser still reaches the machine's speakers. The tests judge sound by what the page reports, never by ear.
    { name: 'chromium', use: { ...devices['Desktop Chrome'], launchOptions: { args: ['--mute-audio'] } } },
    // The owner uses Firefox: it runs the scenarios tagged @smoke.
    {
      name: 'firefox',
      use: { ...devices['Desktop Firefox'], launchOptions: { firefoxUserPrefs: { 'media.volume_scale': '0.0' } } },
      grep: /@smoke/,
    },
  ],
})
