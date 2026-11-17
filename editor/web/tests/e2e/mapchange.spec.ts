// Map changes and the fog options (docs/EVENTS.md "Changing the map", docs/ECONOMY.md "Fog of war"; docs/EDITOR_UI.md "Map changes and fog
// options") through the real editor: create a change on the Changes tab, paint squares into it (the base map stays as it was), look at the map
// after it, add the "Apply a map change" command in an event, save, and check the files on disk and that the real game loader accepts them.
// Then rename and delete with the events following, the fog settings, light and blocks sight, and a phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'
import { canvas, clickCell, diskGrid, openMap, pickTerrain, tab } from './mapkit'
import { addCommand, editor, eventsTab, form, newEvent, shot } from './storykit'

const changesTab = (page: Page) => page.getByRole('tab', { name: 'Changes', exact: true })
const panel = (page: Page) => page.getByTestId('changes-panel')
const paintBox = (page: Page) => page.getByRole('switch', { name: /^Paint into this change/ })
const undo = (page: Page) => page.getByRole('button', { name: 'Undo' })

/** A square of crossroads whose terrain is not `not`, found on the saved grid (independent of the UI). */
function squareWithout(id: string, not: string): { x: number; y: number } {
  const g = diskGrid(id, 'crossroads')
  for (let y = 0; y < g.length; y++) for (let x = 0; x < g[y].length; x++) if (g[y][x] !== not) return { x, y }
  throw new Error('no such square')
}

async function newChange(page: Page, name: string) {
  await panel(page).getByLabel('Name', { exact: true }).fill(name)
  await panel(page).getByTestId('change-add').click()
  await expect(panel(page).getByLabel('Change', { exact: true })).toHaveValue(name)
}

test('@smoke create a change, paint into it, see it after, apply it from an event: the files are exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Changes')
  const before = diskGrid(id, 'crossroads')
  const spot = squareWithout(id, 'forest')

  await openMap(page, id, 'crossroads')
  await changesTab(page).click()
  await expect(panel(page).getByTestId('changes-none')).toBeVisible()
  await newChange(page, 'bridge')
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').changes).toEqual({ bridge: { cells: [] } })
  // An empty change changes nothing: the check says so (the loader refuses it too until it does something).
  await expect(page.getByTestId('map-warnings')).toContainText('changes nothing yet')

  // Paint into the change: the squares go in the change, the map itself is not touched.
  await paintBox(page).check()
  await pickTerrain(page, 'Forest').click()
  await clickCell(page, spot.x, spot.y)
  await expect(canvas(page)).toHaveAttribute('data-change-cells', `${spot.x},${spot.y}`)
  await saved(page)
  const map = readDisk(id, 'maps/crossroads.json')
  expect(map.changes).toEqual({ bridge: { cells: [{ x: spot.x, y: spot.y, terrain: 'forest' }] } })
  expect(diskGrid(id, 'crossroads')).toEqual(before)
  await expect(panel(page).getByTestId('change-summary')).toContainText('1 square')
  await expect(page.getByTestId('map-warnings')).toContainText('never applied')

  // Before / After.
  await expect(canvas(page)).toHaveAttribute('data-change-view', 'before')
  await page.getByTestId('change-view-after').click()
  await expect(canvas(page)).toHaveAttribute('data-change-view', 'after')
  await page.getByTestId('change-view-before').click()
  await shot(page, 'changes-tab')

  // The event: "Apply a map change", the first change is picked.
  await eventsTab(page).click()
  await newEvent(page, 'build')
  await addCommand(page, 'Then', 'Apply a map change')
  await expect(form(page).getByLabel('Map change')).toHaveValue('bridge')
  await addCommand(page, 'Then', 'Change terrain')
  await form(page).getByLabel('New terrain').selectOption({ label: 'Forest' })
  await addCommand(page, 'Then', 'Fog on or off')
  await addCommand(page, 'Then', 'Change everyone’s vision')
  await form(page).getByLabel('Vision adjustment').fill('-2')
  await saved(page)
  const events = readDisk(id, 'maps/crossroads.json').events
  expect(events[0].do[0]).toEqual({ applyChange: 'bridge' })
  expect(events[0].do[1]).toMatchObject({ setTerrain: 'forest', area: { x: 0, y: 0, w: 1, h: 1 } })
  expect(events[0].do[2]).toEqual({ setFog: false })
  expect(events[0].do[3]).toEqual({ setVision: -2 })
  // The real loader accepts the whole thing.
  expect(await game.problems(id)).toEqual([])
  // The note is gone now that an event applies it.
  await tab(page, 'Tool').click()
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
})

