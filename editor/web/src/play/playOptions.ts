// What the Play panel asks the embedded player for: the address of the player page, the choice "this map alone" / "from this chapter",
// and the test level (docs/DIFFICULTY.md "Test level", docs/SAVES.md "Test playthroughs"). Pure functions, so the argument building and
// the remembered choices are tested without a browser. The choices are kept in the editor's local settings (browser storage), never in
// the game's files.
import type { ProjectDoc } from '../model/types'

/** Picker value for "the campaign from chapter 1" (no map id can be this: ids have no '*'). */
export const PLAY_CAMPAIGN = '*campaign'

export type PlayMode = 'map' | 'chapter'

/** The test level: off, automatic (the player works the number out), or a number the maker typed. */
export type TestLevel = { kind: 'off' } | { kind: 'auto' } | { kind: 'number'; n: number }
export const TEST_LEVEL_MIN = 1
export const TEST_LEVEL_MAX = 99
export const TEST_OFF: TestLevel = { kind: 'off' }
export const DEFAULT_TEST_NUMBER = 5

/** A number box's text to a whole number in range, or null. */
export function parseLevelNumber(text: string): number | null {
  if (!/^\d{1,3}$/.test(text.trim())) return null
  const n = Number(text)
  return n >= TEST_LEVEL_MIN && n <= TEST_LEVEL_MAX ? n : null
}

/** Whatever storage held, made into a valid choice (anything odd means off). */
export function readTestLevel(raw: unknown): TestLevel {
  if (!raw || typeof raw !== 'object') return TEST_OFF
  const r = raw as { kind?: unknown; n?: unknown }
  if (r.kind === 'auto') return { kind: 'auto' }
  if (r.kind === 'number' && typeof r.n === 'number' && Number.isInteger(r.n) && r.n >= TEST_LEVEL_MIN && r.n <= TEST_LEVEL_MAX) return { kind: 'number', n: r.n }
  return TEST_OFF
}

/** The remembered levels of a project, one per map ("the level per map"): a record of valid choices. */
export function readTestLevels(raw: unknown): Record<string, TestLevel> {
  const out: Record<string, TestLevel> = {}
  if (raw && typeof raw === 'object' && !Array.isArray(raw))
    for (const [map, v] of Object.entries(raw as Record<string, unknown>)) {
      const t = readTestLevel(v)
      if (t.kind !== 'off') out[map] = t
    }
  return out
}

/** The value of the player's `testlevel` argument; undefined when off. */
export const testLevelArg = (t: TestLevel): string | undefined => (t.kind === 'auto' ? 'auto' : t.kind === 'number' ? String(t.n) : undefined)

/** The first chapter of the campaign that plays on this map, or null (no campaign, or the map is not a chapter's). */
export function firstChapterOfMap(doc: ProjectDoc, mapId: string): { id: string; name: string } | null {
  const c = doc.campaign?.chapters.find((ch) => ch.map === mapId)
  return c ? { id: c.id, name: c.name || c.id } : null
}

export type ChapterChoice = { ok: true; chapter: string; name: string } | { ok: false; reason: 'noCampaign' | 'notChapter' | 'story' }

/** Can the map be played "from this chapter"? Not for the story picker (that already starts at chapter 1). */
export function chapterChoice(doc: ProjectDoc, mapId: string): ChapterChoice {
  if (!doc.campaign) return { ok: false, reason: 'noCampaign' }
  if (mapId === PLAY_CAMPAIGN) return { ok: false, reason: 'story' }
  const c = firstChapterOfMap(doc, mapId)
  return c ? { ok: true, chapter: c.id, name: c.name } : { ok: false, reason: 'notChapter' }
}

/** The mode really used: "chapter" only when it was chosen and this map allows it. */
export const effectiveMode = (stored: PlayMode, choice: ChapterChoice): PlayMode => (stored === 'chapter' && choice.ok ? 'chapter' : 'map')

export interface PlayUrlOptions {
  style?: string
  showAll?: boolean
  muted?: boolean
  difficulty?: string
  /** Start a test playthrough at this chapter (a campaign; `map` is then ignored). */
  chapter?: string
  testLevel?: TestLevel
}

/** The address of the embedded player. `showAll` adds `showall=1` (docs/ECONOMY.md "Fog of war"), `muted` adds `noaudio=1`, `difficulty`
 *  adds `difficulty=<id>` (docs/DIFFICULTY.md), `chapter` makes it `campaign=1&chapter=<id>`, a test level adds `testlevel=auto|N`.
 *  With none of them the address is the plain one. */
export function playerUrl(project: string, map: string, o: PlayUrlOptions = {}): string {
  const story = map === PLAY_CAMPAIGN || !!o.chapter
  const target = story ? 'campaign=1' : `map=${encodeURIComponent(map)}`
  const test = o.testLevel ? testLevelArg(o.testLevel) : undefined
  return `/player/embed.html?project=${encodeURIComponent(project)}&${target}` +
    `${o.chapter ? `&chapter=${encodeURIComponent(o.chapter)}` : ''}` +
    `${o.style ? `&style=${encodeURIComponent(o.style)}` : ''}${o.showAll ? '&showall=1' : ''}${o.muted ? '&noaudio=1' : ''}` +
    `${o.difficulty ? `&difficulty=${encodeURIComponent(o.difficulty)}` : ''}${test ? `&testlevel=${test}` : ''}`
}
