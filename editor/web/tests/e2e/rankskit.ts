// Helpers for the weapon ranks and stat caps tests (docs/SRPG.md section 7): a game that already has carried weapons, groups and a ladder (made through the
// API like a second author would, so a test about one screen does not have to click through all the others), and screenshots.
import type { APIRequestContext, Locator, Page } from '@playwright/test'
import { expect } from './fixtures'

export const LADDER = {
  growth: 'use',
  ranks: [{ id: 'e', name: 'E', xp: 0 }, { id: 'd', name: 'D', xp: 30 }, { id: 'c', name: 'C', xp: 70 }, { id: 'b', name: 'B', xp: 120 }, { id: 'a', name: 'A', xp: 180 }, { id: 's', name: 'S', xp: 250 }],
}

/** Resizes the window to the page's height first: a full-page capture would smear the sticky sidebar over the content. */
export const shot = async (page: Page, name: string): Promise<void> => {
  if (!process.env.OTE_SHOT_DIR) return
  const w = page.viewportSize()!.width
  await page.setViewportSize({ width: w, height: 900 })
  const h = await page.evaluate(() => document.documentElement.scrollHeight)
  await page.setViewportSize({ width: w, height: Math.min(h, 3200) })
  await page.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png` })
  await page.setViewportSize({ width: w, height: 900 })
}

/** Weapons carried, sword / lance / bow groups, a ladder E-S (growth by use), caps, and ranks on a class, a character and weapons.
 *  Written through the API; the game must be one made with `srpgContent: true` (it has characters). Returns the loader's problems. */
export async function rankedGame(request: APIRequestContext, id: string): Promise<void> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const game = doc.game
  game.rules.items = { ...(game.rules.items ?? {}), weapons: 'carried', inventorySize: 4 }
  const groups: Record<string, string> = { iron_sword: 'sword', iron_lance: 'lance', iron_axe: 'axe', iron_bow: 'bow' }
  const weapons = doc.weapons.map((w: any) => (groups[w.id] ? { ...w, group: groups[w.id] } : w))
  weapons.find((w: any) => w.id === 'iron_lance').rank = 'c'
  weapons.find((w: any) => w.id === 'iron_lance').rankXp = 2
  weapons.find((w: any) => w.id === 'iron_axe').rank = 'd'
  const units = doc.units.map((u: any) => {
    if (u.id === 'lord') return { ...u, wields: ['sword', 'lance'], ranks: { sword: 'd' }, caps: { str: 24 } }
    if (u.id === 'archer') return { ...u, wields: ['bow'] }
    return u
  })
  const characters = (doc.characters ?? []).map((c: any, i: number) => (i === 0 ? { ...c, ranks: { sword: 'c' }, capAdjust: { str: 3, spd: -2 } } : c))
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game, weapons, units, characters, ranks: LADDER, caps: { caps: { str: 30, spd: 30, hp: 60 } } } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

/** A picture of one part of the page (a card), for screens whose interesting part is far down. */
export const shotEl = async (el: Locator, name: string): Promise<void> => {
  if (process.env.OTE_SHOT_DIR) await el.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png` })
}
