// Extra sight for some unit types only (docs/ECONOMY.md "Fog of war") through the real editor: the "Who gets the extra sight"
// choice on Terrain and Structures, the unit types and labels picker (the same as a structure's Repairs), the checks, the map
// editor's "Show what a side sees" aid, the files, and the lists following a deleted unit type (one undo step).
import type { Page } from '@playwright/test'
import { editorUrl, expect, readDisk, saved, test } from './fixtures'
import { canvas, openMap, tab } from './mapkit'
import { shot } from './storykit'

const open = async (page: Page, id: string, section: string, item?: string) => {
  await page.goto(editorUrl(id, section, item))
  await expect(page.getByRole('heading', { level: 1 }).first()).toBeVisible()
}

const fogBox = (page: Page) => page.getByRole('checkbox', { name: /^Fog of war/ })
const aidBox = (page: Page) => page.getByRole('checkbox', { name: /^Show what a side sees/ })
const undo = (page: Page) => page.getByRole('button', { name: 'Undo' })
const picker = (page: Page) => page.getByRole('group', { name: 'Unit types and labels that get it' })
const terrainOf = (id: string, kind: string) => (readDisk(id, 'data/terrain.json').terrain as Array<Record<string, any>>).find((t) => t.id === kind)!
const structureOf = (id: string, kind: string) => (readDisk(id, 'data/structures.json').structures as Array<Record<string, any>>).find((s) => s.id === kind)!

test('@smoke extra sight for some unit types only: pick them, the checks, the map aid, the files, and the lists follow a deleted unit type', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await game.create('economy-tactics', 'Fog Limited')
  const checks = page.getByTestId('economy-checks').filter({ visible: true })
  const sightOf = async () => ((await canvas(page).getAttribute('data-fog')) ?? '').split('').filter((c) => c === '1').length

  // Fog on for the map, so the aid can be used.
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await fogBox(page).check()
  await saved(page)

  // --- Terrain "Plain": 3 extra squares of sight, "Only for these". Nothing ticked yet: said on the page and flagged by the checks.
  await open(page, id, 'terrain', 'plain')
  await page.getByLabel('Extra sight for a unit standing here').fill('3')
  await page.getByLabel('Who gets the extra sight').selectOption({ label: 'Only for these' })
  await expect(page.getByTestId('sight-nobody')).toContainText('Nothing is ticked')
  await expect(checks).toContainText('none is ticked')
  await saved(page)
  expect((await game.problems(id)).length).toBeGreaterThan(0) // the real loader refuses an empty list too
  await picker(page).getByLabel('Artillery').check() // no artillery on the map: nobody there gets the bonus
  await expect(page.getByTestId('sight-nobody')).toHaveCount(0)
  await saved(page)
  expect(terrainOf(id, 'plain').visionBonus).toEqual({ amount: 3, units: ['artillery'] })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'fog-limited-sight')

  // --- The map aid applies the limit: blue has no artillery, so the bonus changes nothing; ticking Infantry widens what blue sees.
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await aidBox(page).check()
  await expect(page.getByLabel('Side', { exact: true })).toHaveValue('blue')
  await expect.poll(sightOf).toBeGreaterThan(0)
  const narrow = await sightOf()
  await open(page, id, 'terrain', 'plain')
  await picker(page).getByLabel('Infantry').check()
  await saved(page)
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await aidBox(page).check() // the aid is not remembered: a drawing aid, nothing saved
  await expect.poll(sightOf).toBeGreaterThan(narrow)

  // --- "For all units" writes the plain number again; "Only for these" again starts with an empty list (and the number stays).
  await open(page, id, 'terrain', 'plain')
  await page.getByLabel('Who gets the extra sight').selectOption({ label: 'For all units' })
  await saved(page)
  expect(terrainOf(id, 'plain').visionBonus).toBe(3)
  await page.getByLabel('Who gets the extra sight').selectOption({ label: 'Only for these' })
  await picker(page).getByLabel('Tank').check()
  await saved(page)
  expect(terrainOf(id, 'plain').visionBonus).toEqual({ amount: 3, units: ['tank'] })

  // --- A structure kind works the same way.
  await open(page, id, 'structures', 'city')
  await page.getByLabel('Extra sight for a unit standing here').fill('2')
  await page.getByLabel('Who gets the extra sight').selectOption({ label: 'Only for these' })
  await picker(page).getByLabel('Tank').check()
  await saved(page)
  expect(structureOf(id, 'city').visionBonus).toEqual({ amount: 2, units: ['tank'] })
  expect(await game.problems(id)).toEqual([])

  // --- Delete the Tank unit type: both lists empty, so both bonuses go with it; one undo brings everything back.
  const before = { terrain: JSON.stringify(readDisk(id, 'data/terrain.json')), structures: JSON.stringify(readDisk(id, 'data/structures.json')) }
  await open(page, id, 'units', 'tank')
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('extra sight')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(terrainOf(id, 'plain')).not.toHaveProperty('visionBonus')
  expect(structureOf(id, 'city')).not.toHaveProperty('visionBonus')
  expect(await game.problems(id)).toEqual([])
  await undo(page).click()
  await saved(page)
  expect(JSON.stringify(readDisk(id, 'data/terrain.json'))).toBe(before.terrain)
  expect(JSON.stringify(readDisk(id, 'data/structures.json'))).toBe(before.structures)
  expect(await game.problems(id)).toEqual([])
})
