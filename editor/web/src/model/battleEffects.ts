// Battle scene effects in the document (docs/BATTLE_SCENE.md "Effects"): data/graphics.json "battle" > "effects". Pure functions on the
// document, and the checks that mirror engine/core/data/loader_battle_art.cpp (readEffects / readEffectExtras), so a maker sees a refusal
// before pressing Play. An entry is one of: a weapon's or weapon group's effect (pictures, and maybe a list / byType), an assignment only
// (a weapon or group, no pictures, a list and / or byType), or a library effect (an id and pictures; lists name it).
import { findSheet, sheetsOf } from './graphics'
import { battleOf, framesFromRow, weaponGroups } from './battleArt'
import { soundIds } from './audio'
import { patchObj } from './util'
import type { BattleEffectDef, FxFlashDef, FxShakeDef, ProjectDoc } from './types'
import { withGraphics } from './graphics'
import type { Graphics } from './types'

export const effectList = (doc: ProjectDoc): BattleEffectDef[] => battleOf(doc).effects ?? []

const setEffects = (doc: ProjectDoc, list: BattleEffectDef[]): ProjectDoc =>
  withGraphics(doc, (g) => {
    const b = g.battle ?? {}
    const next = patchObj(b, { effects: list.length ? list : undefined })
    return patchObj(g, { battle: Object.keys(next).length ? next : undefined }) as Graphics
  })

/** What an entry is, for the form: a weapon's effect, a weapon group's effect, or a library effect (named, no weapon: only lists use it). */
export type EffectFor = 'weapon' | 'group' | 'library'
export const effectFor = (e: BattleEffectDef): EffectFor => (e.weapon ? 'weapon' : e.group ? 'group' : 'library')
/** The entry has pictures of its own (an assignment-only entry does not). */
export const hasPictures = (e: BattleEffectDef): boolean => !!e.sheet && Array.isArray(e.frames) && e.frames.length > 0
/** What the effect is called in lists and messages. */
export const effectName = (e: BattleEffectDef, i: number): string => e.id || e.weapon || e.group || `effect ${i + 1}`
/** The effects a list can name: the ones with an id and pictures of their own. */
export const listable = (doc: ProjectDoc, except?: number): Array<{ id: string; index: number }> =>
  effectList(doc).flatMap((e, i) => (e.id && hasPictures(e) && i !== except ? [{ id: e.id, index: i }] : []))

/** The ids an entry's `effects` / `byType` value names, as a list. */
export const idList = (v: string | string[] | undefined): string[] => (v === undefined ? [] : Array.isArray(v) ? v : [v])

/** A new effect: for the first weapon that has none yet (else a library effect), with the first image's first piece as its picture. */
export function addEffect(doc: ProjectDoc): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet) return doc
  const list = effectList(doc)
  const weapon = (doc.weapons ?? []).find((w) => !list.some((e) => e.weapon === w.id))?.id
  const base: BattleEffectDef = weapon ? { weapon, sheet, frames: [[0, 0]] } : { id: freshId(list), sheet, frames: [[0, 0]] }
  return setEffects(doc, [...list, base])
}
function freshId(list: BattleEffectDef[]): string {
  for (let n = 1; ; n++) if (!list.some((e) => e.id === `effect${n}`)) return `effect${n}`
}

/** Changes one entry; `undefined` values delete the key. A new id is followed by the lists that named the old one. */
export function updateEffect(doc: ProjectDoc, index: number, patch: Partial<BattleEffectDef>): ProjectDoc {
  const list = effectList(doc)
  if (index < 0 || index >= list.length) return doc
  const old = list[index].id
  let next = list.map((e, i) => (i === index ? (patchObj(e, patch) as BattleEffectDef) : e))
  const now = next[index].id
  if (old && old !== now) next = next.map((e) => renameInLists(e, old, now))
  return setEffects(doc, next)
}
const rename = (v: string | string[] | undefined, from: string, to: string | undefined): string | string[] | undefined => {
  const l = idList(v)
  if (!l.includes(from)) return v
  const out = to ? l.map((x) => (x === from ? to : x)) : l.filter((x) => x !== from)
  return Array.isArray(v) || out.length !== 1 ? out : out[0]
}
function renameInLists(e: BattleEffectDef, from: string, to: string | undefined): BattleEffectDef {
  const lists = e.effects !== undefined ? rename(e.effects, from, to) : undefined
  const byType = e.byType ? Object.fromEntries(Object.entries(e.byType).map(([t, v]) => [t, rename(v, from, to) ?? []])) : undefined
  return patchObj(e, { effects: lists === undefined || idList(lists).length === 0 ? undefined : lists, byType })
}

