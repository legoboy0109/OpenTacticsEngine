#!/usr/bin/env node
// Browser checks for the web build of ote-player.
//
//   node run.mjs [--build DIR] [--out DIR] [--only a,b] [--browser chromium|firefox]
//
// --build  the web build output (default ../../build-web/engine/player)
// --out    screenshots go here (default ./out, gitignored)
// --only   comma list of suites: standalone pointer resize embed saves campaign views render3d audio perf hex tiles scroll software smoke
//          (default: all but smoke in chromium; smoke is the Firefox run)
// --browser  engine for the selected suites (default chromium; "firefox" runs
//          only the smoke, saves, campaign, views and render3d suites unless --only says otherwise)
//
// Exit code 0 = every check passed. Install once: npm install && npx playwright install chromium firefox
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium, firefox } from 'playwright';
import { startServer } from './lib/server.mjs';
import { Checks } from './lib/checks.mjs';
import { ensureDir } from './lib/session.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opt = (n, d) => { const i = argv.indexOf('--' + n); return i >= 0 ? argv[i + 1] : d; };

const build = path.resolve(opt('build', path.join(here, '../../build-web/engine/player')));
const out = ensureDir(path.resolve(opt('out', path.join(here, 'out'))));
const browserName = opt('browser', 'chromium');
const all = ['standalone', 'pointer', 'resize', 'embed', 'saves', 'campaign', 'views', 'render3d', 'audio', 'perf', 'hex', 'tiles', 'scroll', 'software', 'smoke'];
const only = opt('only', browserName === 'firefox' ? 'smoke,saves,campaign,views,render3d' : all.filter((n) => n !== 'smoke').join(',')).split(',');

const checks = new Checks();
const server = await startServer(build);
// Muted: the web player really plays its music and effects, and a headless browser still reaches the speakers.
const browser = await (browserName === 'firefox'
  ? firefox.launch({ firefoxUserPrefs: { 'media.volume_scale': '0.0' } })
  : chromium.launch({ args: ['--mute-audio'] }));
const analysisPage = await browser.newPage();
const contexts = [];

const env = {
  checks, out, build, browser, browserName, analysisPage,
  url: server.url,
  server,
  // A fresh browser context (own storage) with one page.
  async newContext(options = {}) {
    const context = await browser.newContext(options);
    const page = await context.newPage();
    contexts.push(context);
    return { context, page, close: () => context.close() };
  },
};

for (const name of only) {
  console.log(`\n== ${name} (${browserName}) ==`);
  try {
    const suite = await import(`./suites/${name}.mjs`);
    await suite.run(env);
  } catch (e) {
    checks.check(`suite ${name} ran to the end`, false, e.stack || String(e));
  }
}

await browser.close();
await server.close();
process.exit(checks.summary() ? 0 : 1);
