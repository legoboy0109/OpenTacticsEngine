// (e) Smoke test for any browser (run with --browser firefox): Home renders, enter a battle, one move,
// options survive a reload, DPR 2 canvas size, arrow keys do not scroll.
import { contentBox } from '../lib/analyze.mjs';
import { open, keyboardToBattle, keyboardMoveLord, same } from '../lib/flows.mjs';

export const name = 'smoke';

export async function run(env) {
  const { checks: C } = env;
  const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 }, deviceScaleFactor: 2 });
  const s = await open(page, env.out, `e-${env.browserName}`, env.url + '/index.html');
  const home = await s.shot('home');
  const box = await contentBox(env.analysisPage, home);
  C.check('Home renders', box.x1 - box.x0 > 1500 && box.y1 - box.y0 > 900, JSON.stringify(box));
  const info = await s.canvasInfo();
  C.check('DPR 2: backing store is 1920x1080', info.backing[0] === 1920 && info.backing[1] === 1080, JSON.stringify(info));
  await keyboardToBattle(s);
  const b0 = await s.shot('battle');
  C.check('battle started', s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
  await keyboardMoveLord(s);
  const b1 = await s.shot('moved');
  C.check('one move: action menu appears', !same(b0, b1));
  await s.keys(['Enter']);
  const b2 = await s.shot('waited');
  C.check('unit finished its action', !same(b1, b2));
  C.check('page did not scroll', (await s.canvasInfo()).scroll.every((v) => v === 0));

  // Options persist over a reload (IndexedDB in this browser).
  await s.page.reload();
  await s.waitReady();
  await s.keys(['ArrowDown', 'ArrowDown', 'Enter', 'ArrowRight', 'ArrowRight', 'ArrowRight', 'ArrowRight', 'Escape']);
  await s.page.waitForTimeout(800);
  await s.page.reload();
  await s.waitReady();
  await s.page.waitForTimeout(300);
  const font = s.logs.filter((l) => /\] font:/.test(l)).pop() || '';
  C.check('options persisted over a reload', /AtkinsonHyperlegible.*smooth/.test(font), font);
  C.noErrors(s, `${env.browserName}: no console errors`);
  await close();
}
