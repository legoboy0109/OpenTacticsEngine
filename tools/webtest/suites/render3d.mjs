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
  await close();
}
