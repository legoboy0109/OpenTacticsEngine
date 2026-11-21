// (l) Seams between tile squares WHILE THE CAMERA SCROLLS, at fractional output scales.
//
// tiles.mjs checks one still picture of a map that fits the view. Here the map (tests/fixtures/tiles-sparse, map "scroll": 40 x 24 squares
// of 24 pixels over a sparse sheet of 16-pixel cells, so every square is drawn 1.5x) is bigger than the 480 x 270 view and the cursor walks
// across it, so the camera takes many positions (and eases between them: some frames are mid-scroll). Squares at the picture's edge are cut
// off by the screen; a seam is a dark 1 to 4 pixel line with the tile colour on both sides (the same detector as tiles.mjs).
// Cases are canvases in a page (embed.html alone) and in an iframe of a host page (how the web editor's Play view embeds the player),
// at scales from 1.5 to 5.19 and with a devicePixelRatio of 1, 1.25 and 1.5.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { buildMock, addProject } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';
import { thinLines, sparseSheetPng } from './tiles.mjs';

export const name = 'scroll';

// Walks the cursor right and down (and back), taking a shot after each step; returns the seam counts.
export async function walk(s, page, env, label, steps) {
  let vertical = 0, horizontal = 0, shots = 0, worst = '';
  const seq = [];
  for (let i = 0; i < steps; i++) seq.push(i % 5 === 4 ? 'ArrowDown' : 'ArrowRight');
  for (let i = 0; i < steps; i++) seq.push(i % 5 === 4 ? 'ArrowUp' : 'ArrowLeft');
  for (let i = 0; i < seq.length; i++) {
    await page.keyboard.press(seq[i]);
    await page.waitForTimeout(i % 3 === 0 ? 30 : 160);  // some shots catch the camera mid-ease, some after it settled
    const png = await s.shot(label);
    const r = await thinLines(env.analysisPage, png);
    shots++;
    if (r.vertical || r.horizontal) {
      vertical += r.vertical; horizontal += r.horizontal;
      if (!worst) { worst = `${label}-step${i}`; fs.writeFileSync(path.join(env.out, `scroll-seam-${label}.png`), png); }
    }
  }
  return { vertical, horizontal, shots, worst };
}

export async function run(env) {
  const { checks: C } = env;
  const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
  const mock = buildMock(env.build, path.join(repo, 'games'));
  addProject(mock, 'tiles-sparse', path.join(repo, 'tests/fixtures/tiles-sparse'), (files) => {
    files.push({ path: 'art/tiles.png', bytes: sparseSheetPng() });
  });
  const srv = await startServer(mock);
  try {
    // [viewport w, h, devicePixelRatio, iframe w x h in CSS pixels (null = embed.html is the page)]
    const cases = [[2560, 1401, 1, [2559.6, 1400.6]], [1700, 900, 1.1, [1500.3, 820.7]], [1700, 900, 1.1, [1500.3, 820.7]], [2560, 1401, 1, null], [2560, 1401, 1, [2559, 1400]], [1000, 600, 1.25, null], [1366, 700, 1.5, null],
                   [1317, 741, 1, null], [1801, 900, 1, [1799, 899]], [720, 405, 1, null], [900, 512, 1, null], [400, 225, 1, null]];
    for (const [w, h, dpr, frame] of cases) {
      const { page, close } = await env.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: dpr });
      const s = new Session(page, env.out, 'l');
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
      if (frame) await page.mouse.click(40, 40);  // focus the iframe's canvas
      await page.keyboard.press('Enter');  // skip the turn banner
      await page.waitForTimeout(600);
      const info = frame ? await page.frames().find((f) => f.url().includes('embed.html')).evaluate(() => {
        const c = document.getElementById('canvas'); return { backing: [c.width, c.height] };
      }) : await s.canvasInfo();
      const k = Math.min(info.backing[0] / 480, info.backing[1] / 270);
      const label = `${w}x${h}@${dpr}${frame ? 'f' : ''}`;
      const r = await walk(s, page, env, label, 14);
      C.check(`scroll ${label} (scale ${k.toFixed(3)}): no dark seams in ${r.shots} frames of a scrolling map`,
        r.vertical === 0 && r.horizontal === 0, `${r.vertical} vertical, ${r.horizontal} horizontal line pieces${r.worst ? ' first at ' + r.worst : ''}`);
      C.noErrors(s, `scroll ${label}: no console errors`);
      await close();
    }
  } finally {
    await srv.close();
    fs.rmSync(mock, { recursive: true, force: true });
  }
}
