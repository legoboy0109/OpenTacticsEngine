// (d) embed.html against a static mock of the editor API, inside an iframe of a host page.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from '../lib/server.mjs';
import { buildMock } from '../lib/mock.mjs';
import { Session } from '../lib/session.mjs';
import { same } from '../lib/flows.mjs';
import { checkArtDrawn } from '../lib/analyze.mjs';

export const name = 'embed';

async function openHost(env, base, embedQuery, label, viewport = { width: 700, height: 420 }) {
  const { page, close } = await env.newContext({ viewport });
  const s = new Session(page, env.out, label);
  s.requests = [];
  page.on('request', (r) => s.requests.push(r.url()));
  await s.goto(`${base}/host.html?embed=${encodeURIComponent(embedQuery)}`);
  s.msgs = () => page.evaluate(() => window.msgs);
  s.frame = () => page.frames().find((f) => f.url().includes('embed.html'));
  s.waitMsg = (type, timeout = 20000) =>
    page.waitForFunction((t) => window.msgs.some((m) => m.type === t), type, { timeout });
  return { s, page, close };
}

export async function run(env) {
  const { checks: C } = env;
  const mock = buildMock(env.build, path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../games'));
  const srv = await startServer(mock, { '/api/projects/nolist/files': { status: 404, body: 'no such project' } });
  try {
    // ---- normal project, straight into the battle ----
    {
      const { s, page, close } = await openHost(env, srv.url, 'project=demo-srpg&map=bridge', 'd-ok');
      await s.waitMsg('ote-player-ready');
      const msgs = await s.msgs();
      C.check('ready message reaches the parent', msgs.some((m) => m.type === 'ote-player-ready') && !msgs.some((m) => m.type === 'ote-player-error'), JSON.stringify(msgs));
      C.check('battle started directly (--map)', s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
      C.check('no settings file in embed mode (options never stored)', !s.logs.some((l) => /settings: /.test(l)));
      C.check('file names are URL-encoded', s.requests.some((u) => u.endsWith('/file/notes/odd%20name%20%231%20%C3%A4.txt')),
              s.requests.filter((u) => /odd/.test(u)).join(' '));
      C.check('fonts are fetched from /assets/fonts/', s.requests.some((u) => u.endsWith('/assets/fonts/fonts.json')) &&
              s.requests.some((u) => u.endsWith('/assets/fonts/AtkinsonHyperlegible-Regular.ttf')));
      C.check('canvas has focus inside the iframe', await s.frame().evaluate(() => document.activeElement && document.activeElement.id === 'canvas'));
      const battlePng = await s.shot('battle');
      // The PNG sheets went through the project file API as binary and were decoded.
      await checkArtDrawn(env, C, battlePng, 'embed battle');
      C.check('embed: PNG sheets are fetched from the project files',
              s.requests.some((u) => u.endsWith('/file/art/tiles.png')) && s.requests.some((u) => u.endsWith('/file/art/units.png')));
      C.check('embed: no art sheet errors in the log', !s.logs.some((l) => /art sheet/.test(l)));

      // Keyboard goes to the game; the host page must not scroll.
      await page.mouse.click(300, 200);  // click inside the iframe
      await page.waitForTimeout(500);
      const b0 = await s.shot('after-click');
      for (const k of ['ArrowDown', 'ArrowDown', 'ArrowRight', ' ', 'ArrowDown']) {
        await page.keyboard.press(k);
        await page.waitForTimeout(120);
      }
      const hostScroll = await page.evaluate(() => scrollY);
      const b1 = await s.shot('after-keys');
      C.check('arrow keys and space do not scroll the host page', hostScroll === 0, 'scrollY=' + hostScroll);
      C.check('keyboard input reaches the game inside the iframe', !same(b0, b1));

      // Restart message reloads the project.
      await s.frame().evaluate(() => { window.__marker = 1; });
      await page.evaluate(() => { window.msgs.length = 0; document.getElementById('f').contentWindow.postMessage({ type: 'ote-player-restart' }, '*'); });
      await s.waitMsg('ote-player-ready');
      const marker = await s.frame().evaluate(() => window.__marker);
      C.check('ote-player-restart reloads the player and it reports ready again', marker === undefined);
      const afterRestart = await s.shot('after-restart');
      C.check('after restart the battle is back at its starting state', !same(b1, afterRestart));
      C.noErrors(s, 'embed: no console errors');
      await close();
    }

    // ---- font/style parameters ----
    {
      const { s, close } = await openHost(env, srv.url, 'project=demo-srpg&map=bridge&font=sans', 'd-font');
      await s.waitMsg('ote-player-ready');
      C.check('font= parameter is passed to the player', s.logs.some((l) => /AtkinsonHyperlegible.*smooth/.test(l)));
      await s.shot('font-sans');
      await close();
    }

    // ---- broken game.json ----
    {
      const { s, close } = await openHost(env, srv.url, 'project=broken&map=bridge', 'd-broken');
      await s.waitMsg('ote-player-error');
      const msgs = await s.msgs();
      const err = msgs.find((m) => m.type === 'ote-player-error');
      C.check('broken project: error message is posted to the parent and mentions the file', err && /game\.json/.test(err.message), err && err.message);
      C.check('broken project: no ready message', !msgs.some((m) => m.type === 'ote-player-ready'));
      await s.page.waitForTimeout(400);
      await s.shot('error-screen');
      await close();
    }

    // ---- file list answers 404 ----
    {
      const { s, close } = await openHost(env, srv.url, 'project=nolist', 'd-404');
      s.ignoreErrors(/404/);
      await s.waitMsg('ote-player-error');
      const err = (await s.msgs()).find((m) => m.type === 'ote-player-error');
      C.check('404 for the file list: error posted with the status', err && /404/.test(err.message), err && err.message);
      const panel = await s.frame().evaluate(() => {
        const p = document.querySelector('.ote-error');
        return p ? { shown: p.getBoundingClientRect().width > 0, text: p.innerText } : null;
      });
      C.check('404 for the file list: the page shows its error panel', panel && panel.shown && /404/.test(panel.text), JSON.stringify(panel));
      await s.shot('error-panel');
      s.errors = s.errors.filter((e) => !/404|Failed to load resource/.test(e));
      C.noErrors(s, '404 case: no unexpected console errors');
      await close();
    }

    // ---- missing api: project param absent ----
    {
      const { s, close } = await openHost(env, srv.url, '', 'd-noproject');
      await s.waitMsg('ote-player-error');
      const err = (await s.msgs()).find((m) => m.type === 'ote-player-error');
      C.check('no ?project=: error message names the problem', err && /project/.test(err.message), err && err.message);
      await close();
    }
  } finally {
    await srv.close();
    fs.rmSync(mock, { recursive: true, force: true });
  }
}
