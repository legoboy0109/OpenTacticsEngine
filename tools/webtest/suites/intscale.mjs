// (m) Whole-number scaling (the player setting, default on; docs/GRAPHICS.md "Whole-number scaling").
//
// The battle of demo-srpg's "bridge" map in the embedded player, in canvases that are not whole multiples of the 480 x 270 view
// (1177 x 662, 1000 x 600 at devicePixelRatio 1.25, 2560 x 1401, ...). With the setting on (the default) the picture must be exactly
// k x the view with k the largest whole number that fits the canvas's device pixels, centred with the odd leftover on the right / bottom,
// and everything around it plain black. A click inside the picture hits the right square (the lord is selected, then moved: the action
// menu opens); a click in the border changes nothing. Below 1x the picture is fitted whole (nothing cut off). With
// `integerscale=off` it is today's letterbox at the fractional scale.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { open, tile, same } from '../lib/flows.mjs';
import { startServer } from '../lib/server.mjs';
import { buildMock } from '../lib/mock.mjs';
import { diffShare } from '../lib/analyze.mjs';

export const name = 'intscale';

// What the engine does (engine/platform/common/whole_scale.h), restated: null below 1x.
export function expectedRect(bw, bh, lw = 480, lh = 270) {
  const k = Math.min(Math.floor(bw / lw), Math.floor(bh / lh));
  if (k < 1) return null;
  return { k, x: Math.floor((bw - lw * k) / 2), y: Math.floor((bh - lh * k) / 2), w: lw * k, h: lh * k };
}

// The old letterbox (setting off, or below 1x): the picture fitted to the canvas, centred, as SDL floors it.
export function letterboxRect(bw, bh, lw = 480, lh = 270) {
  const k = Math.min(bw / lw, bh / lh);
  const w = bw / lw <= bh / lh ? bw : Math.floor(lw * k), h = bw / lw <= bh / lh ? Math.floor(lh * k) : bh;
  return { k, x: Math.floor((bw - w) / 2), y: Math.floor((bh - h) / 2), w, h };
}

// Pixels of a screenshot against a rectangle: how many non-black pixels lie OUTSIDE it, and how many lie in each 1-pixel strip just
// inside its four edges (the picture is not black there: the battle HUD and map touch every edge of the view).
export async function probeRect(page, png, r, cornerBg = false) {
  return page.evaluate(async ({ b64, r, cornerBg }) => {
    const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
    const c = new OffscreenCanvas(bmp.width, bmp.height);
    const g = c.getContext('2d');
    g.drawImage(bmp, 0, 0);
    const W = bmp.width, H = bmp.height;
    const d = g.getImageData(0, 0, W, H).data;
    // "Lit" = not the border colour: black, or (cornerBg: the home screen paints its border in its own colour) whatever pixel 0,0 is.
    const bg = cornerBg ? [d[0], d[1], d[2]] : [0, 0, 0];
    const lit = (x, y) => d[(y * W + x) * 4] !== bg[0] || d[(y * W + x) * 4 + 1] !== bg[1] || d[(y * W + x) * 4 + 2] !== bg[2];
    let outside = 0;
    for (let y = 0; y < H; y++)
      for (let x = 0; x < W; x++)
        if ((x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h) && lit(x, y)) outside++;
    const strip = (x0, y0, x1, y1) => { let n = 0; for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) if (x >= 0 && y >= 0 && x < W && y < H && lit(x, y)) n++; return n; };
    return {
      W, H, outside,
      left: strip(r.x, r.y, r.x, r.y + r.h - 1), right: strip(r.x + r.w - 1, r.y, r.x + r.w - 1, r.y + r.h - 1),
      top: strip(r.x, r.y, r.x + r.w - 1, r.y), bottom: strip(r.x, r.y + r.h - 1, r.x + r.w - 1, r.y + r.h - 1),
    };
  }, { b64: png.toString('base64'), r, cornerBg });
}