test('renaming a change makes its commands follow; deleting asks, lists them and removes them with it, and Undo brings all back', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Change rename')
  const spot = squareWithout(id, 'forest')
  await openMap(page, id, 'crossroads')
  await changesTab(page).click()
  await newChange(page, 'bridge')
  await paintBox(page).check()
  await pickTerrain(page, 'Forest').click()
  await clickCell(page, spot.x, spot.y)
  await eventsTab(page).click()
  await newEvent(page, 'build')
  await addCommand(page, 'Then', 'Apply a map change')
  await saved(page)

  // Rename: the event follows.
  await changesTab(page).click()
  await panel(page).getByLabel('Rename: bridge').fill('ford')
  await saved(page)
  expect(Object.keys(readDisk(id, 'maps/crossroads.json').changes)).toEqual(['ford'])
  expect(readDisk(id, 'maps/crossroads.json').events[0].do).toEqual([{ applyChange: 'ford' }])
  // A name that is taken or not allowed is refused with the reason.
  await panel(page).getByLabel('Name', { exact: true }).fill('ford')
  await expect(panel(page)).toContainText('already has a change called “ford”')
  await expect(panel(page).getByTestId('change-add')).toBeDisabled()
  await panel(page).getByLabel('Name', { exact: true }).fill('a b')
  await expect(panel(page)).toContainText('letters, digits')
  await panel(page).getByLabel('Name', { exact: true }).fill('')

  // Delete: the dialog lists the command.
  await panel(page).getByTestId('change-delete').click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Delete the change “ford”?')
  await expect(dlg).toContainText('is removed')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  const gone = readDisk(id, 'maps/crossroads.json')
  expect(gone).not.toHaveProperty('changes')
  expect(gone.events[0].do).toEqual([])
  expect(await game.problems(id)).toEqual([])
  await undo(page).click()
  await saved(page)
  const back = readDisk(id, 'maps/crossroads.json')
  expect(Object.keys(back.changes)).toEqual(['ford'])
  expect(back.events[0].do).toEqual([{ applyChange: 'ford' }])
})

test('an event that names a change that no longer exists is flagged, and the loader indeed refuses it', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Change missing')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.crossroads.events = [{ id: 'oops', when: { battle: 'start' }, do: [{ applyChange: 'ghost' }] }]
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: { crossroads: doc.maps.crossroads } } })
  expect(res.ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')
  await expect(page.getByTestId('map-warnings')).toContainText('“ghost” is not one of this map’s changes')
  expect((await game.problems(id)).some((p) => /ghost/.test(p.message))).toBe(true)
})

test('fog settings: extra vision, the computer sees everything, a computer side that overrules it; switching fog off clears them; light and blocks sight', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Fog options')
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await expect(page.getByLabel('Extra vision')).toHaveCount(0)
  await page.getByRole('checkbox', { name: /^Fog of war/ }).check()
  await page.getByLabel('Extra vision').fill('2')
  await page.getByRole('checkbox', { name: /^The computer sees everything/ }).check()
  await tab(page, 'Sides').click()
  await page.getByLabel('Red Army Sees through fog').selectOption({ label: 'Bound by fog' })
  await saved(page)
  const map = readDisk(id, 'maps/crossroads.json')
  expect(map).toMatchObject({ fog: true, fogVision: 2, aiSeesAll: true })
  expect(map.sides.find((s: any) => s.id === 'red')).toMatchObject({ aiSeesAll: false })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'fog-options-sides')

  // A structure light and a terrain that blocks sight.
  await page.goto(editorUrl(id, 'structures', 'city'))
  await page.getByLabel('Gives light', { exact: true }).fill('3')
  await saved(page)
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByRole('checkbox', { name: /^Blocks sight/ }).check()
  await saved(page)
  expect((readDisk(id, 'data/structures.json').structures as any[]).find((s) => s.id === 'city')).toMatchObject({ light: 3 })
  expect((readDisk(id, 'data/terrain.json').terrain as any[]).find((t) => t.id === 'forest')).toMatchObject({ blocksSight: true })
  expect(await game.problems(id)).toEqual([])

  // Fog off removes the options the loader refuses without fog.
  await openMap(page, id, 'crossroads')
  await tab(page, 'Map').click()
  await page.getByRole('checkbox', { name: /^Fog of war/ }).uncheck()
  await saved(page)
  const off = readDisk(id, 'maps/crossroads.json')
  expect(off).not.toHaveProperty('fog')
  expect(off).not.toHaveProperty('fogVision')
  expect(off).not.toHaveProperty('aiSeesAll')
  expect(await game.problems(id)).toEqual([])
})

test('a torch item: usable, gives light; the checks and the loader agree', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Torch')
  await page.goto(editorUrl(id, 'items'))
  await page.getByRole('button', { name: 'New item' }).click()
  const dlg = page.getByRole('dialog', { name: 'New item' })
  await dlg.getByLabel(/^Name of the item/).fill('Torch')
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name: 'Torch', level: 2 })).toBeVisible()
  const torch = page.getByTestId('item-torch')
  await torch.getByRole('checkbox', { name: /^It gives light/ }).check()
  await torch.getByLabel('Radius').fill('4')
  await torch.getByLabel('Shrinks by').fill('2')
  await saved(page)
  const items = readDisk(id, 'data/items.json').items as any[]
  expect(items.find((i) => i.id === 'torch')).toMatchObject({ kind: 'usable', light: { radius: 4, turns: 3, shrink: 2 } })
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the Changes tab and the new commands fit a 360 px phone', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'Changes phone')
  await page.setViewportSize({ width: 360, height: 800 })
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Maps')
  await openMap(page, id, 'crossroads')
  await expectNoHScroll(page)
  await changesTab(page).click()
  await newChange(page, 'bridge')
  await expectNoHScroll(page)
  await panel(page).scrollIntoViewIfNeeded()
  await shot(page, 'phone-changes')
  await eventsTab(page).click()
  await newEvent(page, 'build')
  await addCommand(page, 'Then', 'Change terrain')
  await expect(page.getByTestId('set-terrain')).toBeVisible()
  await expectNoHScroll(page)
  await addCommand(page, 'Then', 'Apply a map change')
  await expectNoHScroll(page)
  await expect(editor(page)).toBeVisible()
  await tab(page, 'Map').click()
  await page.getByRole('checkbox', { name: /^Fog of war/ }).check()
  await expect(page.getByLabel('Extra vision')).toBeVisible()
  await expectNoHScroll(page)
})
