// E1 of docs/SRPG.md: the Characters screen, characters on the map (Characters tab, unit form), the "class" wording,
// the checks and the loader's problems. Driven like a person would, with the saved files read back.
import type { Page } from '@playwright/test'
import { bareGame } from './artkit'
import { newCharacter, switchOn } from './characterskit'
import { editorUrl, expect, readDisk, requirePlayer, saved, section, test, writeDisk } from './fixtures'
import { clickCell, openMap, pickSide, pickUnitType, tool } from './mapkit'

const charactersTab = (page: Page) => page.getByRole('tab', { name: 'Characters', exact: true })
const pickCharacter = (page: Page, name: string | RegExp) => page.getByRole('radiogroup', { name: 'Characters' }).getByRole('radio', { name })
const diskUnits = (id: string, file: string) => readDisk(id, `maps/${file}.json`).units

test('@smoke make a character, give it a class and bonuses, place it on a map, and delete it', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Characters')
  // Before any character the map editor says "unit type" and has no Characters tab.
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await expect(pickUnitType(page, 'Soldier')).toBeVisible()
  await expect(charactersTab(page)).toHaveCount(0)

  await switchOn(page, id)
  await expect(page.getByRole('heading', { name: 'Characters', level: 1 })).toBeVisible()
  await expect(page.getByText('No characters yet').first()).toBeVisible()
  await expect(page.getByTestId('character-checks')).toHaveCount(0)

  await newCharacter(page, 'Aldric')
  await expect(page).toHaveURL(/\/characters\/aldric$/)
  const form = page.locator('.md-detail')
  await form.getByLabel('Name', { exact: true }).fill('Aldric the Bold')
  await form.getByLabel('Class', { exact: true }).selectOption('cavalier')
  await form.getByLabel('Description').fill('Captain of the Blue Company.')
  await form.getByLabel('Joins at level').fill('3')
  // Three columns: the class's value (greyed), the personal bonus, the total.
  await expect(form.getByTestId('base-str').first()).toHaveText('6')
  await form.getByLabel('Strength personal bonus').fill('2')
  await expect(form.getByTestId('total-str').first()).toHaveText('8')
  await form.getByLabel('Strength personal bonus').fill('-9')
  await expect(form.getByTestId('total-str').first()).toHaveText('0') // a stat never goes below 0
  await form.getByLabel('Strength personal bonus').fill('2')
  await form.getByLabel('Strength personal growth bonus').fill('5')
  await saved(page)
  expect(readDisk(id, 'data/characters.json')).toEqual({
    characters: [{ id: 'aldric', name: 'Aldric the Bold', description: 'Captain of the Blue Company.', type: 'cavalier', level: 3, stats: { str: 2 }, growths: { str: 5 } }],
  })

  // Not on any map yet: the check says so.
  await expect(page.getByTestId('character-checks')).toContainText('Not used on any map')

  // The map: the Characters tab places the character as { character, side, x, y } and says "class".
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await expect(page.getByRole('radiogroup', { name: 'Classes' })).toBeVisible()
  await charactersTab(page).click()
  await pickSide(page, 'Blue').click()
  await pickCharacter(page, /Aldric the Bold/).click()
  const before = diskUnits(id, 'ford')
  await clickCell(page, 3, 1)
  const props = page.getByTestId('unit-props')
  await expect(props.getByTestId('unit-character')).toContainText('Aldric the Bold')
  await expect(props.getByLabel('Class', { exact: true })).toHaveValue('cavalier')
  await expect(props.getByLabel('Story name')).toHaveCount(0) // a character is known to events by its own id
  // Its numbers on this map start from the class plus the personal bonus.
  await expect(props.getByLabel('Strength for this unit')).toHaveAttribute('placeholder', '8')
  await saved(page)
  expect(diskUnits(id, 'ford')).toEqual([...before, { character: 'aldric', side: 'blue', x: 3, y: 1 }])
  // It stands on the map once: greyed in the picker, and the check is gone.
  await expect(pickCharacter(page, /Aldric the Bold/)).toBeDisabled()
  // Another class on this map only is stored as `type`; its own class takes the key away again.
  await props.getByLabel('Class', { exact: true }).selectOption('archer')
  await saved(page)
  expect(diskUnits(id, 'ford').at(-1)).toEqual({ character: 'aldric', side: 'blue', x: 3, y: 1, type: 'archer' })
  await props.getByLabel('Class', { exact: true }).selectOption('cavalier')
  await saved(page)
  expect(diskUnits(id, 'ford').at(-1)).toEqual({ character: 'aldric', side: 'blue', x: 3, y: 1 })
  expect(await game.problems(id)).toEqual([])

  // "Open character" goes back to the form.
  await props.getByRole('link', { name: 'Open character' }).click()
  await expect(page).toHaveURL(/\/characters\/aldric$/)
  await expect(page.getByTestId('character-checks')).toHaveCount(0)

  // Delete: the question names the map; the unit goes with it, in one undo step.
  await page.getByRole('button', { name: 'Delete' }).click()
  await expect(page.getByRole('dialog')).toContainText('placed on the map')
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(diskUnits(id, 'ford')).toEqual(before)
  expect(readDisk(id, 'data/characters.json')).toEqual({ characters: [] })
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(diskUnits(id, 'ford').at(-1)).toMatchObject({ character: 'aldric' })
  expect(readDisk(id, 'data/characters.json').characters).toHaveLength(1)
  expect(await game.problems(id)).toEqual([])
})

