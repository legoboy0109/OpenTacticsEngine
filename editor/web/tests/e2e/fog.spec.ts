// Fog of war (docs/ECONOMY.md "Fog of war") through the real editor: the map's switch, a unit's sight, a forest that hides,
// a mountain and a structure that add sight, a "team sees a unit" condition, and the "show what a side sees" aid. The proof
// that matters: the exact files the editor wrote, and that the real game loader accepts them (/problems is empty). Then the
// Play panel's "Show everything" box with the real web player, and a phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, requirePlayer, saved, test } from './fixtures'
import { canvas, openMap, tab } from './mapkit'
import { editor, eventsTab, newEvent, shot } from './storykit'

const open = async (page: Page, id: string, section: string, item?: string) => {
  await page.goto(editorUrl(id, section, item))
  await expect(page.getByRole('heading', { level: 1 }).first()).toBeVisible()
}

const fogBox = (page: Page) => page.getByRole('checkbox', { name: /^Fog of war/ })
const aidBox = (page: Page) => page.getByRole('checkbox', { name: /^Show what a side sees/ })
const undo = (page: Page) => page.getByRole('button', { name: 'Undo' })

test('@smoke fog on for a map, a unit’s sight, a forest that hides, a mountain and a structure that add sight, a sees condition: the files are exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Fog')

  // --- Map settings: fog is off, the aid is not offered; switching it on writes the key.
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await expect(fogBox(page)).not.toBeChecked()
  await expect(page.getByTestId('fog-settings')).toContainText('The map itself is always known')
  await expect(aidBox(page)).toHaveCount(0)
  await fogBox(page).check()
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').fog).toBe(true)

  // --- The aid: dims what a side does not see at the start.
  await aidBox(page).check()
  await expect(page.getByLabel('Side', { exact: true })).toHaveValue('blue')
  // 12 x 8 squares: one digit each, 1 = that side's team sees it at the start. Blue's units stand at the left, red's at the right.
  const seen = async (x: number, y: number) => (await canvas(page).getAttribute('data-fog'))![y * 12 + x]
  await expect.poll(() => seen(2, 3)).toBe('1')
  expect(await seen(9, 3)).toBe('0')
  await page.getByLabel('Side', { exact: true }).selectOption({ label: 'Red Army' })
  await expect.poll(() => seen(9, 3)).toBe('1')
  expect(await seen(2, 3)).toBe('0')
  await shot(page, 'fog-map-settings')
  await aidBox(page).uncheck()
  await expect(canvas(page)).toHaveAttribute('data-fog', '')

  // --- Units: the sight shows its default (2) and writes nothing until it differs.
  await open(page, id, 'units', 'artillery')
  const sight = page.getByLabel('Sight', { exact: true })
  await expect(sight).toHaveValue('2')
  await expect(page.getByText(/Only matters on maps with fog of war/).first()).toBeVisible()
  await sight.fill('5')
  await saved(page)
  await open(page, id, 'units', 'infantry')
  await page.getByLabel('Sight', { exact: true }).fill('3')
  await saved(page)
  await page.getByLabel('Sight', { exact: true }).fill('2')
  await saved(page)
  await shot(page, 'fog-units-sight')

  // --- Terrain: a forest that hides, a mountain that adds sight. Structures: a city that adds sight.
  await open(page, id, 'terrain', 'forest')
  await page.getByRole('checkbox', { name: /Hides a unit standing here/ }).check()
  await saved(page)
  await open(page, id, 'terrain')
  await page.getByRole('button', { name: 'New terrain' }).click()
  const dlg = page.getByRole('dialog', { name: 'New terrain' })
  await dlg.getByLabel('Name of the terrain').fill('Mountain')
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name: 'Mountain', level: 2 })).toBeVisible()
  await page.getByLabel('Extra sight for a unit standing here').fill('3')
  await saved(page)
  await shot(page, 'fog-terrain')
  await open(page, id, 'structures', 'city')
  await page.getByLabel('Extra sight for a unit standing here').fill('2')
  await saved(page)

  // --- Story: "a team sees a unit" in an event (not offered in campaign scenes).
  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await newEvent(page, 'spotted')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A team sees a unit (fog of war)' })
  const cond = editor(page).getByRole('group', { name: 'Only if: Condition 1' })
  await expect(cond.getByLabel('Seen by team')).toHaveValue('blue')
  await cond.getByLabel('Which unit must be seen', { exact: true }).selectOption({ label: 'Any unit of type…' })
  await cond.getByLabel('Which unit must be seen: unit type').selectOption({ label: 'Tank' })
  await cond.getByLabel('Seen by team').selectOption({ label: 'Red Army' })
  await shot(page, 'fog-story')
  await saved(page)

  // --- The files are exactly that, and the real loader accepts them.
  const units = readDisk(id, 'data/units.json').units as Array<Record<string, any>>
  expect(units.find((u) => u.id === 'artillery')!.vision).toBe(5)
  expect(units.find((u) => u.id === 'infantry')).not.toHaveProperty('vision') // 2 is the default: never written
  expect(units.find((u) => u.id === 'tank')).not.toHaveProperty('vision')
  const terrain = readDisk(id, 'data/terrain.json').terrain as Array<Record<string, any>>
  expect(terrain.find((t) => t.id === 'forest')).toMatchObject({ hides: true })
  expect(terrain.find((t) => t.id === 'forest')).not.toHaveProperty('visionBonus')
  expect(terrain.find((t) => t.id === 'mountain')).toMatchObject({ visionBonus: 3 })
  expect(terrain.find((t) => t.id === 'mountain')).not.toHaveProperty('hides')
  expect((readDisk(id, 'data/structures.json').structures as Array<Record<string, any>>).find((s) => s.id === 'city')).toMatchObject({ visionBonus: 2 })
  const map = readDisk(id, 'maps/crossroads.json')
  expect(map.fog).toBe(true)
  expect(map.events).toEqual([{ id: 'spotted', when: { battle: 'start' }, if: [{ sees: { type: 'tank' }, team: 'red' }], do: [] }])
  expect(await game.problems(id)).toEqual([])

  // --- Undo takes the fog away again (and the key with it).
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await fogBox(page).uncheck()
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json')).not.toHaveProperty('fog')
  await undo(page).click()
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').fog).toBe(true)
})

