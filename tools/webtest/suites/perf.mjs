// (f) Size and speed: file sizes, load time, --input and a 300-frame autoplay battle via ?args=.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { open } from '../lib/flows.mjs';

export const name = 'perf';

export async function run(env) {
  const { checks: C } = env;
  for (const f of ['ote-player.wasm', 'ote-player.js']) {
    const buf = fs.readFileSync(path.join(env.build, f));
    C.note(`${f}: ${(buf.length / 1024).toFixed(0)} KiB (${(zlib.gzipSync(buf).length / 1024).toFixed(0)} KiB gzip)`);
  }

  // Cold load: fresh context = empty HTTP cache and storage.
  const times = [];
  for (let i = 0; i < 3; i++) {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const t0 = Date.now();
    const s = await open(page, env.out, 'f-load', env.url + '/index.html');
    times.push({ wall: Date.now() - t0, ready: await page.evaluate(() => window.__oteReadyMs) });
    if (i === 0) C.noErrors(s, 'cold load: no console errors');
    await close();
  }
  C.note('navigation -> first rendered frame (ms, 3 cold loads, local server): ' +
         times.map((t) => `${Math.round(t.ready)} (wall ${t.wall})`).join(', '));
  C.check('first frame within 5 s', times.every((t) => t.ready < 5000));

  // Autoplay battle for 300 frames.
  {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const q = encodeURIComponent('--game demo-srpg --map bridge --autoplay --frames 300');
    const s = await open(page, env.out, 'f-autoplay', `${env.url}/index.html?args=${q}`);
    await s.waitLog(/main loop ended after 300 frames/, 60000);
    C.check('300-frame autoplay battle ran to the end', true);
    await s.shot('autoplay-end');
    C.check('autoplay loaded the game', s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
    C.noErrors(s, 'autoplay: no console errors');
    await close();
  }

  // --input works on the web.
  {
    const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
    const q = encodeURIComponent('--input confirm,confirm,confirm,wait:10 --seed 3');
    const s = await open(page, env.out, 'f-input', `${env.url}/index.html?args=${q}`);
    await s.waitLog(/main loop ended/, 30000);
    C.check('--input script drove the launcher into the battle and finished', s.logs.some((l) => /loaded game 'demo-srpg'/.test(l)));
    await s.shot('input-end');
    C.noErrors(s, '--input: no console errors');
    await close();
  }
}
