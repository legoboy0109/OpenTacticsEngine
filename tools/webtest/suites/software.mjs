// (m) Seams between tile squares in a browser that gives the player NO WebGL, so SDL falls back to its software renderer.
//
// Found with the editor's Play panel (1178 x 716 canvas, output scale 2.454 x 2.452): thin lines in the engine's background colour ran
// the full length of the picture, on irregular tile edges, through units too. SDL's software renderer places a rectangle by cutting
// (x * scale) and (w * scale) down to ints, each on its own; the snapped numbers (pixel_snap.h) come back from the float multiply a hair
// under the whole pixel about as often as a hair over, so a neighbour's edge could land one pixel off. The renderer now nudges them
// (OutputMap::nudge). The GPU renderers never showed it, so this suite launches its own Chromium with WebGL switched off.
// Same detector and the same scrolling walk as tiles.mjs / scroll.mjs.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';
import { startServer } from '../lib/server.mjs';
import { buildMock, addProject } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';
import { sparseSheetPng } from './tiles.mjs';
import { walk } from './scroll.mjs';

export const name = 'software';

export async function run(env) {
  const { checks: C } = env;
  if (env.browserName !== 'chromium') return;  // the switch below is Chromium's
  const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
  const mock = buildMock(env.build, path.join(repo, 'games'));
  addProject(mock, 'tiles-sparse', path.join(repo, 'tests/fixtures/tiles-sparse'), (files) => {
    files.push({ path: 'art/tiles.png', bytes: sparseSheetPng() });
  });
  const srv = await startServer(mock);
  const browser = await chromium.launch({ args: ['--mute-audio', '--disable-gpu', '--disable-3d-apis', '--disable-webgl', '--disable-webgl2'] });
  try {
    // [viewport w, h, devicePixelRatio, iframe w x h in CSS pixels (null = embed.html is the page)]. The first is the editor's Play
    // panel in a 2560 x 1401 window (the iframe is 1178 x 716.05 CSS pixels); the others are fractional scales without a frame.
    const cases = [[2560, 1401, 1, [1178, 716.046875]], [1317, 741, 1, null], [1801, 900, 1, [1799, 899]], [1000, 600, 1.25, null]];
    for (const [w, h, dpr, frame] of cases) {
      const context = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: dpr });
      const page = await context.newPage();
      const s = new Session(page, env.out, 'm');
      const q = 'project=tiles-sparse&map=scroll&noaudio=1';
      if (frame) {
        await s.goto(`${srv.url}/host.html?embed=${encodeURIComponent(q)}`);
        await page.evaluate(([fw, fh]) => { const f = document.getElementById('f'); f.removeAttribute('width'); f.removeAttribute('height'); f.style.width = fw + 'px'; f.style.height = fh + 'px'; }, frame);
        await page.waitForFunction(() => window.msgs.some((m) => m.type === 'ote-player-ready'), null, { timeout: 30000 });
      } else {
        await s.goto(`${srv.url}/player/embed.html?${q}`);
        await s.waitReady();
      }
      await page.waitForTimeout(400);
      if (frame) await page.mouse.click(40, 40);
      await page.keyboard.press('Enter');
      await page.waitForTimeout(600);
      const info = frame ? await page.frames().find((f) => f.url().includes('embed.html')).evaluate(() => {
        const c = document.getElementById('canvas'); return { backing: [c.width, c.height] };
      }) : await s.canvasInfo();
      const k = Math.min(info.backing[0] / 480, info.backing[1] / 270);
      const label = `sw-${w}x${h}@${dpr}${frame ? 'f' : ''}`;
      const software = s.logs.some((l) => /renderer 'software'/.test(l));
      C.check(`software ${label}: the player really drew with SDL's software renderer`, software, s.logs.filter((l) => /renderer/.test(l)).join(' | '));
      const r = await walk(s, page, env, label, 10);
      C.check(`software ${label} (scale ${k.toFixed(3)}): no dark seams in ${r.shots} frames of a scrolling map`,
        r.vertical === 0 && r.horizontal === 0, `${r.vertical} vertical, ${r.horizontal} horizontal line pieces${r.worst ? ' first at ' + r.worst : ''}`);
      C.noErrors(s, `software ${label}: no console errors`);
      await context.close();
    }
  } finally {
    await browser.close();
    await srv.close();
    fs.rmSync(mock, { recursive: true, force: true });
  }
}