async function openBattle(env, base, w, h, dpr, extra = '') {
  const { page, close } = await env.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: dpr });
  const s = await open(page, env.out, 'm', `${base}/player/embed.html?project=demo-srpg&map=bridge&noaudio=1${extra}`);
  s.wholeScale = !extra.includes('off');
  // The battle opens with story lines and then the turn banner, which swallow clicks: the "Skip" tab (also a pointer hit test), then wait out the banner.
  await s.clickL(458, 203, 1500);
  await s.clickL(458, 203, 1500);
  await s.clickL(458, 203, 4000);
  return { s, page, close };
}

export async function run(env) {
  const { checks: C } = env;
  const mock = buildMock(env.build, path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../games'));
  const srv = await startServer(mock);
  try {
    await runAll(env, srv);
  } finally {
    await srv.close();
    fs.rmSync(mock, { recursive: true, force: true });
  }
}

async function runAll(env, srv) {
  const { checks: C } = env;
  const cases = [[1177, 662, 1], [1000, 600, 1.25], [2560, 1401, 1], [960, 540, 1], [1920, 1081, 1]];
  for (const [w, h, dpr] of cases) {
    const label = `${w}x${h}@${dpr}`;
    const { s, page, close } = await openBattle(env, srv.url, w, h, dpr);
    const info = await s.canvasInfo();
    C.check(`intscale ${label}: the canvas backing store is css x devicePixelRatio`,
      info.backing[0] === Math.round(w * dpr) && info.backing[1] === Math.round(h * dpr), JSON.stringify(info));
    const want = expectedRect(info.backing[0], info.backing[1]);
    const shot = await s.shot(`on-${label}`);
    const p = await probeRect(env.analysisPage, shot, want);
    C.check(`intscale ${label}: picture is ${want.k}x (${want.w}x${want.h}) at ${want.x},${want.y}, and the border is plain black`,
      p.outside === 0 && p.left > 0 && p.right > 0 && p.top > 0 && p.bottom > 0, JSON.stringify({ want, p }));

    // A click inside the picture lands on the square under it: the lord at (2,4) is selected, (4,4) moves him and the action menu opens.
    const before = await s.shot(`on-${label}-before`);
    await s.clickL(...tile(2, 4));
    const selected = await s.shot(`on-${label}-selected`);
    await s.clickL(...tile(4, 4));
    const menu = await s.shot(`on-${label}-menu`);
    C.check(`intscale ${label}: clicking squares inside the picture selects the lord and opens the action menu`,
      !same(before, selected) && !same(selected, menu), 'screenshots did not change');
    await s.keys(['Escape', 'Escape', 'Escape'], 250);  // back to the plain map (the unit may have moved: only the border is compared below)

    // A click in the border does nothing (when there is a border: 960x540 is an exact fit). The map idles (the cursor and the units
    // animate), so "nothing" is judged against that idle drift: a click that did something (selected, moved, cancelled) changes far more.
    const borderX = want.x > 4 ? want.x / 2 : null, borderY = want.y > 4 ? want.y / 2 : null;
    if (borderX !== null || borderY !== null) {
      const d = info.backing[0] / info.css[0];
      const rowY = (want.y + (33 + 24 * 4) * want.k) / d;  // the lord's row, so a border click that wrongly counted would be near him
      const px = borderX !== null ? borderX / d : (want.x + want.w / 2) / d, py = borderX !== null ? rowY : borderY / d;
      const a0 = await s.shot(`on-${label}-a0`);
      const a1 = await s.shot(`on-${label}-a1`);
      await s.click(px, py, 300);
      // The idle animation has a phase: compare several shots after the click with the ones before and keep the closest pair.
      const after_ = [];
      for (let i = 0; i < 5; i++) { after_.push(await s.shot(`on-${label}-b${i}`)); await s.page.waitForTimeout(130); }
      // (Not the hint line at the bottom of the view: it switches from the keyboard's wording to the mouse's on the first pointer move.)
      const area = { x: want.x, y: want.y, w: want.w, h: want.h - 16 * want.k };
      const idle = await diffShare(env.analysisPage, a0, a1, area);
      let after = 1;
      for (const b of after_) for (const a of [a0, a1]) after = Math.min(after, await diffShare(env.analysisPage, a, b, area));
      const control = await diffShare(env.analysisPage, before, selected, area);
      C.check(`intscale ${label}: a click in the border changes nothing (${(after * 100).toFixed(2)}% differs; idle drift ${(idle * 100).toFixed(2)}%, a real click ${(control * 100).toFixed(2)}%)`,
        after < 0.004 && after < control / 10, `clicked ${px.toFixed(1)},${py.toFixed(1)}`);
    }
    C.noErrors(s, `intscale ${label}: no console errors`);
    await close();
  }

  // Below 1x: the picture is fitted whole (nothing cut off), as before.
  {
    const { s, close } = await openBattle(env, srv.url, 400, 225, 1);
    const shot = await s.shot('below-1x');
    const fit = letterboxRect(400, 225);
    const p = await probeRect(env.analysisPage, shot, fit);
    C.check('intscale 400x225: below 1x the whole picture is fitted to the canvas (not cut off)', p.left > 0 && p.right > 0 && p.top > 0 && p.bottom > 0 && p.outside === 0, JSON.stringify({ fit, p }));
    C.noErrors(s, 'intscale 400x225: no console errors');
    await close();
  }
  // 480 x 269: one pixel short of 1x in height -> still fitted, 478 x 269 or so, centred.
  {
    const { s, close } = await openBattle(env, srv.url, 480, 269, 1);
    const shot = await s.shot('below-1x-tall');
    const fit = letterboxRect(480, 269);  // 478 x 269, one pixel in from the left
    const p = await probeRect(env.analysisPage, shot, fit);
    C.check('intscale 480x269: one pixel under 1x is fitted whole (478x269), not cut off', p.left > 0 && p.right > 0 && p.top > 0 && p.bottom > 0 && p.outside === 0, JSON.stringify({ fit, p }));
    await close();
  }

  // Off: today's letterbox at the fractional scale (2.452x of 1177 x 662; 5.19x of 2560 x 1401).
  for (const [w, h, dpr] of [[1177, 662, 1], [2560, 1401, 1], [1000, 600, 1.25]]) {
    const label = `${w}x${h}@${dpr}`;
    const { s, close } = await openBattle(env, srv.url, w, h, dpr, '&integerscale=off');
    const info = await s.canvasInfo();
    const bw = info.backing[0], bh = info.backing[1];
    const fit = letterboxRect(bw, bh), k = fit.k;
    const shot = await s.shot(`off-${label}`);
    const whole = expectedRect(bw, bh);
    // Letterbox: the picture's longer side fills the canvas, which is MORE than the whole multiple. Allow a pixel of rounding each way.
    const loose = await probeRect(env.analysisPage, shot, { x: fit.x - 1, y: fit.y - 1, w: fit.w + 2, h: fit.h + 2 });
    const tight = await probeRect(env.analysisPage, shot, fit);
    C.check(`intscale off ${label}: the picture is fitted at ${k.toFixed(3)}x (letterbox, ${fit.w}x${fit.h}), larger than the ${whole.k}x of the setting on`,
      loose.outside === 0 && fit.w * fit.h > whole.w * whole.h && tight.left > 0 && tight.right > 0 && tight.top > 0 && tight.bottom > 0, JSON.stringify({ fit, whole, tight }));
    // And clicks still land on the squares at the fractional scale.
    const before = await s.shot(`off-${label}-before`);
    await s.clickL(...tile(2, 4));
    const selected = await s.shot(`off-${label}-selected`);
    C.check(`intscale off ${label}: a click on a square still selects the lord`, !same(before, selected), 'no change');
    C.noErrors(s, `intscale off ${label}: no console errors`);
    await close();
  }

  // The standalone page uses the setting too (default on): its picture at 1177 x 662 is the 2x rectangle.
  {
    const { page, close } = await env.newContext({ viewport: { width: 1177, height: 662 } });
    const s = await open(page, env.out, 'm', env.url + '/index.html');
    await page.waitForTimeout(500);
    const info = await s.canvasInfo();
    const want = expectedRect(info.backing[0], info.backing[1]);
    const p = await probeRect(env.analysisPage, await s.shot('standalone'), want, true);
    C.check('intscale standalone page 1177x662: home screen picture is the 2x rectangle (960x540), nothing outside it',
      want.k === 2 && p.outside === 0, JSON.stringify({ want, p }));
    await close();
  }
}
