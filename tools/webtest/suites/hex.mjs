// (h) A hexagon game (docs/HEX.md) through the embed page: it is drawn as hexagons and takes a click.
// The games are the engine's own fixtures (tests/fixtures/hex-pointy and hex-flat, no art) served by the mock of the
// editor's API; blue is made a human side so there is someone to click for.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { buildMock, addProject } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';

export const name = 'hex';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../../..');

const K = 640 / 480;  // the host page shows the player in a 640 x 360 iframe: 480 x 270 logical pixels, scaled up
const BACKGROUND = [10, 10, 16];
const COLOURS = { '.': [156, 204, 101], r: [201, 195, 176], f: [67, 128, 60], m: [141, 123, 104], '~': [59, 120, 196] };

// Colours of the page screenshot at logical pixels (middle of the scaled pixel), read in the analysis page.
async function sample(page, png, points) {
  return page.evaluate(async ({ b64, pts }) => {
    const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
    const c = new OffscreenCanvas(bmp.width, bmp.height);
    const g = c.getContext('2d');
    g.drawImage(bmp, 0, 0);
    return pts.map(([x, y]) => Array.from(g.getImageData(x, y, 1, 1).data).slice(0, 3));
  }, { b64: png.toString('base64'), pts: points.map(([lx, ly]) => [Math.round(lx * 640 / 480), Math.round(ly * 640 / 480)]) });
}

const near = (a, b, tol) => Math.max(Math.abs(a[0] - b[0]), Math.abs(a[1] - b[1]), Math.abs(a[2] - b[2])) <= tol;

// The fixtures' geometry (T = 24, L = 28, 12 x 9 cells; the map is centred in the 352 x 259 viewport).
function geometry(layout) {
  const T = 24, L = 28, w = 12, h = 9, q = 7;
  const pointy = layout === 'pointy';
  const box = (x, y) => pointy ? [x * T + (y & 1 ? T / 2 : 0), y * 3 * q] : [x * 3 * q, y * T + (x & 1 ? T / 2 : 0)];
  const size = pointy ? [w * T + T / 2, (h - 1) * 3 * q + L] : [(w - 1) * 3 * q + L, h * T + T / 2];
  const cam = [Math.floor(-(352 - size[0]) / 2 + 0.5), Math.floor(-(259 - size[1]) / 2 + 0.5)];
  const bs = pointy ? [T, L] : [L, T];
  // The cell whose middle is nearest to a screen pixel (the contract's picking), or null off the map.
  const nearest = (sx, sy) => {
    let best = 1e18, hit = null;
    for (let y = -1; y <= h; y++)
      for (let x = -1; x <= w; x++) {
        const b = box(x, y), dx = b[0] + bs[0] / 2 - cam[0] - sx, dy = b[1] + bs[1] / 2 - cam[1] - sy;
        if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; hit = [x, y]; }
      }
    return hit && hit[0] >= 0 && hit[0] < w && hit[1] >= 0 && hit[1] < h ? hit : null;
  };
  return {
    nearest,
    box: (x, y) => { const b = box(x, y); return [b[0] - cam[0], b[1] - cam[1]]; },
    middle: (x, y) => { const b = box(x, y); return [b[0] + bs[0] / 2 - cam[0], b[1] + bs[1] / 2 - cam[1]]; },
    bs,
  };
}

