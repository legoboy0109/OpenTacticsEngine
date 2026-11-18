// Battle scene art in the document (docs/BATTLE_SCENE.md): data/graphics.json "battle" = animation sets of fighters,
// which unit type / character / weapon group uses which set, and the scene's backgrounds. Pure functions on the document.
// Effects (spells, arrows) are kept untouched when present but have no screen yet (they are written by hand).
import { withGraphics, findSheet, sheetsOf } from './graphics'
import { makeId } from './ids'
import { dropFromMapTables, dropTerrainFromTables, droppedPlatforms } from './battleTables'
import { patchObj } from './util'
import type { BattleAnimDef, BattleAssignDef, BattleSection, BattleSetDef, CellRef, Graphics, ProjectDoc, SideSheetDef } from './types'

/** The named animations of a fighter, in the order the editor lists them. Only `stand` is required. */
export const ANIM_KEYS = ['stand', 'standRanged', 'attack', 'miss', 'crit', 'critRanged', 'ranged', 'cast', 'dodge', 'dodgeRanged', 'hurt', 'defeat'] as const
export type AnimKey = (typeof ANIM_KEYS)[number]
/** The animations that deliver a blow: they have a hit frame. */
export const HIT_ANIMS: AnimKey[] = ['attack', 'miss', 'crit', 'critRanged', 'ranged', 'cast']
/** What an animation falls back to when the set has none (docs/BATTLE_SCENE.md "Which animation plays"); the rest play the simple motion. */
export const ANIM_FALLBACK: Partial<Record<AnimKey, AnimKey>> = { standRanged: 'stand', miss: 'attack', critRanged: 'crit', dodgeRanged: 'dodge' }
/** The most frames one animation may list (the game's limit; all from one image). */
export const MAX_FRAMES = 512
export const DEFAULT_FRAME_MS = 120

export const battleOf = (doc: ProjectDoc): BattleSection => doc.graphics?.battle ?? {}

const withBattle = (doc: ProjectDoc, fn: (b: BattleSection) => BattleSection): ProjectDoc =>
  withGraphics(doc, (g) => {
    const next = fn(g.battle ?? {})
    return patchObj(g, { battle: Object.keys(next).length ? next : undefined }) as Graphics
  })

const prune = <T,>(m: Record<string, T> | undefined): Record<string, T> | undefined => (m && Object.keys(m).length ? m : undefined)
const dropKeyOf = <T,>(m: Record<string, T> | undefined, k: string): Record<string, T> | undefined => {
  if (!m || !(k in m)) return m
  const { [k]: _gone, ...rest } = m
  return prune(rest)
}

// --- the default (game.json "battleScene") -----------------------------------------------------------------------------------------
export type BattleSceneMode = 'scene' | 'map'
export const battleSceneMode = (doc: ProjectDoc): BattleSceneMode => (doc.game?.battleScene === 'scene' ? 'scene' : 'map')
/** "On the map" is the default and is written as NO key, so existing games stay as they are. */
export function setBattleSceneMode(doc: ProjectDoc, mode: BattleSceneMode): ProjectDoc {
  if (!doc.game || battleSceneMode(doc) === mode) return doc
  return { ...doc, game: patchObj(doc.game, { battleScene: mode === 'scene' ? 'scene' : undefined }) }
}

// --- sets -----------------------------------------------------------------------------------------------------------------------------
export const setList = (doc: ProjectDoc): Array<{ id: string; set: BattleSetDef }> => Object.entries(battleOf(doc).sets ?? {}).map(([id, set]) => ({ id, set }))
export const findSet = (doc: ProjectDoc, id: string): BattleSetDef | undefined => battleOf(doc).sets?.[id]
export const newSetId = (doc: ProjectDoc, name: string): string => makeId(name, setList(doc).map((s) => s.id), { fallback: 'fighter' })

export function setIdProblem(doc: ProjectDoc, id: string, except?: string): string | null {
  if (id === '') return 'Type a name.'
  if (!/^[A-Za-z0-9_-]+$/.test(id)) return 'Use letters, digits, _ and - only (no spaces).'
  if (id !== except && findSet(doc, id)) return 'There is already an animation set with this name.'
  return null
}

/** The animation frames of one row of an image: cells [first, row] .. [first + count - 1, row]. */
export const framesFromRow = (row: number, count: number, first = 0): CellRef[] =>
  Array.from({ length: Math.max(1, Math.min(MAX_FRAMES, Math.round(count))) }, (_, i) => [first + i, Math.max(0, Math.round(row))] as CellRef)

