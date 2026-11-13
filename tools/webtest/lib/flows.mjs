// Game flows shared by several suites. Coordinates are in logical pixels of the
// default 480x270 view (the demo games use it); tile (c, r) of the demo map
// "bridge" has its centre at (32 + 24c, 33 + 24r).
import { Session } from './session.mjs';

export const tile = (c, r) => [32 + 24 * c, 33 + 24 * r];
export const same = (a, b) => Buffer.compare(a, b) === 0;

// Opens `url`, waits for the ready marker. Returns the Session.
export async function open(page, outDir, label, url) {
  const s = new Session(page, outDir, label);
  await s.goto(url);
  await s.waitReady();
  return s;
}

// Home -> Emblem Demo -> Play -> map "bridge" -> battle, by keyboard.
export async function keyboardToBattle(s) {
  await s.keys(['Enter', 'Enter', 'Enter'], 200);
  await s.page.waitForTimeout(1200);  // the battle's opening animation swallows input
}

// Home -> battle by mouse / touch (tap = true).
export async function pointerToBattle(s, tap = false) {
  const hit = tap ? (x, y, p) => s.tapL(x, y, p) : (x, y, p) => s.clickL(x, y, p);
  await hit(90, 46);        // "Emblem Demo" in the list
  await hit(240, 46);       // "Play"
  await hit(240, 50, 1200); // the only map
}

// Select the lord at tile (2,4) and move him to (4,4) with the keyboard. The
// action menu ("Wait") is open afterwards.
export async function keyboardMoveLord(s) {
  await s.keys(['Enter', 'ArrowRight', 'ArrowRight', 'Enter']);
}

// Same with the pointer.
export async function pointerMoveLord(s, tap = false) {
  const hit = tap ? (x, y, p) => s.tapL(x, y, p) : (x, y, p) => s.clickL(x, y, p);
  await hit(...tile(2, 4));
  await hit(...tile(4, 4));
}

// Presses the "Wait" entry of the open action menu with the pointer.
export async function pointerWait(s, tap = false) {
  if (tap) await s.tapL(172, 125); else await s.clickL(172, 125);
}

// Opens the end-turn menu with the keyboard and ends the turn; waits for the AI.
export async function keyboardEndTurn(s, aiMs = 6000) {
  await s.keys(['m', 'Enter']);
  await s.page.waitForTimeout(aiMs);
}
