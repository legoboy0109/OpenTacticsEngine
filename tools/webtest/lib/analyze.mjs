// Pixel measurements on PNG screenshots, done inside a browser page (canvas
// getImageData) so no image library is needed.

// Bounding box of everything that is not pure black, plus how much of the
// picture is non-black. Letterbox bars are pure black.
export async function contentBox(page, png) {
  return page.evaluate(async (b64) => {
    const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
    const c = new OffscreenCanvas(bmp.width, bmp.height);
    const g = c.getContext('2d');
    g.drawImage(bmp, 0, 0);
    const d = g.getImageData(0, 0, bmp.width, bmp.height).data;
    let x0 = bmp.width, y0 = bmp.height, x1 = -1, y1 = -1;
    for (let y = 0; y < bmp.height; y++)
      for (let x = 0; x < bmp.width; x++) {
        const i = (y * bmp.width + x) * 4;
        if (d[i] | d[i + 1] | d[i + 2]) {
          if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
        }
      }
    return { width: bmp.width, height: bmp.height, x0, y0, x1, y1 };
  }, png.toString('base64'));
}

// How steep the edges are in a rectangle of the screenshot: the 95th percentile
// of the horizontal luminance step between neighbouring pixels, as a share of
// the rectangle's luminance range. Text rasterized at the display's real
// resolution keeps this high (a stroke edge crosses within about a pixel at any
// DPR); a low-resolution raster stretched up by the browser spreads every edge
// over several pixels and the value drops. rect is in screenshot pixels.
export async function edgeSteepness(page, png, rect) {
  return page.evaluate(async ({ b64, rect }) => {
    const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
    const c = new OffscreenCanvas(bmp.width, bmp.height);
    const g = c.getContext('2d');
    g.drawImage(bmp, 0, 0);
    const { x, y, w, h } = rect;
    const d = g.getImageData(x, y, w, h).data;
    const luma = new Float32Array(w * h);
    for (let i = 0; i < w * h; i++) luma[i] = 0.3 * d[i * 4] + 0.59 * d[i * 4 + 1] + 0.11 * d[i * 4 + 2];
    const sorted = Array.from(luma).sort((a, b) => a - b);
    const range = sorted[Math.floor(sorted.length * 0.99)] - sorted[Math.floor(sorted.length * 0.01)];
    const steps = [];
    for (let r = 0; r < h; r++)
      for (let q = 0; q + 1 < w; q++) {
        const dd = Math.abs(luma[r * w + q + 1] - luma[r * w + q]);
        if (dd > range * 0.05) steps.push(dd);
      }
    steps.sort((a, b) => a - b);
    return { range, edges: steps.length, steepness: steps.length ? steps[Math.floor(steps.length * 0.95)] / range : 0 };
  }, { b64: png.toString('base64'), rect });
}

// Share (0..1) of the screenshot's pixels within `tol` of colour [r, g, b] on every channel.
// Used to tell art from placeholders: demo-srpg's placeholder grass is exactly #8fbf6a
// and covers most of the map, while the starter art's grass uses other greens.
export async function colorShare(page, png, rgb, tol = 0) {
  return page.evaluate(async ({ b64, rgb, tol }) => {
    const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
    const c = new OffscreenCanvas(bmp.width, bmp.height);
    const g = c.getContext('2d');
    g.drawImage(bmp, 0, 0);
    const d = g.getImageData(0, 0, bmp.width, bmp.height).data;
    let n = 0;
    for (let i = 0; i < d.length; i += 4)
      if (Math.abs(d[i] - rgb[0]) <= tol && Math.abs(d[i + 1] - rgb[1]) <= tol && Math.abs(d[i + 2] - rgb[2]) <= tol) n++;
    return n / (d.length / 4);
  }, { b64: png.toString('base64'), rgb, tol });
}

// The art check shared by the suites: a battle screenshot of demo-srpg must show the
// starter art (its grass colour) and almost none of the placeholder grass colour.
export async function checkArtDrawn(env, C, png, label) {
  const placeholder = await colorShare(env.analysisPage, png, [0x8f, 0xbf, 0x6a]);
  const art = await colorShare(env.analysisPage, png, [0x88, 0xb9, 0x5f], 2);
  C.check(`${label}: terrain art is drawn (not the placeholder colours)`, art > 0.08 && placeholder < 0.005,
          `art grass ${(art * 100).toFixed(1)}%, placeholder grass ${(placeholder * 100).toFixed(2)}%`);
}