/** A new set starts with a `stand` of the first image's first piece (a set without a stand is refused by the game). */
export function addSet(doc: ProjectDoc, id: string): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet || setIdProblem(doc, id)) return doc
  const stand: BattleAnimDef = { sheet, frames: [[0, 0]] }
  return withBattle(doc, (b) => ({ ...b, sets: { ...(b.sets ?? {}), [id]: { animations: { stand } } } }))
}

export function renameSet(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const sets = battleOf(doc).sets
  if (!sets || !(from in sets) || from === to || setIdProblem(doc, to, from)) return doc
  return withBattle(doc, (b) => ({
    ...b,
    sets: Object.fromEntries(Object.entries(b.sets ?? {}).map(([k, v]) => [k === from ? to : k, v])),
    assign: b.assign?.map((a) => (a.set === from ? { ...a, set: to } : a)),
  }))
}

/** Deletes the set and every assignment that used it. */
export function deleteSet(doc: ProjectDoc, id: string): ProjectDoc {
  if (!findSet(doc, id)) return doc
  return withBattle(doc, (b) => {
    const rules = b.assign?.filter((a) => a.set !== id)
    return patchObj(b, { sets: dropKeyOf(b.sets, id), assign: rules && rules.length ? rules : undefined })
  })
}

const mapSet = (doc: ProjectDoc, id: string, fn: (s: BattleSetDef) => BattleSetDef): ProjectDoc => {
  const s = findSet(doc, id)
  if (!s) return doc
  const next = fn(s)
  return next === s ? doc : withBattle(doc, (b) => ({ ...b, sets: { ...b.sets, [id]: next } }))
}

export const setFaces = (doc: ProjectDoc, id: string, faces: 'right' | 'left'): ProjectDoc => mapSet(doc, id, (s) => patchObj(s, { faces: faces === 'left' ? 'left' : undefined }))
/** Mirroring is on by default and is written only when it is turned off. */
export const setMirror = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => mapSet(doc, id, (s) => patchObj(s, { mirror: on ? undefined : false }))
export function setAnchor(doc: ProjectDoc, id: string, anchor: [number, number] | undefined): ProjectDoc {
  return mapSet(doc, id, (s) => patchObj(s, { anchor }))
}

/** Replaces or changes one animation of a set. `null` removes it (the stand cannot be removed). */
export function setAnim(doc: ProjectDoc, id: string, key: AnimKey, patch: Partial<BattleAnimDef> | null): ProjectDoc {
  return mapSet(doc, id, (s) => {
    if (patch === null) {
      if (key === 'stand') return s
      const { [key]: _gone, ...rest } = s.animations
      return { ...s, animations: rest }
    }
    const cur = s.animations[key]
    const first = sheetsOf(doc)[0]?.id ?? ''
    const base: BattleAnimDef = cur ?? { sheet: first, frames: [[0, 0]] }
    const next = patchObj(base, patch) as BattleAnimDef
    return { ...s, animations: { ...s.animations, [key]: tidyAnim(next) } }
  })
}

/** Keeps the numbers consistent: marks beyond the last frame go, a list of times follows the frame count. */
function tidyAnim(a: BattleAnimDef): BattleAnimDef {
  const n = a.frames.length
  const out = { ...a }
  if (out.hit !== undefined && out.hit >= n) out.hit = n - 1
  if (out.spawn !== undefined && out.spawn >= n) out.spawn = n - 1
  if (out.sounds) {  // a sound on a frame the animation no longer has goes
    const kept = Object.fromEntries(Object.entries(out.sounds).filter(([f]) => Number(f) < n))
    if (Object.keys(kept).length) out.sounds = kept
    else delete out.sounds
  }
  if (Array.isArray(out.ms)) {
    const ms = out.ms.slice(0, n)
    while (ms.length < n) ms.push(ms[ms.length - 1] ?? DEFAULT_FRAME_MS)
    out.ms = ms
  }
  return out
}

/** The frame times of an animation as a list, one per frame. */
export function msList(a: BattleAnimDef): number[] {
  const n = a.frames.length
  if (Array.isArray(a.ms)) { const list = a.ms; return Array.from({ length: n }, (_, i) => list[i] ?? DEFAULT_FRAME_MS) }
  return Array.from({ length: n }, () => (typeof a.ms === 'number' ? a.ms : DEFAULT_FRAME_MS))
}

