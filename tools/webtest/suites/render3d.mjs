// (h) The 3D drawing layer (docs/RENDER3D.md): the built-in 3D test scene draws in
// the browser, turns, and switches between its flat and perspective cameras.
import { open } from '../lib/flows.mjs';
import { diffShare, colorShare } from '../lib/analyze.mjs';

export const name = 'render3d';

export async function run(env) {
  const { checks: C } = env;
  const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
  const s = await open(page, env.out, 'h-test3d', env.url + '/index.html?args=--test-3d');
  await s.page.waitForTimeout(1200);
  const flat = await s.shot('flat');
  // The sky is one colour; the map must cover a good part of the picture.
  const sky = await colorShare(env.analysisPage, flat, [115, 158, 217], 3);
  C.check('the 3D test scene draws a map over the sky', sky > 0.2 && sky < 0.8, `${(sky * 100).toFixed(1)}% sky`);

  await s.keys(['ArrowRight'], 900);
  const turned = await s.shot('turned');
  const d1 = await diffShare(env.analysisPage, flat, turned);
  C.check('turning the camera changes the picture', d1 > 0.1, `${(d1 * 100).toFixed(1)}%`);

  await s.keys(['Enter'], 600);
  const persp = await s.shot('perspective');
  const d2 = await diffShare(env.analysisPage, turned, persp);
  C.check('the perspective camera gives a different picture', d2 > 0.1, `${(d2 * 100).toFixed(1)}%`);
  C.noErrors(s, '3D test scene: no console errors');

  // The page draws 3D with the graphics chip (WebGL 2) when the browser has it, and the
  // picture is the software backend's (the reference) but for a few edge pixels.
  const hardware = s.logs.some((l) => /3D drawing: OpenGL/.test(l));
  C.check('3D is drawn with WebGL 2', hardware, s.logs.filter((l) => /3D drawing/.test(l)).join(' | '));
  const c2 = await env.newContext({ viewport: { width: 960, height: 540 } });
  const soft = await open(c2.page, env.out, 'h-test3d-software', env.url + '/index.html?args=--test-3d+--3d+software');
  await soft.page.waitForTimeout(1200);
  const softFlat = await soft.shot('flat');
  C.check('--3d software uses the software backend', soft.logs.some((l) => /3D drawing: software/.test(l)));
  // Like for like: WebGL draws at the display's size by default and the software backend at
  // the game's, so the comparison asks WebGL for the game's size too.
  const c3 = await env.newContext({ viewport: { width: 960, height: 540 } });
  const small = await open(c3.page, env.out, 'h-test3d-game-size', env.url + '/index.html?args=--test-3d+--3d-res+game');
  await small.page.waitForTimeout(1200);
  const smallFlat = await small.shot('flat');
  const d3 = await diffShare(env.analysisPage, smallFlat, softFlat);
  C.check('WebGL 2 and the software backend draw the same picture', d3 < 0.005, `${(d3 * 100).toFixed(2)}% of pixels differ`);
  const d4 = await diffShare(env.analysisPage, flat, smallFlat);
  C.check('WebGL 2 at the display\'s size is a finer picture than at the game\'s size', d4 > 0.01 && d4 < 0.2, `${(d4 * 100).toFixed(2)}% of pixels differ`);
  C.noErrors(small, '3D test scene (WebGL 2, game size): no console errors');
  C.noErrors(soft, '3D test scene (software): no console errors');
  await c3.close();
  await c2.close();
  await close();
}
