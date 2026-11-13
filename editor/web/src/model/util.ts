// Small helpers shared by the edit modules. All of them are pure.

/** Shallow copy of `obj` with `patch` applied; `undefined` values delete the key. */
export function patchObj<T extends object>(obj: T, patch: { [K in keyof T]?: T[K] | undefined } | Record<string, unknown>): T {
  const out: Record<string, unknown> = { ...(obj as Record<string, unknown>) }
  for (const [k, v] of Object.entries(patch)) {
    if (v === undefined) delete out[k]
    else out[k] = v
  }
  return out as T
}

/** Copy of the list with the item whose id matches replaced by fn(item). */
export function updateById<T extends { id: string }>(list: T[], id: string, fn: (t: T) => T): T[] {
  return list.map((t) => (t.id === id ? fn(t) : t))
}

export function moveItem<T>(list: T[], from: number, to: number): T[] {
  if (from === to || from < 0 || to < 0 || from >= list.length || to >= list.length) return list
  const out = list.slice()
  const [it] = out.splice(from, 1)
  out.splice(to, 0, it)
  return out
}

/** Copy of a number map without `key`; undefined when it ends up empty. */
export function dropKey(m: Record<string, number> | undefined, key: string): Record<string, number> | undefined {
  if (!m || !(key in m)) return m
  const rest = { ...m }
  delete rest[key]
  return Object.keys(rest).length ? rest : undefined
}

/** Copy of the list without `v`; unchanged (same reference) when absent. */
export function without<T>(list: T[] | undefined, v: T): T[] | undefined {
  if (!list || !list.includes(v)) return list
  const out = list.filter((x) => x !== v)
  return out.length ? out : undefined
}

/** Set a key in an optional number map; undefined value removes it. */
export function setNum(m: Record<string, number> | undefined, key: string, value: number | undefined): Record<string, number> | undefined {
  if (value === undefined) return dropKey(m, key)
  return { ...(m ?? {}), [key]: value }
}
