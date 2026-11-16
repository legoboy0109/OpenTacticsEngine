// (f) Saves (docs/SAVES.md): a battle survives a page reload through the Continue save, manual
// slots persist the same way, and the editor's embed page never saves.
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { buildMock } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';
import { open, keyboardToBattle, keyboardMoveLord, same } from '../lib/flows.mjs';
import { diffShare, colorShare } from '../lib/analyze.mjs';

export const name = 'saves';

// Plays one move (lord to (4,4), Wait), then moves the cursor to the first unit that
// has not acted: exactly where a resumed battle puts it, so pictures can be compared.
async function playOneMove(s) {
  await keyboardMoveLord(s);
  await s.keys(['ArrowUp', 'Enter']);  // Wait is the last entry (Items and Storage come before it)
  await s.page.waitForTimeout(800); // the Continue save is written once the animation is done
  await s.keys(['Tab']);
}

export async function run(env) {
  const { checks: C } = env;
  const saved = /saves: \/userdata\/saves\/demo-srpg/;

  // ---- standalone: Continue after a reload ----
  const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
  const s = await open(page, env.out, 'f1', env.url + '/index.html');
  await keyboardToBattle(s);
  C.check('standalone battle saves into the browser storage folder', s.logs.some((l) => saved.test(l)),
          s.logs.filter((l) => /saves:/.test(l)).join(' | '));
  const fresh = await s.shot('fresh-battle');
  await playOneMove(s);
  const before = await s.shot('before-reload');
  C.check('the move changed the picture', !same(fresh, before));
  await s.page.waitForTimeout(500);  // IDBFS sync

  await s.page.reload();
  await s.waitReady();
  await s.page.waitForTimeout(300);
  await s.keys(['Enter']);  // Home -> Story Tactics Demo's page
  await s.page.waitForTimeout(300);
  const gamePage = await s.shot('game-page');
  await s.keys(['Enter']);  // the first row is Continue
  await s.page.waitForTimeout(1500);
  C.check('Continue was offered and used after the reload',
          s.logs.some((l) => /resumed the Continue save of 'demo-srpg' \(campaign, chapter Skirmish at the Ford, battle\)/.test(l)),
          s.logs.slice(-4).join(' | '));
  const after = await s.shot('after-continue');
  // The lord moved from tile (2,4) to (4,4) and waited. Idle animation makes whole-picture
  // comparisons wobble, so compare the two tiles (2x scale: 960 x 540 viewport).
  const tileRect = (c, r) => ({ x: (32 + 24 * c - 12) * 2, y: (33 + 24 * r - 12) * 2, w: 48, h: 48 });
  const movedTo = await diffShare(env.analysisPage, before, after, tileRect(4, 4));
  const freshThere = await diffShare(env.analysisPage, fresh, after, tileRect(4, 4));
  const movedFrom = await diffShare(env.analysisPage, before, after, tileRect(2, 4));
  C.note(`lord tile (4,4): before reload vs resumed ${(movedTo * 100).toFixed(1)}%, fresh battle vs resumed ${(freshThere * 100).toFixed(1)}%; start tile (2,4): ${(movedFrom * 100).toFixed(1)}%`);
  C.check('the resumed battle shows the lord where he was before the reload', movedTo < 0.25 && freshThere > 0.4 && movedFrom < 0.05,
          `${(movedTo * 100).toFixed(1)}% / ${(freshThere * 100).toFixed(1)}% / ${(movedFrom * 100).toFixed(1)}%`);

  // ---- manual slot: Save, reload, Load ----
  // End Turn menu: End Turn, Objective, Unit (the demo has skills since D1), Save
  await s.keys(['m', 'ArrowDown', 'ArrowDown', 'ArrowDown', 'Enter']);
  await s.page.waitForTimeout(300);
  const slotList = await s.shot('save-list');
  const panel = await colorShare(env.analysisPage, slotList, [18, 20, 32]);
  C.check('Save opens the slot list', panel > 0.1, `panel colour covers ${(panel * 100).toFixed(0)}%`);
  await s.keys(['Enter']);  // slot 1 (empty): saved
  await s.page.waitForTimeout(800);
  await s.page.reload();
  await s.waitReady();
  await s.page.waitForTimeout(300);
  await s.keys(['Enter', 'ArrowDown', 'ArrowDown', 'Enter']);  // game page: Continue, Play, Load
  await s.page.waitForTimeout(300);
  await s.shot('load-list');
  await s.keys(['Enter', 'ArrowRight', 'Enter']);  // slot 1 -> "replaces your Continue save" -> Load
  await s.page.waitForTimeout(1500);
  C.check('a manual slot also survives the reload and loads',
          s.logs.some((l) => /resumed slot 1 of 'demo-srpg'/.test(l)), s.logs.slice(-4).join(' | '));
  C.noErrors(s, 'standalone saves flow: no console errors');
  await close();

  // ---- a fresh browser profile has no Continue ----
  {
    const ctx = await env.newContext({ viewport: { width: 960, height: 540 } });
    const f = await open(ctx.page, env.out, 'f2', env.url + '/index.html');
    await f.keys(['Enter']);
    await f.page.waitForTimeout(300);
    const freshPage = await f.shot('game-page');
    C.check('without a save the game page has no Continue row (the page differs)', !same(freshPage, gamePage));
    await ctx.close();
  }

  // ---- the editor's embed page never saves ----
  const mock = buildMock(env.build, path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../games'));
  const srv = await startServer(mock, {});
  try {
    const ctx = await env.newContext({ viewport: { width: 700, height: 420 } });
    const e = new Session(ctx.page, env.out, 'f3');
    await e.goto(`${srv.url}/host.html?embed=${encodeURIComponent('project=demo-srpg&map=bridge')}`);
    await ctx.page.waitForFunction(() => window.msgs.some((m) => m.type === 'ote-player-ready'), null, { timeout: 20000 });
    await ctx.page.waitForTimeout(1500);
    C.check('embed: the player reports saving is not available', e.logs.some((l) => /saves: not available in this run/.test(l)),
            e.logs.filter((l) => /saves:/.test(l)).join(' | '));
    await ctx.page.mouse.click(300, 200);
    await ctx.page.waitForTimeout(300);
    await ctx.page.keyboard.press('m');  // End Turn menu: End Turn and Cancel only
    await ctx.page.waitForTimeout(300);
    for (const k of ['ArrowDown', 'ArrowDown', 'Enter']) { await ctx.page.keyboard.press(k); await ctx.page.waitForTimeout(150); }
    await ctx.page.waitForTimeout(500);
    const png = await e.shot('after-menu');
    // With Save/Load in the menu this would have opened the slot list (a large dark panel).
    C.check('embed: the end-turn menu has no Save / Load', (await colorShare(env.analysisPage, png, [18, 20, 32])) < 0.05);
    C.noErrors(e, 'embed saves check: no console errors');
    await ctx.close();
  } finally {
    await srv.close();
  }
}
