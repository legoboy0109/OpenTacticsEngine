// (a) Standalone page: Home, keyboard battle flow, options, sharp text at DPR 1 and 2, persistence.
import { contentBox, edgeSteepness, checkArtDrawn } from '../lib/analyze.mjs';
import { open, keyboardToBattle, keyboardMoveLord, keyboardEndTurn, same } from '../lib/flows.mjs';

export const name = 'standalone';

// Home -> Options, pick the "sans" font (4 steps right of "Game default": built-in, pixel-ttf, arcade, sans).
// Home lists every game, then Options. The count comes from the build, so
// games the owner has made with the editor do not throw the navigation off.
async function gameCount(s) {
  return s.page.evaluate(async () => {
    const m = await (await fetch('games.manifest.json')).json();
    return m.files.filter((f) => /^games\/[^/]+\/game\.json$/.test(f.path)).length;
  });
}

async function chooseSansFont(s) {
  const downs = Array(await gameCount(s)).fill('ArrowDown');
  await s.keys([...downs, 'Enter']);  // Home: the row after the games = Options
  await s.keys(['ArrowRight', 'ArrowRight', 'ArrowRight', 'ArrowRight']);
}

export async function run(env) {
  const { checks: C } = env;

  // ---- keyboard flow at DPR 1 ----
  {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const s = await open(page, env.out, 'a1', env.url + '/index.html');
    const home = await s.shot('home');
    const box = await contentBox(env.analysisPage, home);
    C.check('Home renders content over the whole canvas', box.x1 - box.x0 > 800 && box.y1 - box.y0 > 450, JSON.stringify(box));
    const info = await s.canvasInfo();
    C.check('canvas backing store = CSS size x devicePixelRatio', info.backing[0] === 960 && info.backing[1] === 540, JSON.stringify(info));
    C.check('canvas has keyboard focus after load', info.active);

    await keyboardToBattle(s);
    const b0 = await s.shot('battle');
    C.check('battle started', s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
    await checkArtDrawn(env, C, b0, 'standalone battle');
    C.check('no art sheet errors in the log', !s.logs.some((l) => /art sheet/.test(l)));
    await s.keys(['Enter']);
    const b1 = await s.shot('unit-selected');
    C.check('selecting a unit changes the picture (move range)', !same(b0, b1));
    await s.keys(['ArrowRight', 'ArrowRight', 'Enter']);
    const b2 = await s.shot('action-menu');
    C.check('moving opens the action menu', !same(b1, b2));
    await s.keys(['ArrowUp', 'Enter']);  // Wait, the last entry
    const b3 = await s.shot('unit-waited');
    C.check('unit waits (menu closes)', !same(b2, b3));
    await keyboardEndTurn(s);
    const b4 = await s.shot('after-ai');
    C.check('end turn: the AI moved (picture changed)', !same(b3, b4));
    C.noErrors(s, 'standalone keyboard flow: no console errors');
    await close();
  }

  // ---- options: smooth font sharp at DPR 1 vs 2; persistence ----
  const ratios = {};
  for (const dpr of [1, 2]) {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 }, deviceScaleFactor: dpr });
    const s = await open(page, env.out, `a-dpr${dpr}`, env.url + '/index.html');
    await chooseSansFont(s);
    const png = await s.shot('options-sans');
    const info = await s.canvasInfo();
    C.check(`DPR ${dpr}: backing store is ${960 * dpr}x${540 * dpr}`, info.backing[0] === 960 * dpr && info.backing[1] === 540 * dpr, JSON.stringify(info));
    C.check(`DPR ${dpr}: canvas CSS size is the viewport`, info.css[0] === 960 && info.css[1] === 540);
    // First sample-text line of the options screen (screenshot pixels).
    const r = await edgeSteepness(env.analysisPage, png, { x: 48 * dpr, y: 430 * dpr, w: 440 * dpr, h: 22 * dpr });
    ratios[dpr] = r.steepness;
    C.note(`DPR ${dpr}: text edge steepness ${(r.steepness * 100).toFixed(0)}% of contrast (${r.edges} edge steps)`);
    C.check(`DPR ${dpr}: smooth font in use`, s.logs.some((l) => /AtkinsonHyperlegible.*smooth/.test(l)));
    C.noErrors(s, `DPR ${dpr}: no console errors`);
    await close();
  }
  C.check('text at DPR 2 is as crisp as at DPR 1 (rasterized at the real resolution, not stretched)',
          ratios[2] >= ratios[1] * 0.85, `${(ratios[2] * 100).toFixed(0)}% vs ${(ratios[1] * 100).toFixed(0)}%`);

  // Persistence: change font + UI scale, leave Options, reload.
  {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const s = await open(page, env.out, 'a-persist', env.url + '/index.html');
    await chooseSansFont(s);
    await s.keys(['ArrowDown', 'ArrowDown', 'ArrowRight', 'ArrowRight']);  // UI scale two steps up
    await s.shot('options-scaled');
    const scaleLine = s.logs.filter((l) => /ui scale/.test(l)).pop() || '';
    await s.keys(['Escape']);  // back to Home: settings are written and pushed to IndexedDB
    await s.page.waitForTimeout(800);
    const homeAfter = await s.shot('home-after-options');
    await s.page.reload();
    await s.waitReady();
    await s.page.waitForTimeout(300);
    const startFont = s.logs.filter((l) => /\] font:/.test(l)).pop() || '';
    const homeReloaded = await s.shot('home-reloaded');
    C.note('last font line before reload: ' + scaleLine);
    C.note('font line after reload: ' + startFont);
    C.check('options persisted over a page reload (font + UI scale)',
            /AtkinsonHyperlegible.*smooth/.test(startFont) && /ui scale (?!100%)/.test(startFont),
            startFont);
    C.check('Home looks the same after the reload', same(homeAfter, homeReloaded));
    C.noErrors(s, 'persistence flow: no console errors');
    await close();
  }
}
