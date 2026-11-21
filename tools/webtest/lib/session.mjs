// One browser page with console capture and helpers for driving the player.
import fs from 'node:fs';
import path from 'node:path';

export class Session {
  constructor(page, outDir, label) {
    this.page = page;
    this.outDir = outDir;
    this.label = label;
    this.logs = [];     // every console line: "type: text"
    this.errors = [];   // console errors, page errors, failed requests
    this.shots = 0;
    page.on('console', (m) => {
      this.logs.push(`${m.type()}: ${m.text()}`);
      if (m.type() === 'error') this.errors.push('console: ' + m.text());
    });
    page.on('pageerror', (e) => this.errors.push('pageerror: ' + e.message));
    page.on('requestfailed', (r) => this.errors.push('request failed: ' + r.url()));
  }

  // Console errors that are expected by a test can be dropped with this.
  ignoreErrors(re) { this.errors = this.errors.filter((e) => !re.test(e)); }

  async goto(url) {
    this.t0 = Date.now();
    await this.page.goto(url, { waitUntil: 'load' });
  }

  // Waits for the player's ready marker (set after the first frames). Returns ms since navigation start.
  async waitReady(timeout = 30000) {
    await this.page.waitForFunction(() => document.documentElement.dataset.ote === 'ready', null, { timeout });
    return this.page.evaluate(() => window.__oteReadyMs);
  }

  async waitLog(re, timeout = 10000) {
    const t = Date.now();
    while (Date.now() - t < timeout) {
      if (this.logs.some((l) => re.test(l))) return;
      await this.page.waitForTimeout(50);
    }
    throw new Error(`log line ${re} not seen; last logs:\n` + this.logs.slice(-8).join('\n'));
  }

  // Presses keys one by one, with a pause so the engine draws between them.
  async keys(list, pause = 150) {
    for (const k of list) {
      await this.page.keyboard.press(k);
      await this.page.waitForTimeout(pause);
    }
  }

  // Click in CSS pixels of the page.
  async click(x, y, pause = 200) {
    await this.page.mouse.click(x, y);
    await this.page.waitForTimeout(pause);
  }
  async tap(x, y, pause = 200) {
    await this.page.touchscreen.tap(x, y);
    await this.page.waitForTimeout(pause);
  }

  // Logical game pixels (default view 480x270) -> page CSS pixels. The player shows its picture at a whole multiple of the view, centred
  // (the "Whole-number scaling" setting, default on: docs/GRAPHICS.md), measured in the canvas's own (device) pixels; below 1x, or
  // with `s.wholeScale = false` (a run with --integer-scale off), it is letterboxed (centred, aspect kept) instead.
  async toPage(lx, ly, lw = 480, lh = 270) {
    const c = await this.canvasInfo();
    const [bw, bh] = c.backing, d = bw / c.css[0];  // backing pixels per CSS pixel
    const whole = Math.min(Math.floor(bw / lw), Math.floor(bh / lh));
    if (this.wholeScale !== false && whole >= 1)
      return [(Math.floor((bw - lw * whole) / 2) + lx * whole) / d, (Math.floor((bh - lh * whole) / 2) + ly * whole) / d];
    const k = Math.min(c.css[0] / lw, c.css[1] / lh);
    return [(c.css[0] - lw * k) / 2 + lx * k, (c.css[1] - lh * k) / 2 + ly * k];
  }
  async clickL(lx, ly, pause = 250) { const [x, y] = await this.toPage(lx, ly); await this.click(x, y, pause); }
  async tapL(lx, ly, pause = 250) { const [x, y] = await this.toPage(lx, ly); await this.tap(x, y, pause); }

  // Screenshot of the whole page into outDir/<label>-<name>.png. Returns the PNG bytes.
  async shot(name) {
    const file = path.join(this.outDir, `${this.label}-${name}.png`);
    return this.page.screenshot({ path: file });
  }

  // Canvas layout facts: CSS box, backing store size, devicePixelRatio, page scroll.
  async canvasInfo() {
    return this.page.evaluate(() => {
      const c = document.getElementById('canvas');
      const r = c.getBoundingClientRect();
      return {
        css: [Math.round(r.width * 100) / 100, Math.round(r.height * 100) / 100],
        backing: [c.width, c.height],
        dpr: window.devicePixelRatio,
        inner: [innerWidth, innerHeight],
        scroll: [scrollX, scrollY, document.scrollingElement.scrollTop],
        active: document.activeElement === c,
      };
    });
  }
}

export function ensureDir(d) { fs.mkdirSync(d, { recursive: true }); return d; }
