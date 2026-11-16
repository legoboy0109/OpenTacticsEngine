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

// The demo battle opens with a few lines of dialogue: holding the cancel key runs through them
// (docs/EVENTS.md "Dialogue (player)"). Harmless when no dialogue is up: a held cancel on the map
// does nothing.
export async function skipStory(s) {
  await s.page.keyboard.down('x');
  await s.page.waitForTimeout(800);
  await s.page.keyboard.up('x');
  await s.page.waitForTimeout(2600);  // the turn banner with the objective (2.4 s) follows the story
}

// Home -> Story Tactics Demo -> New game -> chapter 1's title card and opening scene -> battle, by keyboard.
export async function keyboardToBattle(s) {
  await s.keys(['Enter', 'Enter'], 200);  // the game's page, then "New game" (its first row without a save)
  await s.page.waitForTimeout(400);
  await s.keys(['Enter'], 300);           // the chapter's title card
  await s.page.keyboard.down('x');        // the scene before the battle: holding cancel skips it
  await s.page.waitForTimeout(800);
  await s.page.keyboard.up('x');
  await s.page.waitForTimeout(1500);      // the battle's opening animation swallows input
  await skipStory(s);
}

// Home -> battle by mouse / touch (tap = true).
export async function pointerToBattle(s, tap = false) {
  const hit = tap ? (x, y, p) => s.tapL(x, y, p) : (x, y, p) => s.clickL(x, y, p);
  await hit(90, 46);        // "Story Tactics Demo" in the list
  await hit(240, 46, 500);  // "New game"
  await hit(240, 100, 500); // the chapter's title card
  await hit(458, 200, 1800); // "Skip" on the scene before the battle; then the battle opens
  await hit(458, 203, 4500); // the "Skip" tab on the battle's opening dialogue; then the turn banner
                             // (after the view has moved where the story pointed; it carries the
                             // objective, 2.4 s) has to pass, or it eats the next tap
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

// Presses the "Wait" entry of the open action menu with the pointer. The lord's menu
// at (4,4) is Items, Storage, Wait: Wait is the third row (rows are 12 apart from y = 125).
export async function pointerWait(s, tap = false) {
  // By touch the first tap on a row selects it and the second chooses it.
  if (tap) { await s.tapL(172, 149); await s.tapL(172, 149); } else await s.clickL(172, 149);
}

// Opens the end-turn menu with the keyboard and ends the turn; waits for the AI.
export async function keyboardEndTurn(s, aiMs = 6000) {
  await s.keys(['m', 'Enter']);
  await s.page.waitForTimeout(aiMs);
}