test('a map with more than 32 sides: fog cannot be switched on, and one that already has it gets a check', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Many Sides')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const map = doc.maps.crossroads
  map.sides = Array.from({ length: 33 }, (_, i) => ({ id: `s${i}`, name: `Side ${i}`, controller: 'ai' }))
  map.units = []
  map.structures = []
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: { crossroads: map } } })
  expect(res.ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await expect(fogBox(page)).toBeDisabled()
  await expect(page.getByTestId('fog-settings')).toContainText('This map has 33 sides. Fog of war works with at most 32.')
  // Written by hand with fog on: the editor says so in amber (and can still switch it off).
  const again = await (await request.get(`/api/projects/${id}`)).json()
  again.maps.crossroads.fog = true
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: again.revision, maps: { crossroads: again.maps.crossroads } } })).ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')
  await expect(page.getByTestId('map-warnings')).toContainText('at most 32 sides')
  expect((await game.problems(id)).some((p) => /32/.test(p.message))).toBe(true) // and the real loader indeed refuses
  await tab(page, 'Map').click()
  await expect(fogBox(page)).toBeEnabled()
  await fogBox(page).uncheck()
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})

test('the checks name a sight, bonus or hides the loader would refuse, on the screen that holds it', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Fog Checks')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.units[0].vision = 25
  doc.terrain[0].visionBonus = 11
  doc.structures[0].visionBonus = -2
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, units: doc.units, terrain: doc.terrain, structures: doc.structures } })
  expect(res.ok()).toBeTruthy()
  const checks = page.getByTestId('economy-checks').filter({ visible: true })
  await open(page, id, 'units', doc.units[0].id)
  await expect(checks).toContainText('sight must be a whole number from 0 to 20')
  await open(page, id, 'terrain', doc.terrain[0].id)
  await expect(checks).toContainText('extra sight must be a whole number from 0 to 10')
  await open(page, id, 'structures', doc.structures[0].id)
  await expect(checks).toContainText('extra sight must be a whole number from 0 to 10')
  expect((await game.problems(id)).length).toBeGreaterThan(0)
})

