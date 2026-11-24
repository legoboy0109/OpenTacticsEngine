// What the game's loader refuses about the newer keys of data/graphics.json, in plain words (engine/core/data/loader_graphics.cpp
// readUnitOffset / readPortraitSize / "characters", loader_unit_frames.cpp readWalk / readUnitPoses): a character's own picture, the
// poses of a unit picture, a sheet's unit offset and the portrait size. `dims` (the image library's width and height of a file) lets
// the cells of a pose be checked against the image; without it that one check is skipped.
import { FRAME_KEYS } from './graphics'
import { walkProblems, type Dims } from './walking'
import type { Look, ProjectDoc } from './types'

export interface ArtCheck { text: string }

const isWhole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)
export const PORTRAIT_MIN = 24
export const PORTRAIT_MAX = 160
export const UNIT_OFFSET_MAX = 256

/** The words for each kind of frames in a message. */
const POSE_WORD: Record<(typeof FRAME_KEYS)[number], string> = { walk: 'walking', facing: 'facing', attack: 'attack', hurt: 'hurt', ko: 'defeated' }

export function artChecks(doc: ProjectDoc, dims?: Dims): ArtCheck[] {
  const g = doc.graphics
  const out: ArtCheck[] = []
  if (!g) return out
  const say = (text: string) => out.push({ text })

  // A sheet's unit offset.
  for (const s of g.sheets) {
    if (s.unitOffset === undefined) continue
    const o = s.unitOffset as unknown
    if (!Array.isArray(o) || o.length !== 2 || !isWhole(o[0]) || !isWhole(o[1]) || Math.abs(o[0]) > UNIT_OFFSET_MAX || Math.abs(o[1]) > UNIT_OFFSET_MAX)
      say(`The unit offset of the image “${s.name || s.id}” must be two whole numbers from -${UNIT_OFFSET_MAX} to ${UNIT_OFFSET_MAX}. The game will not load until it is fixed.`)
  }

  // The portrait size.
  if (g.portraitSize !== undefined) {
    const p = g.portraitSize as unknown
    const ok = (v: unknown) => isWhole(v) && v >= PORTRAIT_MIN && v <= PORTRAIT_MAX
    if (!(ok(p) || (Array.isArray(p) && p.length === 2 && ok(p[0]) && ok(p[1]))))
      say(`The portrait size must be a whole number, or a width and a height, each from ${PORTRAIT_MIN} to ${PORTRAIT_MAX}. The game will not load until it is fixed.`)
  }

  // Pictures of characters (base and per style) name characters the game has; poses and walking frames of every unit picture.
  const known = new Set((doc.characters ?? []).map((c) => c.id))
  const tables: Array<{ where: string; style?: string; units?: Record<string, Look | null>; characters?: Record<string, Look | null>; other?: Array<Record<string, Look | null> | undefined> }> = [
    { where: '', units: g.units, characters: g.characters, other: [g.terrain, g.structures] },
    ...Object.entries(g.styles ?? {}).map(([id, st]) => ({ where: ` in the look “${doc.game?.styles?.find((x) => x.id === id)?.name || id}”`, style: id, units: st.units, characters: st.characters, other: [st.terrain, st.structures] })),
  ]
  for (const t of tables) {
    for (const [id, look] of Object.entries(t.characters ?? {})) {
      if (!known.has(id)) say(`The art names the character “${id}”${t.where}, which this game does not have. The game will not load until it is taken away.`)
      if (look) frameChecks(doc, look, `The character “${id}”${t.where}`, dims, say)
    }
    for (const [id, look] of Object.entries(t.units ?? {}))
      if (look) frameChecks(doc, look, `The unit “${doc.units?.find((u) => u.id === id)?.name ?? id}”${t.where}`, dims, say)
    // Only a unit picture can have poses (terrain and structures are refused).
    for (const table of t.other ?? [])
      for (const [id, look] of Object.entries(table ?? {}))
        if (look && (['facing', 'attack', 'hurt', 'ko'] as const).some((k) => look[k] !== undefined))
          say(`The picture of “${id}”${t.where} has poses (facing, attack, hurt or defeated), but only a unit picture can. The game will not load until they are taken away.`)
  }
  return out
}

/** The poses and walking frames of one unit picture and of its per-side pictures. */
function frameChecks(doc: ProjectDoc, look: Look, who: string, dims: Dims | undefined, say: (t: string) => void): void {
  const check = (l: Look, w: string) => {
    for (const key of FRAME_KEYS) {
      if (!l[key]) continue
      if (typeof l[key] !== 'object') { say(`${w}: the ${POSE_WORD[key]} pictures must be an object like the walking frames.`); continue }
      for (const p of walkProblems(doc, l, dims, key)) say(`${w}: ${p}`)
    }
  }
  check(look, who)
  for (const [side, l] of Object.entries(look.sides ?? {})) check(l, `${who}, side “${side}”`)
}