/** Removes an entry and, from the other entries' lists, the id it had. */
export function removeEffect(doc: ProjectDoc, index: number): ProjectDoc {
  const list = effectList(doc)
  if (index < 0 || index >= list.length) return doc
  const id = list[index].id
  let next = list.filter((_, i) => i !== index)
  if (id) next = next.map((e) => renameInLists(e, id, undefined))
  return setEffects(doc, next)
}

/** Switches who the entry plays for; a library effect loses its lists (the game refuses them there). */
export function setEffectFor(doc: ProjectDoc, index: number, kind: EffectFor, value?: string): ProjectDoc {
  const e = effectList(doc)[index]
  if (!e) return doc
  const id = e.id ?? (kind === 'library' ? freshId(effectList(doc)) : undefined)
  if (kind === 'weapon') return updateEffect(doc, index, { weapon: value ?? (doc.weapons ?? [])[0]?.id, group: undefined })
  if (kind === 'group') return updateEffect(doc, index, { group: value ?? weaponGroups(doc)[0], weapon: undefined })
  return updateEffect(doc, index, { weapon: undefined, group: undefined, id, effects: undefined, byType: undefined })
}

/** The picture of an effect: its image and a run of frames along one row (several images in one effect are left as the file has them). */
export function setEffectPicture(doc: ProjectDoc, index: number, patch: { sheet?: string; row?: number; count?: number }): ProjectDoc {
  const e = effectList(doc)[index]
  if (!e) return doc
  const frames = e.frames ?? [[0, 0]]
  const row = patch.row ?? frames[0]?.[1] ?? 0
  const count = patch.count ?? frames.length
  const next: Partial<BattleEffectDef> = { frames: framesFromRow(row, count), ...(patch.sheet !== undefined ? { sheet: patch.sheet } : {}) }
  if (Array.isArray(e.ms)) next.ms = undefined  // (one time for every frame once the frames are remade)
  return updateEffect(doc, index, next)
}
/** Do the frames run along one row from the left (what the Row and Frames boxes can describe)? */
export const isRowRun = (e: BattleEffectDef): boolean => (e.frames ?? []).every((f, i, all) => f.length === 2 && f[0] === i && f[1] === all[0][1])

// --- the lists of timed things -----------------------------------------------------------------------------------------------------
/** Sounds as sorted [ms, id] rows. */
export const effectSounds = (e: BattleEffectDef): Array<[number, string]> => Object.entries(e.sounds ?? {}).map(([ms, id]) => [Number(ms), id] as [number, string]).sort((a, b) => a[0] - b[0])
export function setEffectSound(doc: ProjectDoc, index: number, ms: number, id: string | undefined): ProjectDoc {
  const e = effectList(doc)[index]
  if (!e) return doc
  const rows = { ...(e.sounds ?? {}) }
  if (id === undefined) delete rows[String(ms)]
  else rows[String(ms)] = id
  return updateEffect(doc, index, { sounds: Object.keys(rows).length ? rows : undefined })
}
/** The next free time for a new sound: 0 first, then every 100 ms. */
export const nextSoundMs = (e: BattleEffectDef): number => { let ms = 0; while (e.sounds && String(ms) in e.sounds) ms += 100; return ms }

export const DEFAULT_FLASH: FxFlashDef = { atMs: 0, ms: 80, color: '#ffffff' }
export const DEFAULT_SHAKE: FxShakeDef = { atMs: 0, ms: 200, strength: 2 }
export function setEffectTimed(doc: ProjectDoc, index: number, key: 'flash' | 'shake', rows: Array<FxFlashDef | FxShakeDef>): ProjectDoc {
  return updateEffect(doc, index, { [key]: rows.length ? rows : undefined })
}

