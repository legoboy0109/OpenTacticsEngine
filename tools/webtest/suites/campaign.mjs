// (g) The campaign in the web build (docs/EVENTS.md "Campaign", docs/SAVES.md "Campaign saves"): New game ->
// title card -> scene -> battle start, then a page reload and Continue picks the playthrough up again.
// The test games are made by tests/campaign_player_test.py (copies of demo-srpg with a three-chapter story) into a
// temp folder; the web build itself is symlinked, only games/ and the manifest are the test's own.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { open, same } from '../lib/flows.mjs';

export const name = 'campaign';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../../..');

// A folder that looks like the web build but holds the test games. Symlinks keep the wasm out of the copy.
function makeServeDir(build) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ote_web_campaign_'));
  for (const f of fs.readdirSync(build)) {
    if (f === 'games' || f === 'games.manifest.json' || f === 'CMakeFiles' || f === 'cmake_install.cmake') continue;
    fs.symlinkSync(path.join(build, f), path.join(dir, f));
  }
  execFileSync('python3', [path.join(repo, 'tests/campaign_player_test.py'), '--make-games', path.join(dir, 'games'), repo]);
  execFileSync('python3', [path.join(repo, 'engine/platform/web/make_manifest.py'), dir, path.join(dir, 'games.manifest.json')]);
  return dir;
}

export async function run(env) {
  const { checks: C } = env;
  const dir = makeServeDir(env.build);
  const srv = await startServer(dir);
  try {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const s = await open(page, env.out, 'g1', srv.url + '/index.html');
    await s.keys(['Enter']);  // Home: the first game is the campaign test game
    await s.page.waitForTimeout(300);
    const gamePage = await s.shot('game-page');
    await s.keys(['Enter']);  // New game
    await s.page.waitForTimeout(400);
    const card = await s.shot('title-card');
    C.check('New game starts the campaign (title card on screen)',
            s.logs.some((l) => /campaign: new game of 'camp'/.test(l)) && !same(gamePage, card),
            s.logs.slice(-4).join(' | '));
    await s.keys(['Enter']);  // the card -> the first line of the before scene
    await s.page.waitForTimeout(400);
    const line = await s.shot('scene');
    C.check('the scene shows its dialogue box', !same(card, line));
    // Hold Cancel (X) for the box's skip time: the rest of the scene is skipped and the battle starts.
    await s.page.keyboard.down('x');
    await s.page.waitForTimeout(800);
    await s.page.keyboard.up('x');
    await s.page.waitForTimeout(1500);
    C.check('after the scene the chapter battle starts and saves',
            s.logs.some((l) => /saves: \/userdata\/saves\/camp/.test(l)), s.logs.filter((l) => /saves:/.test(l)).join(' | '));
    const battle = await s.shot('battle');
    C.check('the battle is on screen', !same(line, battle));
    await s.page.waitForTimeout(800);  // IDBFS sync of the Continue save

    // Reload: the page must offer Continue, and it must bring back chapter 1's battle.
    await s.page.reload();
    await s.waitReady();
    await s.page.waitForTimeout(300);
    await s.keys(['Enter']);  // Home -> the game page
    await s.page.waitForTimeout(300);
    const withContinue = await s.shot('game-page-continue');
    C.check('after the reload the game page has a Continue row (the page differs)', !same(gamePage, withContinue));
    await s.keys(['Enter']);  // Continue
    await s.page.waitForTimeout(1500);
    C.check('Continue resumes the campaign battle after the reload',
            s.logs.some((l) => /resumed the Continue save of 'camp' \(campaign, chapter The Ford, battle\)/.test(l)),
            s.logs.slice(-5).join(' | '));
    C.noErrors(s, 'campaign flow: no console errors');
    await close();
  } finally {
    await srv.close();
    fs.rmSync(dir, { recursive: true, force: true });
  }
}
