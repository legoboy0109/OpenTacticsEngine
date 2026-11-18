// A map's preparation screen as the map file holds it (docs/EVENTS.md "Preparation").
export interface PrepSlot { x: number; y: number }
/** A character id, or `{ character, slot }` to fix the square too (`slot` is the slot's number, from 1). */
export type PrepRequired = string | { character: string; slot?: number }
export interface PrepShopEntry { id: string; name?: string; sells: string[]; buys?: boolean; music?: string }
export interface MapPrep {
  /** Deployment squares, numbered from 1 in this order. */
  slots: PrepSlot[]
  /** Most characters deployed (1 .. slots); missing = every slot. */
  limit?: number
  required?: PrepRequired[]
  forbidden?: string[]
  /** Placed units that stay where the map puts them and are not part of the choice. */
  fixed?: string[]
  /** `buys: false` = the shop only sells; missing = it also buys the party's items back (default true). */
  shop?: { sells: string[]; buys?: boolean; music?: string }
  /** Several shops instead of `shop` (never both): each named, with what it sells, whether it buys, and its music. */
  shops?: PrepShopEntry[]
  /** The side the deployed characters join (missing = the first human side). */
  side?: string
}
