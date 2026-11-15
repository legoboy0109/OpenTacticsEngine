// What every story form needs: the document, the map being edited, what exists to point at (named units, sides, items,
// switches...), an `apply` that edits the map as one undo step, and the map's view state (for picking on the map).
import { createContext, useContext, useMemo, type ReactNode } from 'react'
import { editMap } from '../../../../model/mapCtx'
import type { DefCtx } from '../../../../model/storyDefaults'
import { knownTags, namedUnits, teamsOf, type NamedUnit } from '../../../../model/storyIds'
import type { SumCtx } from '../../../../model/storySummary'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useDoc, useEdit } from '../../../../store/context'
import type { MapViewState } from '../viewState'

export interface Story extends SumCtx, DefCtx {
  mapId: string
  vs: MapViewState
  units: NamedUnit[]
  teams: Array<{ id: string; label: string }>
  tags: string[]
  /** Edits this map; `key` makes quick edits (typing) one undo step. */
  apply: (fn: (m: MapFile) => MapFile, key?: string) => void
  /** Edits the whole document (for edits that touch more than the map). */
  applyDoc: (fn: (d: ProjectDoc) => ProjectDoc, key?: string) => void
}

const Ctx = createContext<Story | null>(null)

/** Lets another screen (the Campaign screen's scenes) hand the same forms a Story of its own. */
export const StoryScope = Ctx.Provider

export function StoryProvider({ mapId, vs, children }: { mapId: string; vs: MapViewState; children: ReactNode }) {
  const doc = useDoc()
  const edit = useEdit()
  const map = doc.maps[mapId]!
  const value = useMemo<Story>(() => ({
    doc, map, mapId, vs,
    units: namedUnits(doc, map), teams: teamsOf(map), tags: knownTags(doc),
    apply: (fn, key) => edit((d) => editMap(d, mapId, (m) => fn(m)), key),
    applyDoc: (fn, key) => edit(fn, key),
  }), [doc, map, mapId, vs, edit])
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>
}

export function useStory(): Story {
  const s = useContext(Ctx)
  if (!s) throw new Error('useStory outside StoryProvider')
  return s
}
