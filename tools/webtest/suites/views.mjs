// (g) Kinds of view (docs/VIEWS.md): the demo's Isometric style draws, differs from the
// top-down one, and the pointer picks squares through the projection. The 3D
// isometric style draws too, picks squares and turns.
import { open, same } from '../lib/flows.mjs';
import { diffShare } from '../lib/analyze.mjs';

export const name = 'views';

const args = (style) => `/index.html?args=--game+demo-srpg+--map+bridge+--style+${style}`;

export async function run(env) {
  const { checks: C } = env;
  const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });

  const flat = await open(page, env.out, 'g-topdown', env.url + args('classic'));
  await flat.page.waitForTimeout(1500);
  const top = await flat.shot('battle');

  const s = await open(page, env.out, 'g-iso', env.url + args('isometric'));
  await s.page.waitForTimeout(1500);
  const iso = await s.shot('battle');
  C.check('the isometric style loaded with its game', s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
  const differs = await diffShare(env.analysisPage, top, iso, { x: 0, y: 0, w: 700, h: 510 });
  C.check('the isometric picture is a different picture from the top-down one', differs > 0.5, `${(differs * 100).toFixed(1)}%`);

  // The lord stands on square (2,4); in this view that is around logical pixel (166, 150).
  await s.clickL(166, 150, 400);
  const selected = await s.shot('lord-selected');
  const tinted = await diffShare(env.analysisPage, iso, selected, { x: 0, y: 0, w: 700, h: 510 });
  C.check('clicking the lord in the isometric view selects him (his move range is tinted)', tinted > 0.08, `${(tinted * 100).toFixed(1)}%`);
  // One square along x is half a diamond right and down: (+24, +12).
  await s.clickL(190, 162, 600);
  const menu = await s.shot('moved-menu');
  C.check('clicking a square in range moves him there and opens the action menu', !same(selected, menu));
  await s.keys(['Enter'], 600);  // Wait
  const waited = await s.shot('waited');
  const moved = await diffShare(env.analysisPage, iso, waited, { x: 300, y: 250, w: 130, h: 110 });
  C.check('after Wait the lord stands on the clicked square', moved > 0.05, `${(moved * 100).toFixed(1)}%`);
  C.noErrors(s, 'isometric view: no console errors');

  // 3D isometric: the same game drawn from real blocks, with a camera that turns.
  const t = await open(page, env.out, 'g-iso3d', env.url + args('isometric-3d'));
  await t.page.waitForTimeout(3500);  // the turn banner has gone
  const iso3d = await t.shot('battle');
  C.check('the 3D isometric style loaded with its game', t.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
  const differs3d = await diffShare(env.analysisPage, top, iso3d, { x: 0, y: 0, w: 700, h: 510 });
  C.check('the 3D isometric picture is a different picture from the top-down one', differs3d > 0.5, `${(differs3d * 100).toFixed(1)}%`);
  // The lord's square is around logical pixel (127, 198) when the battle starts.
  await t.clickL(127, 198, 500);
  const picked = await t.shot('lord-selected');
  const tinted3d = await diffShare(env.analysisPage, iso3d, picked, { x: 0, y: 0, w: 700, h: 510 });
  C.check('clicking the lord in the 3D isometric view selects him (his move range is tinted)', tinted3d > 0.08, `${(tinted3d * 100).toFixed(1)}%`);
  await t.keys(['Escape'], 300);
  // The on-screen turn buttons sit in the map's bottom-left corner; the right one is around (30, 248).
  await t.clickL(30, 248, 900);
  const turned = await t.shot('turned-by-button');
  const d1 = await diffShare(env.analysisPage, iso3d, turned, { x: 0, y: 0, w: 700, h: 470 });
  C.check('the on-screen turn button turns the view', d1 > 0.15, `${(d1 * 100).toFixed(1)}%`);
  await t.keys(['Period'], 900);
  const turned2 = await t.shot('turned-by-key');
  const d2 = await diffShare(env.analysisPage, turned, turned2, { x: 0, y: 0, w: 700, h: 470 });
  C.check('the turn key turns the view again', d2 > 0.15, `${(d2 * 100).toFixed(1)}%`);
  // Turning back the other way works too (the scroll position may differ a little, so only a change is checked).
  await t.keys(['Comma'], 900);
  const back = await t.shot('turned-back');
  const d3 = await diffShare(env.analysisPage, turned2, back, { x: 0, y: 0, w: 700, h: 470 });
  C.check('the other turn key turns the view back', d3 > 0.15, `${(d3 * 100).toFixed(1)}%`);
  C.noErrors(t, '3D isometric view: no console errors');

  // Full 3D: the same blocks under a free camera with perspective.
  const f = await open(page, env.out, 'g-full3d', env.url + args('full-3d'));
  await f.page.waitForTimeout(3500);
  const full = await f.shot('battle');
  C.check('the Full 3D style loaded with its game', f.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
  const differsFull = await diffShare(env.analysisPage, iso3d, full, { x: 0, y: 0, w: 700, h: 470 });
  C.check('the full 3D picture is a different picture from the 3D isometric one', differsFull > 0.3, `${(differsFull * 100).toFixed(1)}%`);
  // The lord's square is around logical pixel (188, 196) when the battle starts.
  await f.clickL(188, 196, 500);
  const pickedFull = await f.shot('lord-selected');
  const tintedFull = await diffShare(env.analysisPage, full, pickedFull, { x: 0, y: 0, w: 700, h: 470 });
  C.check('clicking the lord in the full 3D view selects him (his move range is tinted)', tintedFull > 0.08, `${(tintedFull * 100).toFixed(1)}%`);
  await f.keys(['Escape'], 300);
  let before = await f.shot('before-camera');
  const moved3d = async (name, what, least) => {
    const after = await f.shot(name);
    const d = await diffShare(env.analysisPage, before, after, { x: 0, y: 0, w: 700, h: 470 });
    C.check(what, d > least, `${(d * 100).toFixed(1)}%`);
    before = after;
  };
  await f.keys(['Period'], 600);
  await moved3d('turned', 'the turn key turns the free camera', 0.1);
  await f.keys(['KeyF'], 600);
  await moved3d('tilted', 'the tilt key tilts it', 0.1);
  await f.keys(['Minus'], 600);
  await moved3d('zoomed', 'the zoom key zooms out', 0.1);
  // The on-screen buttons: "+" (zoom in) is the fifth, around logical pixel (95, 248).
  await f.clickL(95, 248, 600);
  await moved3d('zoomed-by-button', 'the on-screen zoom button zooms in', 0.1);
  // Mouse: the wheel zooms, dragging with the right button turns and tilts.
  const box = await f.page.locator('canvas').first().boundingBox();
  const mx = box.x + box.width * 0.35, my = box.y + box.height * 0.4;
  await f.page.mouse.move(mx, my);
  await f.page.mouse.wheel(0, 300);
  await f.page.waitForTimeout(600);
  await moved3d('wheel', 'the mouse wheel zooms', 0.1);
  await f.page.mouse.down({ button: 'right' });
  await f.page.mouse.move(mx + 120, my + 40, { steps: 6 });
  await f.page.mouse.up({ button: 'right' });
  await f.page.waitForTimeout(600);
  await moved3d('dragged', 'dragging with the right mouse button turns the camera', 0.15);
  C.noErrors(f, 'full 3D view: no console errors');
  await close();
}
