// A "cut-scene map" (game.json maps entry `"cutScene": true`, docs/EVENTS.md "Showing another map"): a map only story scenes show with a
// `showMap` command. It is never played, so the Play picker, the campaign's chapter and free-battle pickers leave it out, and the loader
// refuses events, objectives and a preparation on it (engine/core/data/map_loader.cpp).
import { withGame } from './doc'
import type { MapRef, ProjectDoc } from './types'

export const isCutScene = (doc: ProjectDoc, mapId: string): boolean => doc.game?.maps.some((m) => m.id === mapId && m.cutScene === true) ?? false

/** The maps a person can play or give a chapter: every map of the game except the cut-scene ones. */
export const playableMaps = <T extends Pick<MapRef, 'cutScene'>>(maps: T[]): T[] => maps.filter((m) => m.cutScene !== true)

/** The flag is written only when on (false is the default, so the key goes away). */
export function setCutScene(doc: ProjectDoc, mapId: string, on: boolean): ProjectDoc {
  return withGame(doc, (g) => ({
    ...g,
    maps: g.maps.map((m) => {
      if (m.id !== mapId) return m
      const { cutScene: _was, ...rest } = m
      return on ? { ...rest, cutScene: true } : rest
    }),
  }))
}

/** What the loader refuses on a cut-scene map: its own events, objectives and preparation (its scenes belong in the event that shows it). */
export function cutSceneChecks(doc: ProjectDoc, mapId: string): string[] {
  const map = doc.maps[mapId]
  if (!map || !isCutScene(doc, mapId)) return []
  const out: string[] = []
  const say = (what: string) => out.push(`This map is only for story scenes, so it cannot have ${what}: it is never played. Put the scene’s commands in the event of the map that shows it, or untick “Only for story scenes”.`)
  if (map.events !== undefined) say('events')
  if (map.objectives !== undefined) say('objectives')
  if (map.prep !== undefined) say('a preparation screen')
  return out
}