test('@smoke the Play panel offers "Show everything" only on a map with fog; it changes the player address, is remembered, and the player loads', async ({ page, game }) => {
  requirePlayer()
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Fog Play')
  const dialog = page.getByRole('dialog', { name: 'Play', exact: true })
  const play = async () => {
    await page.getByRole('button', { name: 'Play', exact: true }).click()
    await expect(dialog).toBeVisible()
    await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
    await expect(page.getByTestId('play-error')).toHaveCount(0)
  }
  const all = () => dialog.getByRole('checkbox', { name: 'Show everything' })

  // No fog: no box, no parameter.
  await openMap(page, id, 'crossroads')
  await play()
  await expect(all()).toHaveCount(0)
  await expect(dialog.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=crossroads`)
  await dialog.getByRole('button', { name: 'Close' }).click()

  // Fog on: the box shows, unticked.
  await tab(page, 'Map').click()
  await fogBox(page).check()
  await saved(page)
  await play()
  await expect(all()).toBeVisible()
  await expect(all()).not.toBeChecked()
  await expect(dialog.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=crossroads`)
  await shot(page, 'fog-play-header')
  await all().check()
  await expect(dialog.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=crossroads&showall=1`)
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)

  // Remembered for this game: closing and playing again keeps it ticked.
  await dialog.getByRole('button', { name: 'Close' }).click()
  await play()
  await expect(all()).toBeChecked()
  await expect(dialog.locator('iframe')).toHaveAttribute('src', /&showall=1$/)
  await all().uncheck()
  await expect(dialog.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=crossroads`)
})

test.describe('on a phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
  test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

  test('the map settings with fog and the aid at 360 px, and the fields on Units, Terrain and Structures: no horizontal scroll', async ({ page, game }) => {
    const id = await game.create('economy-tactics', 'Phone Fog')
    await openMap(page, id, 'crossroads')
    await tab(page, 'Map').click()
    await expect(fogBox(page)).toBeVisible()
    await fogBox(page).check()
    await aidBox(page).check()
    await expect(page.getByLabel('Side', { exact: true })).toBeVisible()
    await expectNoHScroll(page)
    for (const c of [fogBox(page), aidBox(page), page.getByLabel('Side', { exact: true })]) {
      await c.scrollIntoViewIfNeeded()
      const box = (await c.boundingBox())!
      expect(box.x + box.width, 'inside the screen').toBeLessThanOrEqual(361)
    }
    await shot(page, 'fog-phone-map-settings')
    await page.goto(editorUrl(id, 'units', 'tank'))
    await expect(page.getByLabel('Sight', { exact: true })).toBeVisible()
    await expectNoHScroll(page)
    await shot(page, 'fog-phone-unit')
    await page.goto(editorUrl(id, 'terrain', 'forest'))
    await expect(page.getByRole('checkbox', { name: /Hides a unit standing here/ })).toBeVisible()
    await expectNoHScroll(page)
    await shot(page, 'fog-phone-terrain')
    await page.goto(editorUrl(id, 'structures', 'city'))
    await expect(page.getByLabel('Extra sight for a unit standing here')).toBeVisible()
    await expectNoHScroll(page)

    // The Play panel's header with the extra box (fog is on for the map): everything stays inside the screen.
    requirePlayer()
    await openMap(page, id, 'crossroads')
    await page.getByRole('button', { name: 'Play', exact: true }).click()
    const dialog = page.getByRole('dialog', { name: 'Play', exact: true })
    await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
    const box = dialog.getByRole('checkbox', { name: 'Show everything' })
    await expect(box).toBeVisible()
    for (const c of [box, dialog.getByRole('button', { name: 'Restart' }), dialog.getByRole('button', { name: 'Close' })]) {
      const b = (await c.boundingBox())!
      expect(b.x, 'inside the screen').toBeGreaterThanOrEqual(0)
      expect(b.x + b.width, 'inside the screen').toBeLessThanOrEqual(361)
    }
    await shot(page, 'fog-phone-play')
  })
})
