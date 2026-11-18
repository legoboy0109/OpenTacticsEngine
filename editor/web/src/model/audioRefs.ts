// Everything in a game that NAMES a music track or a sound effect, and what to do when one is renamed or deleted. One
// traversal, like storyWalk.ts / campaignWalk.ts, serves four jobs:
//   - rename (the callback returns a new id: every place follows, in the same undo step),
//   - clean up after a delete (DROP: the smallest thing that cannot work any more is removed: a slot of the standard set,
//     one event of a weapon's sounds, a map's music key, a scene's music, a story command),
//   - "used by" (`dropped` gets a plain sentence for each),
//   - checks (the callback gets `where` and says whether the name exists).
// The places: audio.json's `use`, the `sounds` blocks of weapons, units and items, the music keys of maps (here:
// `walkAudioSettings`); the story commands in map events (storyWalk.ts) and the campaign's scenes (campaignWalk.ts), which
// know the kinds 'music' and 'sound' too. "" (silence on purpose) is never a name: it is kept.
// The document store runs `reconcileAudio` after EVERY edit, so no delete path can forget it.
import { MAP_MUSIC_KEYS, MAP_MUSIC_LABEL, MUSIC_SLOT_LABEL, MUSIC_SLOTS, OVERRIDE_KEY_LABEL, OVERRIDE_KEYS, SOUND_KEY_LABEL, STANDARD_SOUND_KEYS, UNIT_KEY_LABEL, type OverrideOwner, type StandardSoundKey } from './audioLabels'
import { walkCampaign } from './campaignWalk'
import { scrubDoc, storyUsages } from './storyRefs'
import { DROP, walkStory, type Ref, type Walk } from './storyWalk'
import type { Usage } from './references'
import type { Audio, AudioUse } from './audioTypes'
import type { MapFile, ProjectDoc } from './types'

export type AudioKind = 'music' | 'sound'

/** `block` with every id mapped through the walk; the same object when nothing changed; undefined when it ended up empty. */
function walkBlock(block: Record<string, string> | undefined, keys: readonly string[], kind: AudioKind, w: Walk, where: string, label: (k: string) => string, owner: string): Record<string, string> | undefined {
  if (!block || typeof block !== 'object') return block
  let out = block
  for (const k of keys) {
    const v = block[k]
    if (typeof v !== 'string' || v === '') continue
    const r = w.ref({ kind, id: v }, `${where}, “${label(k)}”`)
    if (r === DROP) {
      const { [k]: _gone, ...rest } = out
      out = rest
      w.dropped?.({ what: 'setting', text: `${owner}: “${label(k)}” goes back to the game’s sound`, where })
    } else if (r !== v) out = { ...out, [k]: r }
  }
  if (out === block) return block
  return Object.keys(out).length ? out : undefined
}