// --- who uses which set ---------------------------------------------------------------------------------------------------------------
export const assignList = (doc: ProjectDoc): BattleAssignDef[] => battleOf(doc).assign ?? []

const setAssign = (doc: ProjectDoc, list: BattleAssignDef[]): ProjectDoc => withBattle(doc, (b) => patchObj(b, { assign: list.length ? list : undefined }))

/** A new rule for the unit type (the first one that has no rule yet, else the first). */
export function addAssign(doc: ProjectDoc, set: string, type?: string): ProjectDoc {
  const t = type ?? (doc.units ?? []).find((u) => !assignList(doc).some((a) => a.type === u.id && !a.character && !a.group))?.id ?? doc.units?.[0]?.id
  return t ? setAssign(doc, [...assignList(doc), { set, type: t }]) : doc
}
export function updateAssign(doc: ProjectDoc, index: number, patch: Partial<BattleAssignDef>): ProjectDoc {
  const list = assignList(doc)
  if (index < 0 || index >= list.length) return doc
  return setAssign(doc, list.map((a, i) => (i === index ? (patchObj(a, patch) as BattleAssignDef) : a)))
}
export function removeAssign(doc: ProjectDoc, index: number): ProjectDoc {
  return setAssign(doc, assignList(doc).filter((_, i) => i !== index))
}
/** What makes a rule the same as another (the game refuses two rules with the same key). */
export function assignKey(a: BattleAssignDef): string { return `${a.character ?? ''}|${a.type ?? ''}|${a.group ?? ''}|${a.weapon ?? ''}|${a.side ?? ''}` }
/** The weapon groups the game's weapons have (what a rule's "weapon group" can name). */
export const weaponGroups = (doc: ProjectDoc): string[] => Array.from(new Set((doc.weapons ?? []).map((w) => w.group).filter((g): g is string => !!g)))
/** The weapons a rule can name. */
export const weaponChoices = (doc: ProjectDoc): Array<{ id: string; name: string }> => (doc.weapons ?? []).map((w) => ({ id: w.id, name: w.name || w.id }))

// --- a different image for a side ------------------------------------------------------------------------------------------------------
/** The image the fighters of `side` draw the set from (the set's own "sides" entry), or '' = the animations' own images. */
export const setSideSheet = (set: BattleSetDef, side: string): string => set.sides?.[side]?.sheet ?? ''
/** Gives a side another image for the whole set ('' = back to the set's own). It must have the same layout: the game checks that. */
export function updateSideSheet(doc: ProjectDoc, id: string, side: string, sheet: string): ProjectDoc {
  return mapSet(doc, id, (s) => {
    const sides = { ...(s.sides ?? {}) }
    if (sheet === '') delete sides[side]; else sides[side] = { sheet }
    return patchObj(s, { sides: Object.keys(sides).length ? sides : undefined })
  })
}

// --- backgrounds ------------------------------------------------------------------------------------------------------------------------
export const backgroundList = (doc: ProjectDoc): Array<{ id: string; sheet: string; cell: CellRef }> =>
  Object.entries(battleOf(doc).backgrounds ?? {}).map(([id, b]) => ({ id, sheet: b.sheet, cell: b.cell }))
export const newBackgroundId = (doc: ProjectDoc, name: string): string => makeId(name, backgroundList(doc).map((b) => b.id), { fallback: 'scene' })

