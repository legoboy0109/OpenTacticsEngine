// Ids are made once from a name and never change when the name does.

const COMBINING = /[̀-ͯ]/g

/** "Iron Sword" -> "iron_sword". Accents are folded; names with no usable
 *  ASCII letters ("日本語", "") fall back to `fallback`. */
export function slugify(name: string, opts: { sep?: string; fallback?: string; max?: number } = {}): string {
  const { sep = '_', fallback = 'item', max = 40 } = opts
  const folded = name.normalize('NFKD').replace(COMBINING, '').toLowerCase()
  const edge = new RegExp(`^${sep}+|${sep}+$`, 'g')
  const s = folded.replace(/[^a-z0-9]+/g, sep).replace(edge, '').slice(0, max).replace(edge, '')
  return s || fallback
}

/** base, base_2, base_3 ... first one not in `taken`. */
export function uniqueId(base: string, taken: Iterable<string>, sep = '_'): string {
  const set = new Set(taken)
  if (!set.has(base)) return base
  for (let n = 2; ; n++) {
    const cand = `${base}${sep}${n}`
    if (!set.has(cand)) return cand
  }
}

/** Slug of `name` that does not collide with `existing` or `reserved`. */
export function makeId(name: string, existing: Iterable<string>, opts: { fallback?: string; reserved?: string[] } = {}): string {
  const taken = [...existing, ...(opts.reserved ?? [])]
  return uniqueId(slugify(name, { fallback: opts.fallback }), taken)
}

/** Folder name for a new game: lowercase letters, digits and hyphens (max 40). */
export function projectSlug(name: string): string {
  return slugify(name, { sep: '-', fallback: 'game' })
}

export function uniqueProjectId(name: string, existing: Iterable<string>): string {
  const set = new Set(existing)
  const base = projectSlug(name)
  if (!set.has(base)) return base
  for (let n = 2; ; n++) {
    const suffix = `-${n}`
    const cand = base.slice(0, 40 - suffix.length) + suffix
    if (!set.has(cand)) return cand
  }
}

/** Names the engine puts on every unit table in formulas; a stat with one of
 *  these ids would be silently overwritten there. */
export const RESERVED_STAT_IDS = ['hp', 'maxhp', 'level', 'xp', 'type', 'name', 'tags']
