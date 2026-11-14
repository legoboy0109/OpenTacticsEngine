// Game screen edits: name/description/author, font, screen size, visual styles.
import { withGame } from './doc'
import { dropStyleArt } from './graphicsClean'
import { makeId } from './ids'
import { moveItem, patchObj, updateById } from './util'
import type { FontSpec, ProjectDoc, Style } from './types'

export function setGameText(doc: ProjectDoc, field: 'name' | 'description' | 'author', value: string): ProjectDoc {
  // Optional text that is emptied leaves the file entirely; the name stays (as empty text if need be).
  return withGame(doc, (g) => patchObj(g, { [field]: field !== 'name' && value === '' ? undefined : value }))
}

export function setView(doc: ProjectDoc, field: 'width' | 'height' | 'tileSize', value: number | undefined): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, view: patchObj(g.view ?? {}, { [field]: value }) }))
}

/** "Default pixel font" (undefined) removes the font key; a built-in sets
 *  its name and resets size/smooth to that font's own defaults. */
export function setFontName(doc: ProjectDoc, name: string | undefined): ProjectDoc {
  return withGame(doc, (g) => {
    if (name === undefined) return patchObj(g, { font: undefined })
    if (g.font?.name === name && g.font.file === undefined) return g
    const keep: FontSpec = { name }
    return { ...g, font: keep }
  })
}

export function setFontField(doc: ProjectDoc, field: 'size' | 'smooth', value: number | boolean | undefined): ProjectDoc {
  return withGame(doc, (g) => (g.font ? { ...g, font: patchObj(g.font, { [field]: value }) } : g))
}

export const newStyleId = (doc: ProjectDoc, name: string): string =>
  makeId(name, (doc.game?.styles ?? []).map((s) => s.id), { fallback: 'look' })

export function addStyle(doc: ProjectDoc, name: string, id: string = newStyleId(doc, name)): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, styles: [...(g.styles ?? []), { id, name }] }))
}

export function updateStyle(doc: ProjectDoc, id: string, patch: Partial<Omit<Style, 'id'>>): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, styles: updateById(g.styles ?? [], id, (s) => patchObj(s, patch)) }))
}

/** Replaces a style by `fn(style)` (kind of view and sizes: model/styleView.ts). */
export function updateStyleWith(doc: ProjectDoc, id: string, fn: (s: Style) => Style): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, styles: updateById(g.styles ?? [], id, fn) }))
}

export function moveStyle(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, styles: moveItem(g.styles ?? [], from, to) }))
}

export function removeStyle(doc: ProjectDoc, id: string): ProjectDoc {
  const removed = withGame(doc, (g) => {
    const rest = (g.styles ?? []).filter((s) => s.id !== id)
    return patchObj(g, { styles: rest.length ? rest : undefined })
  })
  return dropStyleArt(removed, id)
}

/** One colour entry of a style (`terrainColors` or `structureColors`); an emptied map leaves the key out. */
function setStyleColor(doc: ProjectDoc, styleId: string, field: 'terrainColors' | 'structureColors', id: string, color: string | undefined): ProjectDoc {
  const cur = doc.game?.styles?.find((s) => s.id === styleId)?.[field] ?? {}
  const next = { ...cur }
  if (color === undefined) delete next[id]; else next[id] = color
  return updateStyle(doc, styleId, { [field]: Object.keys(next).length ? next : undefined })
}

/** Recolour one terrain in one style; undefined colour goes back to the terrain's own. */
export const setStyleTerrainColor = (doc: ProjectDoc, styleId: string, terrainId: string, color: string | undefined): ProjectDoc =>
  setStyleColor(doc, styleId, 'terrainColors', terrainId, color)

/** Recolour one structure's block in one style; undefined colour goes back to the structure's own. */
export const setStyleStructureColor = (doc: ProjectDoc, styleId: string, structureId: string, color: string | undefined): ProjectDoc =>
  setStyleColor(doc, styleId, 'structureColors', structureId, color)