function walkUse(a: Audio, w: Walk): Audio {
  const use = a.use
  if (!use || typeof use !== 'object') return a
  let next: AudioUse = use
  for (const slot of MUSIC_SLOTS) {
    const v = use[slot]
    if (typeof v !== 'string' || v === '') continue
    const where = `the standard set, “${MUSIC_SLOT_LABEL[slot]}”`
    const r = w.ref({ kind: 'music', id: v }, where)
    if (r === DROP) {
      const { [slot]: _gone, ...rest } = next
      next = rest
      w.dropped?.({ what: 'setting', text: `Sound, standard set: “${MUSIC_SLOT_LABEL[slot]}” plays no music any more`, where })
    } else if (r !== v) next = { ...next, [slot]: r }
  }
  // Music by situation: a side's phase music and the few-enemies track.
  if (use.phaseMusic && typeof use.phaseMusic === 'object') {
    let pm = use.phaseMusic
    for (const [side, v] of Object.entries(use.phaseMusic)) {
      if (typeof v !== 'string' || v === '') continue
      const where = `the standard set, music on “${side}”’s turn`
      const r = w.ref({ kind: 'music', id: v }, where)
      if (r === DROP) {
        const { [side]: _g, ...rest } = pm
        pm = rest
        w.dropped?.({ what: 'setting', text: `Sound, standard set: the turn of “${side}” plays no music of its own any more`, where })
      } else if (r !== v) pm = { ...pm, [side]: r }
    }
    if (pm !== use.phaseMusic) {
      if (Object.keys(pm).length) next = { ...next, phaseMusic: pm }
      else { const { phaseMusic: _p, ...rest } = next; next = rest }
    }
  }
  if (use.fewEnemies && typeof use.fewEnemies === 'object' && typeof use.fewEnemies.music === 'string' && use.fewEnemies.music !== '') {
    const where = 'the standard set, few enemies left'
    const r = w.ref({ kind: 'music', id: use.fewEnemies.music }, where)
    if (r === DROP) {
      const { fewEnemies: _f, ...rest } = next
      next = rest
      w.dropped?.({ what: 'setting', text: 'Sound, standard set: no music plays when few enemies are left any more', where })
    } else if (r !== use.fewEnemies.music) next = { ...next, fewEnemies: { ...use.fewEnemies, music: r } }
  }
  if (Array.isArray(use.promotionSounds)) {  // the sounds played at times after a promotion starts
    let kept = use.promotionSounds
    let changed = false
    const out = []
    for (const [i, e] of kept.entries()) {
      if (!e || typeof e.sound !== 'string' || e.sound === '') { out.push(e); continue }
      const where = `the standard set, promotion sound ${i + 1}`
      const rr = w.ref({ kind: 'sound', id: e.sound }, where)
      if (rr === DROP) { changed = true; w.dropped?.({ what: 'setting', text: `Sound, standard set: promotion sound ${i + 1} (at ${e.atMs} ms) goes`, where }) }
      else if (rr !== e.sound) { changed = true; out.push({ ...e, sound: rr }) }
      else out.push(e)
    }
    if (changed) {
      kept = out
      if (kept.length) next = { ...next, promotionSounds: kept }
      else { const { promotionSounds: _p, ...rest } = next; next = rest }
    }
  }
  const sounds = walkBlock(use.sounds, STANDARD_SOUND_KEYS, 'sound', w, 'the standard set', (k) => SOUND_KEY_LABEL[k as StandardSoundKey], 'Sound, standard set')
  if (sounds !== use.sounds) {
    if (sounds) next = { ...next, sounds }  // the key stays where it was
    else { const { sounds: _s, ...rest } = next; next = rest }
  }
  if (next === use) return a
  if (Object.keys(next).length === 0) { const { use: _u, ...rest } = a; return rest }
  return { ...a, use: next }
}

function walkOwners(doc: ProjectDoc, w: Walk): ProjectDoc {
  let out = doc
  const names: Record<OverrideOwner, [string, (id: string) => string]> = {
    weapons: ['Weapon', (id) => doc.weapons?.find((x) => x.id === id)?.name ?? id],
    units: ['Unit', (id) => doc.units?.find((x) => x.id === id)?.name ?? id],
    items: ['Item', (id) => doc.items?.find((x) => x.id === id)?.name ?? id],
  }
  for (const owner of ['weapons', 'units', 'items'] as const) {
    const list = doc[owner] as Array<{ id: string; sounds?: Record<string, string> }> | null
    if (!list) continue
    const [noun, nameOf] = names[owner]
    let changed = false
    const next = list.map((e) => {
      if (!e.sounds) return e
      const label = owner === 'units' ? (k: string) => UNIT_KEY_LABEL[k] ?? k : (k: string) => OVERRIDE_KEY_LABEL[k] ?? k
      const who = `${noun} “${nameOf(e.id)}”`
      const block = walkBlock(e.sounds, OVERRIDE_KEYS[owner], 'sound', w, who, label, who)
      if (block === e.sounds) return e
      changed = true
      if (block) return { ...e, sounds: block }
      const { sounds: _gone, ...rest } = e
      return rest as typeof e
    })
    if (changed) out = { ...out, [owner]: next }
  }
  return out
}

