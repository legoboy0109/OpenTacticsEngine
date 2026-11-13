// (c) Resizing the viewport in the middle of a battle: re-letterboxing, aspect ratio, sharpness.
import { contentBox, edgeSteepness } from '../lib/analyze.mjs';
import { open, keyboardToBattle } from '../lib/flows.mjs';

export const name = 'resize';

export async function run(env) {
  const { checks: C } = env;
  const dpr = 2;
  const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 }, deviceScaleFactor: dpr });
  const s = await open(page, env.out, 'c', env.url + '/index.html');
  await keyboardToBattle(s);
  await s.keys(['Enter']);  // unit selected: something more interesting on screen

  const sizes = [[640, 540], [500, 900], [1280, 400], [960, 540]];
  let first = null, last = null;
  for (const [w, h] of sizes) {
    await page.setViewportSize({ width: w, height: h });
    await page.waitForTimeout(500);
    const png = await s.shot(`${w}x${h}`);
    const info = await s.canvasInfo();
    C.check(`${w}x${h}: canvas CSS size follows the viewport, backing = x${dpr}`,
      info.css[0] === w && info.css[1] === h && info.backing[0] === w * dpr && info.backing[1] === h * dpr, JSON.stringify(info));
    const box = await contentBox(env.analysisPage, png);
    const cw = box.x1 - box.x0 + 1, ch = box.y1 - box.y0 + 1;
    const k = Math.min(w / 480, h / 270) * dpr;  // expected picture size in screenshot pixels
    C.check(`${w}x${h}: picture is 16:9, fits, and is centered`,
      Math.abs(cw / ch - 16 / 9) < 0.03 && Math.abs(cw - 480 * k) < 8 &&
      Math.abs((box.x0 + box.x1) / 2 - (w * dpr) / 2) < 8 && Math.abs((box.y0 + box.y1) / 2 - (h * dpr) / 2) < 8,
      JSON.stringify(box));
    // Sharpness of the info panel text (right side of the picture), relative to its own size.
    const st = await edgeSteepness(env.analysisPage, png, {
      x: Math.round(box.x0 + cw * 0.74), y: Math.round(box.y0 + ch * 0.55), w: Math.round(cw * 0.24), h: Math.round(ch * 0.2) });
    C.note(`${w}x${h}: panel text edge steepness ${(st.steepness * 100).toFixed(0)}%`);
    if (!first) first = st.steepness;
    last = st.steepness;
  }
  C.check('text stays sharp after resizing back and forth', last > 0.5 && first > 0.5, `${(first * 100).toFixed(0)}% / ${(last * 100).toFixed(0)}%`);
  await s.keys(['ArrowRight']);  // the game still reacts after all the resizing
  C.noErrors(s, 'resize: no console errors');
  await close();
}
