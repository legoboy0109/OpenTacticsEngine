// (g) Kinds of view (docs/VIEWS.md): the demo's Isometric style draws, differs from the
// top-down one, and the pointer picks squares through the projection.
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
  await close();
}
