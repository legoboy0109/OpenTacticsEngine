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
  await close();
}
