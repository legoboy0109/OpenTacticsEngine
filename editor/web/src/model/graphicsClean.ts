// Art follows its owners: deleting a terrain, unit or visual style removes its pictures in the same edit; duplicating copies them.
import { clone } from './doc'
import { patchObj } from './util'
import type { Graphics, Look, LookKind, ProjectDoc, StyleArt } from './types'

const hasKey = (m: object | undefined, k: string) => !!m && Object.prototype.hasOwnProperty.call(m, k)
const omit = <T,>(m: Record<string, T> | undefined, k: string): Record<string, T> | undefined => {
  if (!m || !hasKey(m, k)) return m
  const rest = { ...m }
  delete rest[k]
  return Object.keys(rest).length ? rest : undefined
}

function mapStyles(g: Graphics, fn: (s: StyleArt) => StyleArt): Graphics['styles'] {
  if (!g.styles) return g.styles
  const out: Record<string, StyleArt> = {}
  for (const [k, s] of Object.entries(g.styles)) {
    const next = fn(s)
    if (Object.keys(next).length) out[k] = next
  }
  return Object.keys(out).length ? out : undefined
}

/** Removes a terrain's or unit's pictures (base and every look's override). A removed terrain also leaves every other
 *  terrain's "also joins with" list. */
export function dropArtOf(doc: ProjectDoc, kind: LookKind, id: string): ProjectDoc {
  const g = doc.graphics
  if (!g) return doc
  const touched = hasKey(g[kind], id) || Object.values(g.styles ?? {}).some((s) => hasKey(s[kind], id))
  const joins = kind === 'terrain' && joinsWith(g, id)
  if (!touched && !joins) return doc
  let next: Graphics = patchObj(g, { [kind]: omit(g[kind] as Record<string, Look> | undefined, id) })
  next = patchObj(next, { styles: mapStyles(next, (s) => patchObj(s, { [kind]: omit(s[kind] as Record<string, Look | null> | undefined, id) })) })
  if (joins) next = stripJoinWith(next, id)
  return { ...doc, graphics: next }
}

const joinsWith = (g: Graphics, terrainId: string): boolean => {
  const has = (l: Look | null | undefined) => !!l?.joinWith?.includes(terrainId)
  return Object.values(g.terrain ?? {}).some(has) || Object.values(g.styles ?? {}).some((s) => Object.values(s.terrain ?? {}).some(has))
}

function stripJoinWith(g: Graphics, terrainId: string): Graphics {
  const fix = (l: Look): Look => {
    if (!l.joinWith?.includes(terrainId)) return l
    const rest = l.joinWith.filter((x) => x !== terrainId)
    return patchObj(l, { joinWith: rest.length ? rest : undefined })
  }
  const fixMap = <T extends Look | null>(m: Record<string, T> | undefined): Record<string, T> | undefined =>
    m && Object.fromEntries(Object.entries(m).map(([k, v]) => [k, (v === null ? v : fix(v)) as T]))
  return patchObj(g, {
    terrain: fixMap(g.terrain),
    styles: mapStyles(g, (s) => patchObj(s, { terrain: fixMap(s.terrain) })),
  })
}

/** Removes a visual style's picture overrides (the style itself is removed by the Game screen's edit). */
export function dropStyleArt(doc: ProjectDoc, styleId: string): ProjectDoc {
  const g = doc.graphics
  if (!g || !hasKey(g.styles, styleId)) return doc
  return { ...doc, graphics: patchObj(g, { styles: omit(g.styles, styleId) }) }
}

/** The new thing gets copies of the old one's pictures (base and every look's override). */
export function copyArtOf(doc: ProjectDoc, kind: LookKind, fromId: string, toId: string): ProjectDoc {
  const g = doc.graphics
  if (!g) return doc
  const put = <T,>(m: Record<string, T> | undefined): Record<string, T> | undefined =>
    m && hasKey(m, fromId) ? { ...m, [toId]: clone(m[fromId]) } : m
  const next = patchObj(g, {
    [kind]: put(g[kind] as Record<string, Look> | undefined),
    styles: mapStyles(g, (s) => patchObj(s, { [kind]: put(s[kind] as Record<string, Look | null> | undefined) })),
  })
  return JSON.stringify(next) === JSON.stringify(g) ? doc : { ...doc, graphics: next }
}
