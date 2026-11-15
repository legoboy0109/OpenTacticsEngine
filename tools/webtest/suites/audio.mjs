// (i) Sound (docs/SOUND.md): the page loads with no audio-related console error or warning, the sound
// device waits for the first key press (browsers refuse to start sound before one), and after it an
// AudioContext is running. An init script wraps AudioContext so the test can see the instances SDL makes.
import { open } from '../lib/flows.mjs';

export const name = 'audio';

const WATCH_AUDIO = () => {
  window.__audioContexts = [];
  for (const k of ['AudioContext', 'webkitAudioContext']) {
    const Orig = window[k];
    if (!Orig) continue;
    window[k] = class extends Orig {
      constructor(...a) { super(...a); window.__audioContexts.push(this); }
    };
  }
};

const states = (page) => page.evaluate(() => (window.__audioContexts || []).map((c) => c.state));

export async function run(env) {
  const { checks: C } = env;
  const { page, close } = await env.newContext({ viewport: { width: 960, height: 540 } });
  await page.addInitScript(WATCH_AUDIO);
  const s = await open(page, env.out, 'i-audio', env.url + '/index.html');
  await page.waitForTimeout(500);
  const before = await states(page);
  C.check('no AudioContext before the first key press', before.length === 0, JSON.stringify(before));
  C.check('no audio line in the log before the first key press', !s.logs.some((l) => /audio/i.test(l)), s.logs.filter((l) => /audio/i.test(l)).join(' | '));

  await s.keys(['ArrowDown']);  // the first input opens the sound device
  await page.waitForTimeout(800);
  const after = await states(page);
  C.check('an AudioContext exists after a key press', after.length >= 1, JSON.stringify(after));
  C.check('...and it is running', after.length >= 1 && after.every((x) => x === 'running'), JSON.stringify(after));
  C.check('the engine says it opened the sound device', s.logs.some((l) => /audio: SDL3 audio, 44100 Hz/.test(l)),
          s.logs.filter((l) => /audio/i.test(l)).join(' | '));

  // Options -> Music volume: stepping the slider plays the tick through the real device.
  const games = await page.evaluate(async () => {
    const m = await (await fetch('games.manifest.json')).json();
    return m.files.filter((f) => /^games\/[^/]+\/game\.json$/.test(f.path)).length;
  });
  await s.keys([...Array(games - 1).fill('ArrowDown'), 'Enter']);  // Home (one row down already): the entry after the games = Options
  await s.keys([...Array(6).fill('ArrowDown'), 'ArrowRight', 'ArrowRight', 'ArrowDown', 'ArrowLeft']);
  await page.waitForTimeout(400);
  C.check('the AudioContext keeps running', (await states(page)).every((x) => x === 'running'));
  C.noErrors(s, 'no console errors with sound on');
  // SDL3's Emscripten audio is built on ScriptProcessorNode, which Chromium flags as deprecated: that one
  // line comes from SDL, not from us, and is expected until SDL moves to AudioWorklet.
  const warnings = s.logs.filter((l) => /^(warning|warn): /.test(l) && /audio/i.test(l) && !/ScriptProcessorNode is deprecated/.test(l));
  C.check('no other audio-related console warning', warnings.length === 0, warnings.slice(0, 3).join(' | '));
  await close();
}
