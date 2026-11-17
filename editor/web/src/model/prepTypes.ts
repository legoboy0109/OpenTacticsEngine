// A map's preparation screen as the map file holds it (docs/EVENTS.md "Preparation").
export interface PrepSlot { x: number; y: number }
/** A character id, or `{ character, slot }` to fix the square too (`slot` is the slot's number, from 1). */
export type PrepRequired = string | { character: string; slot?: number }
export interface MapPrep {
  /** Deployment squares, numbered from 1 in this order. */
  slots: PrepSlot[]
  /** Most characters deployed (1 .. slots); missing = every slot. */
  limit?: number
  required?: PrepRequired[]
  forbidden?: string[]
  /** Placed units that stay where the map puts them and are not part of the choice. */
  fixed?: string[]
  shop?: { sells: string[] }
  /** The side the deployed characters join (missing = the first human side). */
  side?: string
}
