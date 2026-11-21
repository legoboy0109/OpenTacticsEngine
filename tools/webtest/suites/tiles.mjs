// (k) Seams between tile-layer squares when the canvas is shown at a fractional scale.
//
// The fixture (tests/fixtures/tiles-sparse) paints every square of a 10 x 7 map with tile 0 of a SPARSE sheet: one opaque, bright tile
// whose neighbours in the sheet are fully transparent. At the game's 24-pixel squares the picture is 1.5x the sheet's 16-pixel cells, and a
// browser window is hardly ever an exact multiple of the 480 x 270 view, so quad edges fall between device pixels. If a pixel on a quad
// edge reads outside the cell, it reads the transparent neighbour and the black clear colour shows through: a dark grid. The check
// scans the painted map for dark pixels (there must be none: the tile is one flat bright colour).
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { buildMock, addProject } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';

export const name = 'tiles';

const TILE_RGB = [230, 200, 40];

// The sheet: 8 x 4 cells of 16 x 16; cell 0 is (col 1, row 1 = tile 9) one flat opaque colour, every other cell fully transparent (rgb 0, alpha 0).
export function sparseSheetPng() {
  const w = 128, h = 64;
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++)
    for (let x = 16; x < 32 && y >= 16 && y < 32; x++) {
      const i = y * (w * 4 + 1) + 1 + x * 4;
      raw[i] = TILE_RGB[0]; raw[i + 1] = TILE_RGB[1]; raw[i + 2] = TILE_RGB[2]; raw[i + 3] = 255;
    }
  const crcTable = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
  const crc = (b) => { let c = 0xffffffff; for (const x of b) c = crcTable[(c ^ x) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (tag, data) => {
    const out = Buffer.alloc(12 + data.length);
    out.writeUInt32BE(data.length, 0); out.write(tag, 4, 'latin1'); data.copy(out, 8);
    out.writeUInt32BE(crc(out.subarray(4, 8 + data.length)), 8 + data.length);
    return out;
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6;
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// Seams in a screenshot of the painted map: counts the dark runs of 1 to 4 pixels (horizontal or vertical) that have the tile colour on
// both sides of them. A seam is exactly that; unit squares, text and the cursor are edged by other colours on at least one side.
// Also returns the size of the map (the bounding box of the tile colour), which the player lays out itself.
export async function thinLines(page, png) {
  return page.evaluate(async ({ b64, rgb }) => {
    const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
    const c = new OffscreenCanvas(bmp.width, bmp.height);
    const g = c.getContext('2d');
    g.drawImage(bmp, 0, 0);
    const W = bmp.width, H = bmp.height;
    const d = g.getImageData(0, 0, W, H).data;
    const at = (x, y) => (y * W + x) * 4;
    const isTile = (i) => Math.abs(d[i] - rgb[0]) < 12 && Math.abs(d[i + 1] - rgb[1]) < 12 && Math.abs(d[i + 2] - rgb[2]) < 12;
    const isDark = (i) => d[i] + d[i + 1] + d[i + 2] < 300;  // the tile's own colour sums to 470
    let x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (let y = 0; y < H; y++)
      for (let x = 0; x < W; x++)
        if (isTile(at(x, y))) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (x1 < 0) return { w: 0, h: 0, vertical: 0, horizontal: 0 };
    // Scans one line of pixels; `step(i)` gives the pixel offset along it.
    let vertical = 0, horizontal = 0;
    for (let y = y0; y <= y1; y++)
      for (let x = x0 + 1; x < x1; x++) {
        if (!isTile(at(x - 1, y)) || !isDark(at(x, y))) continue;
        let n = 1;
        while (n <= 4 && x + n <= x1 && isDark(at(x + n, y))) n++;
        if (n <= 4 && x + n <= x1 && isTile(at(x + n, y))) { vertical++; x += n - 1; }
      }
    for (let x = x0; x <= x1; x++)
      for (let y = y0 + 1; y < y1; y++) {
        if (!isTile(at(x, y - 1)) || !isDark(at(x, y))) continue;
        let n = 1;
        while (n <= 4 && y + n <= y1 && isDark(at(x, y + n))) n++;
        if (n <= 4 && y + n <= y1 && isTile(at(x, y + n))) { horizontal++; y += n - 1; }
      }
    return { w: x1 - x0 + 1, h: y1 - y0 + 1, vertical, horizontal };
  }, { b64: png.toString('base64'), rgb: TILE_RGB });
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
    // [viewport w, h, devicePixelRatio] and the picture's scale (backing pixels per logical pixel). 720 x 405 is exactly 1.5: every second
    // square edge falls on a pixel centre, which is where the seams first showed (before the renderer put rectangles on whole pixels).
    // 1920 x 1081 is a whole scale with a letterbox offset of half a pixel; 960 x 540 a plain whole scale; the rest fractional (also
    // below 1, and with a devicePixelRatio, as a browser at 125% or 150% has).
    const cases = [[720, 405, 1], [400, 225, 1], [1317, 741, 1], [1801, 900, 1], [1920, 1081, 1], [1366, 700, 1.5], [1000, 600, 1.25],
                   [2561, 1305, 1], [960, 540, 1]];
    // Each case runs with whole-number scaling on (the default: the picture is a whole multiple, so no seam is possible by construction;
    // the sparse sheet is drawn 1.5x its cells, so this also shows the whole multiple is what is on screen) and off (the fractional
    // scale, where pixel_snap.h keeps the seams away).
    for (const mode of ['on', 'off'])
    for (const [w, h, dpr] of cases) {
      const { page, close } = await env.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: dpr });
      const s = new Session(page, env.out, 'k');
      await s.goto(`${srv.url}/player/embed.html?project=tiles-sparse&map=sparse&noaudio=1&integerscale=${mode}`);
      await s.waitReady();
      await s.keys(['Enter'], 600);  // skip the turn banner
      await page.waitForTimeout(400);
      const png = await s.shot(`${mode}-${w}x${h}@${dpr}`);
      const info = await s.canvasInfo();
      const k0 = Math.min(info.backing[0] / 480, info.backing[1] / 270);
      const k = mode === 'on' && k0 >= 1 ? Math.floor(k0) : k0;
      const r = await thinLines(env.analysisPage, png);
      C.check(`tiles [${mode}] ${w}x${h} @${dpr} (scale ${k.toFixed(3)}): no dark seams between tile squares`,
        r.w > 100 && r.vertical === 0 && r.horizontal === 0, `map ${r.w}x${r.h}px, ${r.vertical} vertical and ${r.horizontal} horizontal dark line pieces`);
      C.noErrors(s, `tiles [${mode}] ${w}x${h} @${dpr}: no console errors`);
      await close();
    }
  } finally {
    await srv.close();
    fs.rmSync(mock, { recursive: true, force: true });
  }
}