export function addBackground(doc: ProjectDoc, id: string): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet || backgroundList(doc).some((b) => b.id === id)) return doc
  return withBattle(doc, (b) => ({ ...b, backgrounds: { ...(b.backgrounds ?? {}), [id]: { sheet, cell: [0, 0] } } }))
}
export function setBackgroundPicture(doc: ProjectDoc, id: string, sheet: string, cell: CellRef): ProjectDoc {
  const cur = battleOf(doc).backgrounds?.[id]
  return cur ? withBattle(doc, (b) => ({ ...b, backgrounds: { ...b.backgrounds, [id]: patchObj(cur, { sheet, cell }) } })) : doc
}
/** Removes the background and what pointed at it (terrain choices, the default). */
export function removeBackground(doc: ProjectDoc, id: string): ProjectDoc {
  return dropFromMapTables(withBattle(doc, (b) => patchObj(b, {
    backgrounds: dropKeyOf(b.backgrounds, id),
    terrainBackground: prune(Object.fromEntries(Object.entries(b.terrainBackground ?? {}).filter(([, v]) => v !== id))),
    defaultBackground: b.defaultBackground === id ? undefined : b.defaultBackground,
  })), 'battleBackgrounds', (v) => v === id)  // (a map's table forgets it too: model/battleTables.ts)
}
export const terrainBackground = (doc: ProjectDoc, terrain: string): string => battleOf(doc).terrainBackground?.[terrain] ?? ''
export function setTerrainBackground(doc: ProjectDoc, terrain: string, background: string): ProjectDoc {
  return withBattle(doc, (b) => patchObj(b, {
    terrainBackground: prune(background === '' ? Object.fromEntries(Object.entries(b.terrainBackground ?? {}).filter(([k]) => k !== terrain)) : { ...(b.terrainBackground ?? {}), [terrain]: background }),
  }))
}
export const defaultBackground = (doc: ProjectDoc): string => battleOf(doc).defaultBackground ?? ''
export function setDefaultBackground(doc: ProjectDoc, background: string): ProjectDoc {
  return withBattle(doc, (b) => patchObj(b, { defaultBackground: background === '' ? undefined : background }))
}

// --- follow-through: what goes when something else goes --------------------------------------------------------------------------------
/** Removes every use of the image: animations (and sets left without a stand, with their rules), effects, backgrounds. */
export function dropBattleSheet(g: Graphics, sheetId: string): Graphics {
  const b = g.battle
  if (!b) return g
  const sets: Record<string, BattleSetDef> = {}
  const goneSets = new Set<string>()
  for (const [id, s] of Object.entries(b.sets ?? {})) {
    // A side's own image that is removed only drops that side's entry; the set stays.
    const dropSides = (m: Record<string, SideSheetDef> | undefined) => {
      if (!m) return m
      const kept = Object.fromEntries(Object.entries(m).filter(([, v]) => v.sheet !== sheetId))
      return Object.keys(kept).length === Object.keys(m).length ? m : prune(kept)
    }
    const anims = Object.fromEntries(Object.entries(s.animations).filter(([, a]) => a.sheet !== sheetId).map(([k, a]) => [k, a.sides ? (patchObj(a, { sides: dropSides(a.sides) }) as BattleAnimDef) : a]))
    if (!anims.stand) { goneSets.add(id); continue }
    const sides = dropSides(s.sides)
    sets[id] = Object.keys(anims).length === Object.keys(s.animations).length && sides === s.sides && Object.entries(anims).every(([k, a]) => a === s.animations[k]) ? s : patchObj({ ...s, animations: anims }, { sides })
  }
  const backgrounds = Object.fromEntries(Object.entries(b.backgrounds ?? {}).filter(([, v]) => v.sheet !== sheetId))
  const goneBg = new Set(Object.keys(b.backgrounds ?? {}).filter((k) => !(k in backgrounds)))
  const next = patchObj(b, {
    sets: prune(sets),
    assign: (() => { const a = b.assign?.filter((r) => !goneSets.has(r.set)); return a && a.length ? a : undefined })(),
    effects: (() => { const e = b.effects?.filter((x) => x.sheet !== sheetId); return e && e.length ? e : undefined })(),
    backgrounds: prune(backgrounds),
    terrainBackground: prune(Object.fromEntries(Object.entries(b.terrainBackground ?? {}).filter(([, v]) => !goneBg.has(v)))),
    defaultBackground: b.defaultBackground && goneBg.has(b.defaultBackground) ? undefined : b.defaultBackground,
    ...droppedPlatforms(b, sheetId),
  })
  return patchObj(g, { battle: Object.keys(next).length ? next : undefined }) as Graphics
}

/** A unit type, a character or a terrain was deleted: the rules and choices that named it go. */
export function dropBattleOf(doc: ProjectDoc, what: 'type' | 'character' | 'terrain', id: string): ProjectDoc {
  const b = doc.graphics?.battle
  if (!b) return doc
  if (what === 'terrain') {
    const rest = dropTerrainFromTables(doc, id)  // the platform table and the maps' tables (model/battleTables.ts)
    if (!b.terrainBackground || !(id in b.terrainBackground)) return rest
    return setTerrainBackground(rest, id, '')
  }
  const list = (b.assign ?? []).filter((a) => (what === 'type' ? a.type !== id : a.character !== id))
  return list.length === (b.assign ?? []).length ? doc : setAssign(doc, list)
}

export { findSheet }
