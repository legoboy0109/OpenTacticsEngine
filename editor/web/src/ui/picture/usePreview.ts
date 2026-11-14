// Memoised preview specs for the open game. They only rebuild when pictures, terrain, units, the look or an image changed
// (not on every paint stroke), so a palette full of thumbnails stays cheap while painting.
import { createContext, useContext, useMemo } from 'react'
import { structurePreview, structureThumb, terrainPreview, terrainThumb, unitPreview, unitThumb, type PreviewSpec } from '../../art/preview'
import { PLAIN_STYLE, type StyleView } from '../../art/scene'
import { useArtState } from '../../art/useArt'
import type { MapSide, ProjectDoc } from '../../model/types'
import { useDoc, useStore } from '../../store/context'

export interface LookChoice { id?: string; view: StyleView }
export const NO_LOOK: LookChoice = { view: PLAIN_STYLE }
/** The look the map editor is previewing (so its palettes draw with the same pictures as its canvas). */
export const LookContext = createContext<LookChoice>(NO_LOOK)
export const useLook = (): LookChoice => useContext(LookContext)

function useSpec(build: (doc: ProjectDoc, src: ReturnType<typeof useStore>['art']) => PreviewSpec, extra: unknown[]): PreviewSpec {
  const doc = useDoc()
  const store = useStore()
  const { version } = useArtState()
  // eslint-disable-next-line react-hooks/exhaustive-deps
  return useMemo(() => build(doc, store.art), [doc.graphics, doc.terrain, doc.units, doc.structures, doc.game, store, version, ...extra])
}

export const useTerrainThumb = (id: string, look: LookChoice): PreviewSpec =>
  useSpec((d, a) => terrainThumb(d, a, look.view, look.id, id), [id, look])
export const useUnitThumb = (id: string, side: MapSide | undefined, look: LookChoice): PreviewSpec =>
  useSpec((d, a) => unitThumb(d, a, look.view, look.id, id, side), [id, side?.id, side?.color, look])
/** A bigger sample for the Picture card; `draft` swaps in a look the picker has not saved yet. */
export const useTerrainSample = (id: string, look: LookChoice, doc?: ProjectDoc): PreviewSpec =>
  useSpec((d, a) => terrainPreview(doc ?? d, a, look.view, look.id, id), [id, look, doc])
export function useUnitSample(id: string, look: LookChoice, doc?: ProjectDoc, sideIds?: string[]): PreviewSpec {
  const maps = useDoc().maps // side colours come from the maps
  return useSpec((d, a) => unitPreview(doc ?? d, a, look.view, look.id, id, 48, sideIds), [id, look, doc, sideIds?.join(','), maps])
}
export const useStructureThumb = (id: string, side: MapSide | undefined, look: LookChoice): PreviewSpec =>
  useSpec((d, a) => structureThumb(d, a, look.view, look.id, id, side), [id, side?.id, side?.color, look])
/** A bigger sample for the Picture card (the structure at its real footprint, owned and unowned, or by each of `sideIds`). */
export function useStructureSample(id: string, look: LookChoice, doc?: ProjectDoc, sideIds?: Array<string | undefined>): PreviewSpec {
  const maps = useDoc().maps // side colours come from the maps
  return useSpec((d, a) => structurePreview(doc ?? d, a, look.view, look.id, id, 40, sideIds), [id, look, doc, sideIds?.join(','), maps])
}
