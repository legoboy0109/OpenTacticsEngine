// The battle scene's optional fixed stage (docs/BATTLE_SCENE.md "Fixed stage"): data/graphics.json "battle" > "stage". Pure edits on the
// document, and the checks mirror engine/core/data/loader_battle_stage.cpp (the game refuses the file for any of them).
// Defaults are never written ("center", "cover", the range line on); unknown keys stay.
import { backgroundList, battleOf, defaultBackground } from './battleArt'
import { withBattle } from './battleTables'
import { findSheet } from './graphics'
import { patchObj } from './util'
import type { BattleStageDef, ProjectDoc, StagePair, StagePoint } from './types'

export type PointGroup = 'feet' | 'feetFar' | 'platform' | 'platformFar'
export type Side = 'left' | 'right'
export const POINT_GROUPS: PointGroup[] = ['feet', 'feetFar', 'platform', 'platformFar']
export const STAGE_MIN = 16
export const STAGE_MAX = 1024
export type StageAlign = 'top' | 'center' | 'bottom'
export type StageFit = 'cover' | 'contain'

export const stageOf = (doc: ProjectDoc): BattleStageDef | undefined => battleOf(doc).stage
export const stageAlign = (s: BattleStageDef): StageAlign => (s.align === 'top' || s.align === 'bottom' ? s.align : 'center')
export const stageFit = (s: BattleStageDef): StageFit => (s.fit === 'contain' ? 'contain' : 'cover')
export const stageRangeLine = (s: BattleStageDef): boolean => s.rangeLine !== false

const clampSize = (n: number) => Math.max(STAGE_MIN, Math.min(STAGE_MAX, n))

/** The picture of the game's default backdrop (its piece size), when it has one. */
export function defaultBackdropSize(doc: ProjectDoc): { width: number; height: number } | undefined {
  const id = defaultBackground(doc)
  const bg = backgroundList(doc).find((b) => b.id === id)
  const sheet = bg && findSheet(doc, bg.sheet)
  return sheet ? { width: sheet.cellWidth, height: sheet.cellHeight } : undefined
}

/** Switching the stage on: the size of the default backdrop (else 240 x 160) and the two fighters standing in the lower middle. */
export function enableStage(doc: ProjectDoc): ProjectDoc {
  if (stageOf(doc)) return doc
  const size = defaultBackdropSize(doc)
  const width = clampSize(size?.width ?? 240), height = clampSize(size?.height ?? 160)
  const y = Math.round(height * 0.65)
  const stage: BattleStageDef = { width, height, feet: { left: [Math.round(width * 0.37), y], right: [Math.round(width * 0.63), y] } }
  return withBattle(doc, (b) => ({ ...b, stage }))
}
/** Switching it off removes the key (the scene is laid out as before). */
export function removeStage(doc: ProjectDoc): ProjectDoc {
  if (!stageOf(doc)) return doc
  return withBattle(doc, (b) => patchObj(b, { stage: undefined }))
}
const withStage = (doc: ProjectDoc, fn: (s: BattleStageDef) => BattleStageDef): ProjectDoc => {
  const s = stageOf(doc)
  if (!s) return doc
  const next = fn(s)
  return next === s ? doc : withBattle(doc, (b) => ({ ...b, stage: next }))
}

export function setStageSize(doc: ProjectDoc, key: 'width' | 'height', v: number): ProjectDoc {
  return withStage(doc, (s) => (s[key] === v ? s : { ...s, [key]: v }))
}
export function setStagePoint(doc: ProjectDoc, group: PointGroup, side: Side, axis: 0 | 1, v: number): ProjectDoc {
  return withStage(doc, (s) => {
    const cur = s[group]
    if (!cur) return s
    const pt: StagePoint = [...cur[side]] as StagePoint
    pt[axis] = v
    return { ...s, [group]: { ...cur, [side]: pt } }
  })
}
const copyPair = (p: StagePair): StagePair => ({ left: [...p.left] as StagePoint, right: [...p.right] as StagePoint })
/** Adds (a copy of the close points, or for a platform a place under the feet) or removes an optional group. Removing `platform` removes `platformFar`. */
export function setStageGroup(doc: ProjectDoc, group: 'feetFar' | 'platform' | 'platformFar', on: boolean): ProjectDoc {
  return withStage(doc, (s) => {
    if (!on) return patchObj(s, group === 'platform' ? { platform: undefined, platformFar: undefined } : { [group]: undefined })
    if (s[group]) return s
    if (group === 'feetFar') return { ...s, feetFar: copyPair(s.feet) }
    if (group === 'platform') {
      const at = (p: StagePoint): StagePoint => [Math.max(0, p[0] - 32), Math.max(0, p[1] - 8)]
      return { ...s, platform: { left: at(s.feet.left), right: at(s.feet.right) } }
    }
    return s.platform ? { ...s, platformFar: copyPair(s.platform) } : s
  })
}
export function setStageAlign(doc: ProjectDoc, v: StageAlign): ProjectDoc {
  return withStage(doc, (s) => patchObj(s, { align: v === 'center' ? undefined : v }))
}
export function setStageFit(doc: ProjectDoc, v: StageFit): ProjectDoc {
  return withStage(doc, (s) => patchObj(s, { fit: v === 'cover' ? undefined : v }))
}
export function setStageRangeLine(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withStage(doc, (s) => patchObj(s, { rangeLine: on ? undefined : false }))
}

const whole = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)
const GROUP_WORDS: Record<PointGroup, string> = { feet: 'foot points', feetFar: 'foot points at range', platform: 'platform places', platformFar: 'platform places at range' }

/** What the loader would refuse, in plain words (the game will not start until each is fixed). */
export function stageProblems(doc: ProjectDoc): string[] {
  const s = stageOf(doc)
  if (!s) return []
  const out: string[] = []
  const sizeOk = whole(s.width) && whole(s.height) && s.width >= STAGE_MIN && s.width <= STAGE_MAX && s.height >= STAGE_MIN && s.height <= STAGE_MAX
  if (!sizeOk) out.push(`The stage must be between ${STAGE_MIN} and ${STAGE_MAX} pixels wide and high.`)
  for (const group of POINT_GROUPS) {
    const pair = s[group]
    if (!pair) continue
    for (const side of ['left', 'right'] as const) {
      const p = pair[side]
      if (!Array.isArray(p) || p.length !== 2 || !whole(p[0]) || !whole(p[1])) { out.push(`The ${side} one of the ${GROUP_WORDS[group]} is not a pair of whole numbers.`); continue }
      if (sizeOk && (p[0] < 0 || p[1] < 0 || p[0] >= s.width || p[1] >= s.height))
        out.push(`The ${side} one of the ${GROUP_WORDS[group]} (${p[0]}, ${p[1]}) is outside the stage (${s.width} × ${s.height}).`)
    }
  }
  if (s.platformFar && !s.platform) out.push('The platform places at range need the platform places too.')
  return out
}