test('a class that characters use cannot go without them: deleting it says so and takes them along', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Class delete')
  await switchOn(page, id)
  await newCharacter(page, 'Mira')
  await page.locator('.md-detail').getByLabel('Class', { exact: true }).selectOption('archer')
  await saved(page)
  await section(page, 'Units').click()
  await page.getByRole('button', { name: /^Archer/ }).click()
  await page.getByRole('button', { name: 'Delete' }).click()
  await expect(page.getByRole('dialog')).toContainText('Mira')
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(readDisk(id, 'data/characters.json')).toEqual({ characters: [] })
  expect(await game.problems(id)).toEqual([])
})

test('duplicating a character gives a fresh id and reordering is saved; the portrait card is there', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Dup')
  await switchOn(page, id)
  await newCharacter(page, 'Tess')
  await page.getByRole('button', { name: 'Duplicate' }).click()
  await expect(page).toHaveURL(/\/characters\/tess_copy$/)
  await page.getByRole('button', { name: 'Move character up' }).click()
  await saved(page)
  expect(readDisk(id, 'data/characters.json').characters.map((c: { id: string }) => c.id)).toEqual(['tess_copy', 'tess'])
  expect(await game.problems(id)).toEqual([])
  // No image in a "bare" game: the portrait card says what to do first.
  await expect(page.getByTestId('character-portrait')).toBeVisible()
})

test('the loader\'s words reach the Problems panel and the Characters entry, and picking a real class fixes it', async ({ page, game, request }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Problem')
  writeDisk(id, 'data/characters.json', JSON.stringify({ characters: [{ id: 'ghost', name: 'Ghost', type: 'nope' }] }))
  await page.goto(editorUrl(id, 'characters'))
  await page.getByRole('button', { name: /^\d+ problems?$/ }).click()
  const panel = page.getByRole('region', { name: 'Problems' })
  await expect(panel).toContainText('character "ghost": unknown unit type "nope"')
  await expect(page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name: /^Characters/ }).locator('.nav-dot')).toBeVisible()
  // Fix it from the form: the class is picked from the real ones and the problem goes away.
  await page.getByRole('button', { name: 'Close problems' }).click()
  await page.locator('.md-detail').getByLabel('Class', { exact: true }).selectOption('lord')
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})

test('Play starts with a character on the map', async ({ page, game, request }) => {
  requirePlayer()
  const id = await bareGame(request, game, 'strategy-rpg', 'Play characters')
  await switchOn(page, id)
  await newCharacter(page, 'Rowan')
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await charactersTab(page).click()
  await pickSide(page, 'Blue').click()
  await pickCharacter(page, /Rowan/).click()
  await clickCell(page, 3, 1)
  await saved(page)
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})
