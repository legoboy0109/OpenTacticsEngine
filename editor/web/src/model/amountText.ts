// Amounts that are a whole number or a formula (an ability's xp, an inflict's chance): "10" is the number 10, other text stays a formula.
export type Amount = number | string | undefined

/** "10" -> 10 (a whole number), other text stays a formula, '' -> removed. */
export function amountFromText(text: string): Amount {
  const t = text.trim()
  if (t === '') return undefined
  return /^\d+$/.test(t) ? Number(t) : t
}
export const amountText = (v: unknown): string => (typeof v === 'number' || typeof v === 'string' ? String(v) : '')
