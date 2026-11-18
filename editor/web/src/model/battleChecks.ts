// Checks for the battle scene's art (docs/BATTLE_SCENE.md), in plain words. `blocks` = the game's loader refuses the file for it
// (this mirrors engine/core/data/loader_battle_art.cpp, so a maker sees it before pressing Play); the rest loads fine but is worth a look.
import { ANIM_KEYS, HIT_ANIMS, MAX_FRAMES, assignKey, assignList, backgroundList, battleOf, setList, weaponGroups } from './battleArt'
import { effectList, effectProblems, effectName } from './battleEffects'
import { findSheet } from './graphics'
import type { ProjectDoc, Section } from './types'

export interface BattleCheck { section: Section; ids: string[]; text: string; blocks: boolean }

/** `dims` says how big an image file is (the editor's image cache); without it a frame cannot be checked against its image. */
export type Dims = (file: string) => { width: number; height: number } | undefined

export function battleChecks(doc: ProjectDoc, dims?: Dims): BattleCheck[] {
  const out: BattleCheck[] = []
  const add = (ids: string[], text: string, blocks: boolean) => out.push({ section: 'art', ids, text, blocks })
  const b = battleOf(doc)
  const used = new Set((b.assign ?? []).map((a) => a.set))
  for (const { id, set } of setList(doc)) {
    if (!set.animations.stand) add([id], `Set “${id}” has no “stand” animation. The game will not start until it has one.`, true)
    for (const key of ANIM_KEYS) {
      const a = set.animations[key]
      if (!a) continue
      const sheet = findSheet(doc, a.sheet)
      if (!sheet) { add([id], `Set “${id}”: “${key}” uses an image that is not in the game.`, true); continue }
      if (a.frames.length === 0) add([id], `Set “${id}”: “${key}” has no frames.`, true)
      if (a.frames.length > MAX_FRAMES) add([id], `Set “${id}”: “${key}” has ${a.frames.length} frames; the most an animation can have is ${MAX_FRAMES}.`, true)
      // A side's own image: the same layout as the one it replaces (the same piece size, every frame inside it).
      const replacements: Array<[string, string]> = [
        ...Object.entries(set.sides ?? {}).filter(([side]) => !a.sides?.[side]).map(([side, v]) => [side, v.sheet] as [string, string]),
        ...Object.entries(a.sides ?? {}).map(([side, v]) => [side, v.sheet] as [string, string]),
      ]
      for (const [side, sheetId] of replacements) {
        const other = findSheet(doc, sheetId)
        if (!other) { add([id], `Set “${id}”: the image for the side “${side}” is not in the game.`, true); continue }
        if (other.cellWidth !== sheet.cellWidth || other.cellHeight !== sheet.cellHeight) {
          add([id], `Set “${id}”: the image for the side “${side}” has pieces of ${other.cellWidth} × ${other.cellHeight} but “${key}” uses pieces of ${sheet.cellWidth} × ${sheet.cellHeight}. A side's image must be laid out the same way.`, true)
          continue
        }
        const od = dims?.(other.file)
        if (od) {
          const cols = Math.floor(od.width / other.cellWidth), rows = Math.floor(od.height / other.cellHeight)
          if (a.frames.some(([c, r]) => c >= cols || r >= rows)) add([id], `Set “${id}”: the image for the side “${side}” is smaller than the one it replaces; some frames of “${key}” are outside it (it has ${cols} × ${rows} pieces).`, true)
        }
      }
      const d = dims?.(sheet.file)
      if (d) {
        const cols = Math.floor(d.width / sheet.cellWidth), rows = Math.floor(d.height / sheet.cellHeight)
        const bad = a.frames.filter(([c, r]) => c >= cols || r >= rows)
        if (bad.length) add([id], `Set “${id}”: “${key}” has ${bad.length === 1 ? 'a frame' : `${bad.length} frames`} outside the image (it has ${cols} × ${rows} pieces).`, true)
      }
      if (a.hit !== undefined && a.hit >= a.frames.length) add([id], `Set “${id}”: the hit frame of “${key}” is beyond its last frame.`, true)
      if (HIT_ANIMS.includes(key) && a.hit === undefined) add([id], `Set “${id}”: “${key}” has no hit frame marked, so the blow lands on its last frame. Mark the frame where the weapon hits.`, false)
    }
    if (!used.has(id)) add([id], `Set “${id}” is not used by any unit type, character or weapon group yet.`, false)
    const anchor = set.anchor
    const stand = set.animations.stand
    const sheet = stand && findSheet(doc, stand.sheet)
    if (anchor && sheet && (anchor[0] >= sheet.cellWidth || anchor[1] >= sheet.cellHeight)) add([id], `Set “${id}”: the foot point is outside a ${sheet.cellWidth} × ${sheet.cellHeight} frame.`, true)
  }
  const groups = weaponGroups(doc)
  assignList(doc).forEach((a, i) => {
    const who = a.character ? `character “${a.character}”` : a.type ? `unit type “${a.type}”` : a.weapon ? `weapon “${a.weapon}”` : 'a rule'
    const ids = [a.set]
    if (!b.sets?.[a.set]) add(ids, `A rule gives ${who} the set “${a.set}”, which does not exist.`, true)
    if (!a.type && !a.character && !a.group && !a.weapon && !a.side) add(ids, `Rule ${i + 1} names no unit type, character, weapon, weapon group or side.`, true)
    if (a.weapon && a.group) add(ids, `Rule ${i + 1} names a weapon and a weapon group; name one of them (a weapon already belongs to its group).`, true)
    if (a.weapon && !(doc.weapons ?? []).some((w) => w.id === a.weapon)) add(ids, `Set “${a.set}” is given to the weapon “${a.weapon}”, which no longer exists.`, true)
    if (assignList(doc).some((o, j) => j < i && assignKey(o) === assignKey(a))) add(ids, `Rule ${i + 1} is the same as an earlier rule; the game refuses two rules for the same unit type, character, weapon, weapon group and side.`, true)
    if (a.type && !(doc.units ?? []).some((u) => u.id === a.type)) add(ids, `Set “${a.set}” is given to the unit type “${a.type}”, which no longer exists.`, true)
    if (a.character && !(doc.characters ?? []).some((c) => c.id === a.character)) add(ids, `Set “${a.set}” is given to the character “${a.character}”, who no longer exists.`, true)
    if (a.group && !groups.includes(a.group)) add(ids, `Set “${a.set}” is given to the weapon group “${a.group}”, which no weapon has.`, true)
  })
  for (const bg of backgroundList(doc)) {
    if (!findSheet(doc, bg.sheet)) add([bg.id], `Background “${bg.id}” uses an image that is not in the game.`, true)
  }
  for (const [terrain, bg] of Object.entries(b.terrainBackground ?? {})) {
    if (!(doc.terrain ?? []).some((t) => t.id === terrain)) add([terrain], `A battle background is set for the terrain “${terrain}”, which no longer exists.`, true)
    if (!b.backgrounds?.[bg]) add([terrain], `The terrain “${terrain}” uses the background “${bg}”, which does not exist.`, true)
  }
  effectList(doc).forEach((e, i) => { for (const p of effectProblems(doc, i, dims)) add([effectName(e, i)], p, true) })
  return out
}