/** Lists: the `effects` of an entry, and its `byType` rows. An emptied list is written as no key. */
export function setEffectIds(doc: ProjectDoc, index: number, ids: string[]): ProjectDoc {
  return updateEffect(doc, index, { effects: ids.length === 0 ? undefined : ids.length === 1 ? ids[0] : ids })
}
export function setByType(doc: ProjectDoc, index: number, type: string, ids: string[] | null): ProjectDoc {
  const e = effectList(doc)[index]
  if (!e) return doc
  const rows = { ...(e.byType ?? {}) }
  if (ids === null) delete rows[type]
  else rows[type] = ids.length === 1 ? ids[0] : ids  // ([] = no effect for that type)
  return updateEffect(doc, index, { byType: Object.keys(rows).length ? rows : undefined })
}
export function renameByTypeKey(doc: ProjectDoc, index: number, from: string, to: string): ProjectDoc {
  const e = effectList(doc)[index]
  if (!e?.byType || !(from in e.byType) || from === to || to in e.byType) return doc
  return updateEffect(doc, index, { byType: Object.fromEntries(Object.entries(e.byType).map(([k, v]) => [k === from ? to : k, v])) })
}

// --- checks (the loader's refusals, in plain words) --------------------------------------------------------------------------------------
const ID = /^[A-Za-z0-9_-]+$/
const COLOR = /^#[0-9A-Fa-f]{6}$/
const whole = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)