/** The "music" of a map's preparation shop (one) or shops (a list): follows a rename, goes with a deleted track. */
function walkShopMusic(map: MapFile, mapName: string, w: Walk): MapFile {
  const prep = map.prep
  if (!prep) return map
  let next = prep
  const follow = <T extends { music?: string }>(s: T, label: string): T => {
    if (typeof s.music !== 'string' || s.music === '') return s
    const where = `map “${mapName}”, ${label}`
    const r = w.ref({ kind: 'music', id: s.music }, where)
    if (r === DROP) {
      const { music: _g, ...rest } = s
      w.dropped?.({ what: 'setting', text: `Map “${mapName}”: ${label} plays no music of its own any more`, where })
      return rest as T
    }
    return r === s.music ? s : { ...s, music: r }
  }
  if (prep.shop) {
    const s = follow(prep.shop, 'the preparation shop')
    if (s !== prep.shop) next = { ...next, shop: s }
  }
  if (Array.isArray(prep.shops)) {
    const list = prep.shops.map((s) => follow(s, `the preparation shop “${s.name ?? s.id}”`))
    if (list.some((s, i) => s !== prep.shops![i])) next = { ...next, shops: list }
  }
  return next === prep ? map : { ...map, prep: next }
}

function walkMapMusic(doc: ProjectDoc, w: Walk): ProjectDoc {
  let maps = doc.maps
  for (const [id, m] of Object.entries(doc.maps)) {
    if (!m) continue
    let next: MapFile = m
    for (const key of MAP_MUSIC_KEYS) {
      const v = m[key]
      if (typeof v !== 'string' || v === '') continue
      const mapName = doc.game?.maps.find((x) => x.id === id)?.name ?? id
      const where = `map “${mapName}”, “${MAP_MUSIC_LABEL[key]}”`
      const r = w.ref({ kind: 'music', id: v }, where)
      if (r === DROP) {
        const { [key]: _gone, ...rest } = next
        next = rest as MapFile
        w.dropped?.({ what: 'setting', text: `Map “${mapName}”: “${MAP_MUSIC_LABEL[key]}” goes back to the game’s music`, where })
      } else if (r !== v) next = { ...next, [key]: r }
    }
    const mapName = doc.game?.maps.find((x) => x.id === id)?.name ?? id
    if (m.phaseMusic && typeof m.phaseMusic === 'object') {
      let pm = m.phaseMusic
      for (const [side, v] of Object.entries(m.phaseMusic)) {
        if (typeof v !== 'string' || v === '') continue
        const where = `map “${mapName}”, music on “${side}”’s turn`
        const r = w.ref({ kind: 'music', id: v }, where)
        if (r === DROP) {
          const { [side]: _g, ...rest } = pm
          pm = rest
          w.dropped?.({ what: 'setting', text: `Map “${mapName}”: the turn of “${side}” goes back to the game’s music`, where })
        } else if (r !== v) pm = { ...pm, [side]: r }
      }
      if (pm !== m.phaseMusic) next = (Object.keys(pm).length ? { ...next, phaseMusic: pm } : (({ phaseMusic: _p, ...rest }) => rest)(next)) as MapFile
    }
    if (m.fewEnemies && typeof m.fewEnemies === 'object' && typeof m.fewEnemies.music === 'string' && m.fewEnemies.music !== '') {
      const where = `map “${mapName}”, few enemies left`
      const r = w.ref({ kind: 'music', id: m.fewEnemies.music }, where)
      if (r === DROP) {
        const { fewEnemies: _f, ...rest } = next
        next = rest as MapFile
        w.dropped?.({ what: 'setting', text: `Map “${mapName}”: few enemies left goes back to the game’s music`, where })
      } else if (r !== m.fewEnemies.music) next = { ...next, fewEnemies: { ...m.fewEnemies, music: r } }
    }
    next = walkShopMusic(next, mapName, w)  // the music of the preparation shops
    if (next !== m) maps = { ...maps, [id]: next }
  }
  return maps === doc.maps ? doc : { ...doc, maps }
}

