// (b) Mouse-driven flow, and touch on an emulated phone (portrait + landscape).
import { contentBox } from '../lib/analyze.mjs';
import { open, pointerToBattle, pointerMoveLord, pointerWait, same } from '../lib/flows.mjs';

export const name = 'pointer';

async function pointerFlow(env, s, tap, label) {
  const { checks: C } = env;
  await pointerToBattle(s, tap);
  const b0 = await s.shot('battle');
  C.check(`${label}: battle started from the game list`, s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
  await pointerMoveLord(s, tap);
  const b1 = await s.shot('action-menu');
  C.check(`${label}: select unit + pick destination opens the action menu`, !same(b0, b1));
  await pointerWait(s, tap);
  const b2 = await s.shot('unit-waited');
  C.check(`${label}: clicking the menu entry finishes the unit's action`, !same(b1, b2));
  return [b0, b1, b2];
}

async function chromeCdp(env, page, context) {
  return context.newCDPSession(page);
}

export async function run(env) {
  const { checks: C } = env;

  // ---- mouse ----
  {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const s = await open(page, env.out, 'b-mouse', env.url + '/index.html');
    await pointerFlow(env, s, false, 'mouse');
    C.noErrors(s, 'mouse flow: no console errors');
    await close();
  }

  // ---- touch: portrait then landscape ----
  const phone = { viewport: { width: 390, height: 844 }, deviceScaleFactor: 3, hasTouch: true, isMobile: true };
  {
    const { context, page, close } = await env.newContext(phone);
    const s = await open(page, env.out, 'b-touch', env.url + '/index.html');
    await s.shot('portrait-home');
    let info = await s.canvasInfo();
    C.check('touch portrait: canvas fills the screen', info.css[0] === 390 && info.css[1] === 844, JSON.stringify(info));
    C.check('touch portrait: backing store uses the device pixel ratio', info.backing[0] === 1170 && info.backing[1] === 2532, JSON.stringify(info.backing));
    await pointerFlow(env, s, true, 'touch');
    // (Home fills its bars with its own background colour; the battle's bars are black.)
    const box = await contentBox(env.analysisPage, await s.shot('portrait-battle'));
    // Whole-number scaling (default on, docs/GRAPHICS.md): 1170 device pixels hold 2x the 480 x 270 view (960 x 540), centred both ways.
    C.check('touch portrait: picture fully visible, 2x (960 wide), ~16:9, centered',
      Math.abs((box.x1 - box.x0 + 1) - 960) <= 2 && Math.abs((box.x0 + box.x1) / 2 - 585) < 3 && Math.abs((box.y0 + box.y1) / 2 - 1266) < 3 &&
      Math.abs((box.x1 - box.x0 + 1) / (box.y1 - box.y0 + 1) - 16 / 9) < 0.03, JSON.stringify(box));

    // Drag and pinch must not scroll or zoom the page.
    const cdp = await chromeCdp(env, page, context);
    for (let i = 0; i <= 10; i++) {
      await cdp.send('Input.dispatchTouchEvent', { type: i === 0 ? 'touchStart' : 'touchMove', touchPoints: [{ x: 200, y: 700 - i * 40 }] });
    }
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    await cdp.send('Input.synthesizePinchGesture', { x: 195, y: 420, scaleFactor: 2.5, relativeSpeed: 400 });
    await page.waitForTimeout(400);
    const vp = await page.evaluate(() => ({
      scale: visualViewport.scale, scrollY, docH: document.documentElement.scrollHeight, innerH: innerHeight }));
    C.check('touch: dragging/pinching does not scroll or zoom the page', vp.scale === 1 && vp.scrollY === 0 && vp.docH <= vp.innerH, JSON.stringify(vp));

    // Landscape.
    await page.setViewportSize({ width: 844, height: 390 });
    await page.waitForTimeout(500);
    const land = await s.shot('landscape-battle');
    info = await s.canvasInfo();
    C.check('touch landscape: canvas fills the screen', info.css[0] === 844 && info.css[1] === 390, JSON.stringify(info));
    const lbox = await contentBox(env.analysisPage, land);
    // Height-limited: 1170 device pixels of height hold 4x the view's 270 (1080 high, 1920 wide), centred both ways.
    const k = info.dpr;
    C.check('touch landscape: picture fully visible, 4x (1080 high), height-limited and centered',
      Math.abs((lbox.y1 - lbox.y0 + 1) - 1080) <= 2 && Math.abs((lbox.x0 + lbox.x1) / 2 - 422 * k) < 3 && Math.abs((lbox.y0 + lbox.y1) / 2 - 195 * k) < 3 &&
      Math.abs((lbox.x1 - lbox.x0 + 1) / (lbox.y1 - lbox.y0 + 1) - 16 / 9) < 0.03, JSON.stringify(lbox));
    C.noErrors(s, 'touch flow: no console errors');
    await close();
  }
}
