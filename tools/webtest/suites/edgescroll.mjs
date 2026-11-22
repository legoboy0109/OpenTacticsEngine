// (n) Mouse edge scrolling in a browser, the way the web editor's Play panel shows the player: in an iframe at a whole multiple (3x).
//
// In a browser the pointer is not stopped by the edge of the picture: pushed against it, it slides out of the frame onto the page, and
// the player (rightly) stops scrolling when the pointer leaves (battle/edge_scroll.h). So the map can only be scrolled by RESTING the
// pointer inside the band, and the band must be wide enough on screen to rest in. Each case moves the pointer the way a hand does
// (several steps), rests, moves it back to the middle and reads the player's "camera: x,y" line (logged once the camera is still).
// tests/edge_scroll_player_test.py checks the rules themselves (timing, speed, keys, touch, menus) on the desktop build.
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { buildMock, addProject } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';
import { sparseSheetPng } from './tiles.mjs';

export const name = 'edgescroll';

const cameras = (s) => s.logs.map((l) => /camera: (-?\d+),(-?\d+)/.exec(l)).filter(Boolean).map((m) => [+m[1], +m[2]]);

export async function run(env) {
  const { checks: C } = env;
  const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
  const mock = buildMock(env.build, path.join(repo, 'games'));
  addProject(mock, 'tiles-sparse', path.join(repo, 'tests/fixtures/tiles-sparse'), (files) => {
    files.push({ path: 'art/tiles.png', bytes: sparseSheetPng() });
  });
  const srv = await startServer(mock);
  try {
    const { page, close } = await env.newContext({ viewport: { width: 1500, height: 870 }, deviceScaleFactor: 1 });
    const s = new Session(page, env.out, 'n');
    await s.goto(`${srv.url}/host.html?embed=${encodeURIComponent('project=tiles-sparse&map=scroll&noaudio=1')}`);
    await page.evaluate(() => { const f = document.getElementById('f'); f.removeAttribute('width'); f.removeAttribute('height'); f.style.width = '1440px'; f.style.height = '810px'; });
    await page.waitForFunction(() => window.msgs.some((m) => m.type === 'ote-player-ready'), null, { timeout: 30000 });
    await page.waitForTimeout(400);
    const box = await page.locator('#f').boundingBox();
    const mid = [box.x + box.width / 2, box.y + box.height / 2];
    await page.mouse.click(mid[0], mid[1]);  // focus the iframe's canvas
    await page.keyboard.press('Enter');      // skip the turn banner
    await page.waitForTimeout(800);
    await page.mouse.move(mid[0], mid[1], { steps: 4 });
    await page.waitForTimeout(600);

    // Rests the pointer at (x, y) for `ms`, then goes back to the middle; returns the camera's travel since the last case.
    let before = cameras(s).at(-1) ?? [0, 0];
    const rest = async (x, y, ms) => {
      await page.mouse.move(x, y, { steps: 6 });
      await page.waitForTimeout(ms);
      await page.mouse.move(mid[0], mid[1], { steps: 6 });
      await page.waitForTimeout(700);  // the camera line comes once it has been still for 20 frames
      const now = cameras(s).at(-1) ?? before;
      const d = [now[0] - before[0], now[1] - before[1]];
      before = now;
      return d;
    };
    const right = box.x + box.width, bottom = box.y + box.height;
    // a hand at the edge: 4 CSS pixels inside the frame (on a 3x picture, under 2 picture pixels), resting a second
    let d = await rest(right - 4, mid[1], 1000);
    C.check(`edge scroll 3x: resting 4 px inside the right edge scrolls right (moved ${d})`, d[0] > 24 && d[1] === 0, JSON.stringify(d));
    d = await rest(mid[0], bottom - 4, 1000);
    C.check(`edge scroll 3x: resting 4 px inside the bottom edge scrolls down (moved ${d})`, d[0] === 0 && d[1] > 24, JSON.stringify(d));
    // a little further in, where a hand that came back from overshooting the frame ends up: still the band
    d = await rest(right - 12, mid[1], 1000);
    C.check(`edge scroll 3x: resting 12 px inside the right edge scrolls right (moved ${d})`, d[0] > 24, JSON.stringify(d));
    d = await rest(box.x + 12, mid[1], 1000);
    C.check(`edge scroll 3x: resting 12 px inside the left edge scrolls left (moved ${d})`, d[0] < -24, JSON.stringify(d));
    // a square near the edge is no edge: a tile (72 px at 3x) inside, nothing moves however long the pointer rests
    d = await rest(right - 72, mid[1], 1500);
    C.check(`edge scroll 3x: resting a tile inside the right edge does not scroll (moved ${d})`, d[0] === 0 && d[1] === 0, JSON.stringify(d));
    // shorter than the dwell: nothing
    d = await rest(right - 6, mid[1], 250);
    C.check(`edge scroll 3x: passing through the band (0.25 s) does not scroll (moved ${d})`, d[0] === 0 && d[1] === 0, JSON.stringify(d));
    // overshooting out of the frame and staying out: nothing (the pointer left)
    d = await rest(right + 40, mid[1], 1000);
    C.check(`edge scroll 3x: a pointer that left the frame does not scroll (moved ${d})`, d[0] === 0 && d[1] === 0, JSON.stringify(d));
    C.noErrors(s, 'edge scroll: no console errors');
    await close();
  } finally {
    await srv.close();
  }
}