/** Movement types' step sounds and battle animations' frame sounds. */
function walkStepsAndFrames(doc: ProjectDoc, w: Walk): ProjectDoc {
  let out = doc
  const game = out.game
  if (game) {
    let changed = false
    const moveTypes = game.moveTypes.map((m) => {
      const s = m.sounds as Record<string, string> | undefined
      if (!s || typeof s !== 'object') return m
      const who = `Movement type “${m.name || m.id}”`
      const block = walkBlock(s, ['step'], 'sound', w, who, () => 'Steps', who)
      if (block === s) return m
      changed = true
      // (a stepInterval that was kept next to the step sound stays; walkBlock only knows the sound keys)
      const interval = s.stepInterval
      const keep = block ?? (interval !== undefined ? { stepInterval: interval } : undefined)
      if (block && interval !== undefined) return { ...m, sounds: { ...block, stepInterval: interval } }
      if (keep) return { ...m, sounds: keep }
      const { sounds: _g, ...rest } = m
      return rest as typeof m
    })
    if (changed) out = { ...out, game: { ...game, moveTypes } }
  }
  const battle = out.graphics?.battle
  if (battle?.sets && out.graphics) {
    let sets = battle.sets
    for (const [setId, set] of Object.entries(battle.sets)) {
      let anims = set.animations
      for (const [key, a] of Object.entries(set.animations)) {
        if (!a.sounds || typeof a.sounds !== 'object') continue
        let block = a.sounds
        for (const [frame, id] of Object.entries(a.sounds)) {
          if (typeof id !== 'string' || id === '') continue
          const where = `battle set “${setId}”, “${key}”, frame ${frame}`
          const r = w.ref({ kind: 'sound', id }, where)
          if (r === DROP) {
            const { [frame]: _g, ...rest } = block
            block = rest
            w.dropped?.({ what: 'setting', text: `Art: battle set “${setId}”, “${key}”: the sound on frame ${frame} goes`, where })
          } else if (r !== id) block = { ...block, [frame]: r }
        }
        if (block !== a.sounds) {
          const { sounds: _s, ...restA } = a
          anims = { ...anims, [key]: Object.keys(block).length ? { ...restA, sounds: block } : restA }
        }
      }
      if (anims !== set.animations) sets = { ...sets, [setId]: { ...set, animations: anims } }
    }
    if (sets !== battle.sets) out = { ...out, graphics: { ...out.graphics, battle: { ...battle, sets } } }
  }
  return out
}

/** The places outside story commands and scenes: audio.json's `use`, the `sounds` of weapons / units / items, maps' music. */
export function walkAudioSettings(doc: ProjectDoc, w: Walk): ProjectDoc {
  let out = doc
  if (out.audio) { const a = walkUse(out.audio, w); if (a !== out.audio) out = { ...out, audio: a } }
  out = walkOwners(out, w)
  out = walkStepsAndFrames(out, w)
  return walkMapMusic(out, w)
}

/** Settings, then story commands in every map, then the campaign's scenes (their music and their commands). */
export function walkAudioAll(doc: ProjectDoc, w: Walk): ProjectDoc {
  let out = walkAudioSettings(doc, w)
  let maps = out.maps
  for (const [id, m] of Object.entries(out.maps)) {
    if (!m || (!m.events && !m.objectives)) continue
    const next = walkStory(m, w)
    if (next !== m) maps = { ...maps, [id]: next }
  }
  if (maps !== out.maps) out = { ...out, maps }
  if (out.campaign) { const c = walkCampaign(out.campaign, w); if (c !== out.campaign) out = { ...out, campaign: c } }
  return out
}

const idsOf = (o: Record<string, unknown> | undefined): Set<string> => new Set(Object.keys(o ?? {}))