export async function run(env) {
  const { checks: C } = env;
  const mock = buildMock(env.build, path.join(repo, 'games'));
  // (the fixture's text starts with "//" comment lines: the loader allows them, JSON.parse does not)
  const strip = (t) => t.split('\n').filter((l) => !/^\s*\/\//.test(l)).join('\n');
  const tiles = JSON.parse(strip(fs.readFileSync(path.join(repo, 'tests/fixtures/hex-pointy/maps/skirmish.json'), 'utf8'))).tiles;
  for (const layout of ['pointy', 'flat']) {
    addProject(mock, 'hex-' + layout, path.join(repo, 'tests/fixtures/hex-' + layout), (files) => {
      const f = files.find((x) => x.path === 'maps/skirmish.json');
      const text = f.bytes.toString('utf8').replace(/"controller": "ai"/, '"controller": "human"');
      f.bytes = Buffer.from(text);
    });
  }
  const srv = await startServer(mock);
  try {
    for (const layout of ['pointy', 'flat']) {
      const g = geometry(layout);
      const { page, close } = await env.newContext({ viewport: { width: 700, height: 420 } });
      const s = new Session(page, env.out, 'h-' + layout);
      await s.goto(`${srv.url}/host.html?embed=${encodeURIComponent('project=hex-' + layout + '&map=skirmish')}`);
      await page.waitForFunction(() => window.msgs.some((m) => m.type === 'ote-player-ready'), null, { timeout: 30000 });
      C.check(`${layout}: the hexagon game loads and starts in the embed page`,
        s.logs.some((l) => /loaded game 'hex-/.test(l)) && !s.errors.length, s.errors.join('; '));
      await page.waitForTimeout(1800);  // the turn banner
      const b0 = await s.shot('battle');

      // Cell middles show their terrain; a corner of a cell's box shows what is around it, not the cell's own colour.
      const cells = [[0, 0], [2, 0], [6, 0], [7, 1], [4, 6], [9, 2], [5, 8]];
      const mids = cells.map(([x, y]) => { const m = g.middle(x, y); return [m[0] - 4, m[1] - 5]; });
      const got = await sample(env.analysisPage, b0, mids);
      const bad = cells.filter(([x, y], i) => !near(got[i], COLOURS[tiles[y][x]], 12));
      C.check(`${layout}: cell middles show their terrain colours`, bad.length === 0, JSON.stringify(bad) + ' ' + JSON.stringify(got));
      // The corners of a cell's box lie outside its hexagon: they show the terrain of the hexagon they belong to.
      const probes = [];
      for (const [x, y] of [[6, 0], [7, 0], [8, 0]]) {
        const b = g.box(x, y);
        for (const [cx, cy] of [[1, 1], [g.bs[0] - 2, 1], [1, g.bs[1] - 2], [g.bs[0] - 2, g.bs[1] - 2]]) probes.push([b[0] + cx, b[1] + cy]);
      }
      const corner = await sample(env.analysisPage, b0, probes);
      const wrong = probes.filter(([px, py], i) => {
        const o = g.nearest(px + 0.5, py + 0.5);
        return !near(corner[i], o ? COLOURS[tiles[o[1]][o[0]]] : BACKGROUND, 12);
      });
      C.check(`${layout}: the corners of a cell's box show the hexagon next to it, or the background`, wrong.length === 0,
        JSON.stringify(wrong.map((p) => [p, g.nearest(p[0] + 0.5, p[1] + 0.5)])));
      const edge = g.box(0, 0);
      const origin = await sample(env.analysisPage, b0, [[edge[0] + 1, edge[1] + 1]]);
      C.check(`${layout}: the map's first corner shows the background (the box is bigger than the hexagon)`,
        near(origin[0], BACKGROUND, 6), JSON.stringify(origin));

      // A click on the blue infantry at (3, 4) selects it: the hexagon at its lower left turns blue (move range); a
      // click there moves it.
      const unit = g.middle(3, 4), left = g.middle(2, 5), probe = [left[0] - 4, left[1] - 5];
      const before = (await sample(env.analysisPage, b0, [probe]))[0];
      await s.click(unit[0] * K, unit[1] * K, 500);
      const b1 = await s.shot('selected');
      const after = (await sample(env.analysisPage, b1, [probe]))[0];
      C.check(`${layout}: a click on a hexagon selects the unit there (range tint on the next hexagon)`,
        after[2] > before[2] - 5 && after[0] < before[0] - 40, JSON.stringify([before, after]));
      await s.click(left[0] * K, left[1] * K, 900);
      const b2 = await s.shot('moved');
      const [there] = await sample(env.analysisPage, b2, [[left[0] - 7, left[1] - 7]]);  // inside the unit's blue block
      C.check(`${layout}: a click on a reachable hexagon moves the unit there`, near(there, [0x3d, 0x85, 0xc6], 40),
        JSON.stringify(there));
      C.noErrors(s, `${layout}: no console errors`);
      await close();
    }
  } finally {
    await srv.close();
    fs.rmSync(mock, { recursive: true, force: true });
  }
}
