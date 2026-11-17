// What the "Preview a fight" button plays (docs/BATTLE_SCENE.md): the attacker strikes twice, the first blow hits, the second is
// dodged. Pure: time in, what each fighter shows out. A missing animation is the fallback motion of the stand picture, like the game.
import { DEFAULT_FRAME_MS, msList, type AnimKey } from '../model/battleArt'
import type { BattleAnimDef, BattleSetDef } from '../model/types'

export type Fallback = 'none' | 'lunge' | 'shake' | 'sidestep'
export interface FighterPose {
  anim: AnimKey          // the animation the set plays (stand when it rests or the fallback moves)
  localMs: number        // time since it began
  fallback: Fallback     // the fallback motion that moves the stand picture instead (the set has no such animation)
  progress: number       // 0..1 through that motion
}

export const LOOP_MS = 4000
const FALLBACK = { lunge: { total: 450, hit: 280 }, shake: { total: 320 }, sidestep: { total: 380 } }

export const animTotal = (a: BattleAnimDef): number => msList(a).reduce((x, y) => x + y, 0)
/** When the hit frame begins, in ms from the start of the animation (the last frame when none is marked). */
export function hitAtMs(a: BattleAnimDef): number {
  const list = msList(a)
  const frame = a.hit !== undefined ? Math.min(a.hit, list.length - 1) : list.length - 1
  return list.slice(0, frame).reduce((x, y) => x + y, 0)
}
/** The frame an animation shows `ms` after it began (a loop wraps, another holds its last frame). */
export function frameAtMs(a: BattleAnimDef, ms: number, loop: boolean): number {
  const list = msList(a)
  const total = list.reduce((x, y) => x + y, 0) || DEFAULT_FRAME_MS
  let t = Math.max(0, ms)
  if (loop) t %= total
  let acc = 0
  for (let i = 0; i < list.length; i++) { acc += list[i]; if (t < acc) return i }
  return list.length - 1
}

const stand: FighterPose = { anim: 'stand', localMs: 0, fallback: 'none', progress: 0 }

/** The attack animation the preview uses for the striker: attack, or the fallback lunge. */
function attackOf(set: BattleSetDef | undefined): { key: AnimKey; total: number; hit: number; fallback: boolean } {
  const a = set?.animations.attack
  return a ? { key: 'attack', total: animTotal(a), hit: hitAtMs(a), fallback: false } : { key: 'attack', total: FALLBACK.lunge.total, hit: FALLBACK.lunge.hit, fallback: true }
}

function reaction(set: BattleSetDef | undefined, key: 'hurt' | 'dodge', since: number): FighterPose {
  const a = set?.animations[key]
  const total = a ? animTotal(a) : key === 'hurt' ? FALLBACK.shake.total : FALLBACK.sidestep.total
  if (since < 0 || since >= total) return stand
  return a ? { anim: key, localMs: since, fallback: 'none', progress: since / total }
    : { anim: 'stand', localMs: 0, fallback: key === 'hurt' ? 'shake' : 'sidestep', progress: since / total }
}

export function fightPreview(attacker: BattleSetDef | undefined, defender: BattleSetDef | undefined, tMs: number): { attacker: FighterPose; defender: FighterPose; strike: 0 | 1 | 2 } {
  const t = ((tMs % LOOP_MS) + LOOP_MS) % LOOP_MS
  const atk = attackOf(attacker)
  const starts = [500, 2000]
  let att = stand, def = stand, strike: 0 | 1 | 2 = 0
  for (let i = 0; i < 2; i++) {
    const since = t - starts[i]
    if (since < 0) continue
    if (since < atk.total) {
      att = atk.fallback ? { anim: 'stand', localMs: 0, fallback: 'lunge', progress: since / atk.total } : { anim: 'attack', localMs: since, fallback: 'none', progress: since / atk.total }
      strike = (i + 1) as 1 | 2
    }
    // the blow: the first lands (hurt), the second is dodged (the foe moves a little before it lands)
    def = i === 0 ? (since >= atk.hit ? reaction(defender, 'hurt', since - atk.hit) : def) : reaction(defender, 'dodge', since - (atk.hit - 120))
    if (since >= 0 && since < atk.total) strike = (i + 1) as 1 | 2
  }
  return { attacker: att, defender: def, strike }
}