/** Tracks and sounds that `before` had and `after` no longer has. Removing all audio takes all of them. */
export function goneAudio(before: ProjectDoc, after: ProjectDoc): Ref[] {
  if (!before.audio || before.audio === after.audio) return []
  const out: Ref[] = []
  const lost = (kind: AudioKind, a: Set<string>, b: Set<string>) => { for (const id of a) if (!b.has(id)) out.push({ kind, id }) }
  lost('music', idsOf(before.audio.music), idsOf(after.audio?.music))
  lost('sound', idsOf(before.audio.sounds), idsOf(after.audio?.sounds))
  return out
}

const hit = (gone: Ref[], r: Ref): boolean => gone.some((g) => g.kind === r.kind && g.id === r.id)

/** Takes out of the game every place that names a track or sound that this edit deleted. The same object when nothing was lost.
 *  Only what was there BEFORE and is gone AFTER is touched: a name that was already dangling stays, so the checks can point
 *  at it instead of the editor silently deleting something. */
export function reconcileAudio(before: ProjectDoc, after: ProjectDoc): ProjectDoc {
  if (before === after) return after
  const gone = goneAudio(before, after)
  if (!gone.length) return after
  const drop: Walk = { ref: (r) => (hit(gone, r) ? DROP : r.id) }
  // Settings first, then story commands (the same walk the unit clean-ups use), then the scenes.
  let out = walkAudioSettings(after, drop)
  out = scrubDoc(out, gone)
  if (out.campaign) { const c = walkCampaign(out.campaign, drop); if (c !== out.campaign) out = { ...out, campaign: c } }
  return out
}

/** Every place that names track/sound `from` now names `to`. (Renaming the key itself is audio.ts's `renameAudioId`.) */
export function followAudioRename(doc: ProjectDoc, kind: AudioKind, from: string, to: string): ProjectDoc {
  if (from === to) return doc
  return walkAudioAll(doc, { ref: (r) => (r.kind === kind && r.id === from ? to : r.id) })
}

/** Renames a track or sound: its key in audio.json (the order is kept) and every place that names it. */
export function renameAudioId(doc: ProjectDoc, kind: AudioKind, from: string, to: string): ProjectDoc {
  const a = doc.audio
  const section = kind === 'music' ? 'music' : 'sounds'
  const table = a?.[section] as Record<string, unknown> | undefined
  if (!a || !table || !(from in table) || from === to || to in table) return doc
  const renamed = Object.fromEntries(Object.entries(table).map(([k, v]) => [k === from ? to : k, v]))
  return followAudioRename({ ...doc, audio: { ...a, [section]: renamed } as Audio }, kind, from, to)
}

/** Lines for the delete confirmation: what is taken out when this track or sound goes. */
export const audioUsageLines = (doc: ProjectDoc, kind: AudioKind, id: string): Usage[] => usageLinesOf(doc, [{ kind, id }])

/** The same for taking ALL sound out of the game: every place that names any track or sound, each line once. */
export function allAudioUsageLines(doc: ProjectDoc): Usage[] {
  const gone: Ref[] = [
    ...Object.keys(doc.audio?.music ?? {}).map((id): Ref => ({ kind: 'music', id })),
    ...Object.keys(doc.audio?.sounds ?? {}).map((id): Ref => ({ kind: 'sound', id })),
  ]
  const seen = new Set<string>()
  return usageLinesOf(doc, gone).filter((u) => !seen.has(u.text) && seen.add(u.text))
}

function usageLinesOf(doc: ProjectDoc, gone: Ref[]): Usage[] {
  const out: Usage[] = []
  const sectionOf = (what: string): Usage['section'] =>
    what.startsWith('Weapon') ? 'weapons' : what.startsWith('Unit') ? 'units' : what.startsWith('Item') ? 'items' : what.startsWith('Map') ? 'maps' : what.startsWith('Movement') ? 'game' : what.startsWith('Art') ? 'art' : 'sound'
  walkAudioSettings(doc, {
    ref: (r) => (hit(gone, r) ? DROP : r.id),
    dropped: ({ text }) => out.push({ section: sectionOf(text), text }),
  })
  out.push(...storyUsages(doc, gone))
  return out
}