/** What the game would refuse in effect `index` (the text starts with the effect's name). */
export function effectProblems(doc: ProjectDoc, index: number, dims?: (file: string) => { width: number; height: number } | undefined): string[] {
  const list = effectList(doc)
  const e = list[index]
  if (!e) return []
  const out: string[] = []
  const name = `Effect “${effectName(e, index)}”`
  const bad = (t: string) => out.push(`${name}: ${t}`)
  const lib = !e.weapon && !e.group && !!e.id
  if (e.id !== undefined && !ID.test(e.id)) bad('the name may use letters, digits, _ and - only.')
  if (e.id && list.some((o, j) => j < index && o.id === e.id)) bad('another effect has the same name.')
  if (!lib && !e.weapon && !e.group) bad('name a weapon or a weapon group (or give it a name and use it in a list).')
  if (e.weapon && e.group) bad('name a weapon or a weapon group, not both.')
  if (e.weapon && !(doc.weapons ?? []).some((w) => w.id === e.weapon)) bad(`the weapon “${e.weapon}” does not exist.`)
  if (e.group && !weaponGroups(doc).includes(e.group)) bad(`no weapon has the group “${e.group}”.`)
  if (!lib && list.some((o, j) => j < index && !!(o.weapon || o.group) && o.weapon === e.weapon && o.group === e.group)) bad(`there is already an effect for ${e.weapon ?? e.group}.`)
  const lists = e.effects !== undefined || e.byType !== undefined
  if (lists && lib) bad('lists and by-unit-type choices belong to a weapon or a weapon group.')
  for (const id of idList(e.effects)) refProblem(doc, id, bad, '')
  for (const [type, v] of Object.entries(e.byType ?? {})) {
    if (!(doc.units ?? []).some((u) => u.id === type)) bad(`there is no unit type “${type}” (by unit type).`)
    for (const id of idList(v)) refProblem(doc, id, bad, ` for the unit type “${type}”`)
  }
  if (!hasPictures(e)) {
    if (!(lists && !lib && !e.sheet && !e.frames)) bad('it needs an image and frames.')
    return out
  }
  const sheet = findSheet(doc, e.sheet ?? '')
  if (!sheet) { bad('its image is not in the game.'); return out }
  const d = dims?.(sheet.file)
  if (d) {
    const cols = Math.floor(d.width / sheet.cellWidth), rows = Math.floor(d.height / sheet.cellHeight)
    if ((e.frames ?? []).some((f) => f.length === 2 && (f[0] >= cols || f[1] >= rows))) bad(`some frames are outside the image (it has ${cols} × ${rows} pieces).`)
  }
  if ((e.frames ?? []).length > 512) bad('more than 512 frames.')
  const travel = e.mode === 'travel'
  const layer = e.layer ?? 'front'
  if (e.mode !== undefined && e.mode !== 'target' && e.mode !== 'travel') bad('it must appear on the foe or fly to it.')
  if (e.travelMs !== undefined && (!whole(e.travelMs) || e.travelMs < 20 || e.travelMs > 5000)) bad('the flight time must be between 20 and 5000 milliseconds.')
  if (e.hitAtMs !== undefined) {
    if (!whole(e.hitAtMs) || e.hitAtMs < 0 || e.hitAtMs > 15000) bad('the hit time must be between 0 and 15000 milliseconds.')
    if (travel) bad('a flying effect hits when it arrives; the hit time is for an effect that appears in place.')
  }
  if (e.layer !== undefined && !['front', 'behind', 'screen'].includes(e.layer)) bad('the layer must be front, behind or screen.')
  if (layer === 'screen' && travel) bad('an effect that covers the screen cannot fly.')
  if (e.mirror && layer !== 'screen') bad('“turn round” is only for an effect that covers the screen.')
  if (e.at !== undefined && e.at !== 'target' && e.at !== 'striker') bad('“at” must be the target or the striker.')
  if (e.at === 'striker' && travel) bad('an effect on the striker appears in place; it cannot fly.')
  if (e.at === 'striker' && layer === 'screen') bad('an effect that covers the screen has no place to appear at.')
  if (e.returns && !travel) bad('only a flying effect can fly back.')
  if (e.lingerMs !== undefined) {
    if (!whole(e.lingerMs) || e.lingerMs < 0 || e.lingerMs > 5000) bad('the time it stays must be between 0 and 5000 milliseconds.')
    else if (e.lingerMs > 0 && !travel) bad('only a flying effect can stay at the target.')
  }
  for (const [ms, id] of Object.entries(e.sounds ?? {})) {
    if (!/^\d+$/.test(ms) || Number(ms) > 60000) bad(`a sound is set at “${ms}”; it must be milliseconds from 0 to 60000.`)
    if (typeof id !== 'string' || !ID.test(id)) bad(`the sound at ${ms} ms must name a sound.`)
    else if (!soundIds(doc).includes(id)) bad(`the sound “${id}” is not in the game's sounds.`)
  }
  ;(e.flash ?? []).forEach((f, i) => {
    if (!whole(f.atMs) || f.atMs < 0 || f.atMs > 60000) bad(`flash ${i + 1}: the start must be between 0 and 60000 milliseconds.`)
    if (!whole(f.ms) || f.ms < 10 || f.ms > 5000) bad(`flash ${i + 1}: the length must be between 10 and 5000 milliseconds.`)
    if (f.color !== undefined && !COLOR.test(f.color)) bad(`flash ${i + 1}: the colour must look like #ffffff.`)
  })
  ;(e.shake ?? []).forEach((f, i) => {
    if (!whole(f.atMs) || f.atMs < 0 || f.atMs > 60000) bad(`shake ${i + 1}: the start must be between 0 and 60000 milliseconds.`)
    if (!whole(f.ms) || f.ms < 10 || f.ms > 5000) bad(`shake ${i + 1}: the length must be between 10 and 5000 milliseconds.`)
    if (!whole(f.strength) || f.strength < 1 || f.strength > 64) bad(`shake ${i + 1}: the strength must be between 1 and 64 pixels.`)
  })
  if ((e.flash ?? []).length > 32 || (e.shake ?? []).length > 32) bad('at most 32 flashes and 32 shakes.')
  return out
}
function refProblem(doc: ProjectDoc, id: string, bad: (t: string) => void, where: string) {
  const target = effectList(doc).find((o) => o.id === id)
  if (!target) bad(`the list${where} names “${id}”, which is not an effect.`)
  else if (!hasPictures(target)) bad(`the list${where} names “${id}”, which has no pictures of its own.`)
}
export const effectChecks = (doc: ProjectDoc, dims?: (file: string) => { width: number; height: number } | undefined): string[] =>
  effectList(doc).flatMap((_, i) => effectProblems(doc, i, dims))
