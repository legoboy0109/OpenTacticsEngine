// Fuel, supplying and ammunition on unit types and weapons (docs/ECONOMY.md "Repair and supply"). Pure edits.
import { updateUnit } from './units'
import { updateWeapon } from './weapons'
import type { ProjectDoc } from './types'

export const FUEL_MAX = 999
export const FUEL_START = 60
export const FUEL_BURN_START = 5
export const AMMO_MAX = 99
export const AMMO_START = 6

/** The tank: a number switches fuel on, undefined removes it together with the burn per turn (which needs a tank). */
export function setFuel(doc: ProjectDoc, id: string, fuel: number | undefined): ProjectDoc {
  if (fuel === undefined) return updateUnit(doc, id, { fuel: undefined, fuelPerTurn: undefined })
  return updateUnit(doc, id, { fuel: Math.min(FUEL_MAX, Math.max(1, Math.round(fuel))) })
}

/** Fuel lost at each turn start; undefined removes it. */
export function setFuelPerTurn(doc: ProjectDoc, id: string, burn: number | undefined): ProjectDoc {
  return updateUnit(doc, id, { fuelPerTurn: burn === undefined ? undefined : Math.min(FUEL_MAX, Math.max(1, Math.round(burn))) })
}

/** A burn that starts below the tank, so switching it on is valid at once (null = the tank is too small for any). */
export const startingBurn = (fuel: number): number | null => (fuel < 2 ? null : Math.min(FUEL_BURN_START, fuel - 1))

export const setSupplies = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateUnit(doc, id, { supplies: on ? true : undefined })

export const setAmmo = (doc: ProjectDoc, id: string, ammo: number | undefined): ProjectDoc =>
  updateWeapon(doc, id, { ammo: ammo === undefined ? undefined : Math.min(AMMO_MAX, Math.max(1, Math.round(ammo))) })

/** The item that carries this weapon, if any: the loader refuses `ammo` on it (its item's uses already limit it). */
export const carriedAsItem = (doc: ProjectDoc, weaponId: string): string | null =>
  doc.items?.find((i) => i.kind === 'weapon' && i.weapon === weaponId)?.name ?? null
