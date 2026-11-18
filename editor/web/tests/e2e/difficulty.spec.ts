// Difficulties in the editor (docs/DIFFICULTY.md): the Rules card, a map's own numbers, a unit's "Only on", the checks and the Play
// panel's choice; desktop and a 360 px phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, requirePlayer, saved, test, writeDisk } from './fixtures'
import { clickCell, openMap, pickSide, pickUnitType, tab, tool } from './mapkit'

const THREE = {
  default: 'normal',
  list: [
    { id: 'easy', name: 'Easy', description: 'Gentler foes.', enemy: { levels: -1 } },
    { id: 'normal', name: 'Normal' },
    { id: 'hard', name: 'Hard', description: 'Tougher foes.', enemy: { levels: 1 }, xpPercent: 90 },
  ],
}

/** Gives a created game the three difficulties on disk (the helper reads the files when the page opens). */
function withDifficulties(id: string, set: object = THREE): void {
  const g = readDisk(id, 'game.json')
  g.difficulties = set
  writeDisk(id, 'game.json', JSON.stringify(g, null, 2))
}

const entry = (page: Page, id: string) => page.getByTestId(`difficulty-${id}`)

test('@smoke the Difficulty card: start, edit numbers, the rule, the default, add and delete; the file stays plain', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'DiffCard')
  expect(readDisk(id, 'game.json').difficulties).toBeUndefined()  // a game that does not use them has no key
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByTestId('difficulty-none')).toBeVisible()
  await page.getByRole('button', { name: 'Start with Easy, Normal and Hard' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list.map((d: { id: string }) => d.id)).toEqual(['easy', 'normal', 'hard'])
  expect(readDisk(id, 'game.json').difficulties.default).toBe('normal')
  expect(readDisk(id, 'game.json').difficulties).not.toHaveProperty('change')  // lowerOnly is the default and never written

  // Numbers of Hard: levels, a flat Defense, experience.
  await entry(page, 'hard').getByText('What it changes').click()
  await page.getByLabel('Enemy levels: Hard', { exact: true }).fill('2')
  await page.getByLabel('Experience percent: Hard', { exact: true }).fill('80')
  await page.getByLabel('Defense: Hard', { exact: true }).fill('1')
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list[2]).toMatchObject({ id: 'hard', enemy: { levels: 2, stats: { def: 1 } }, xpPercent: 80 })
  // 100 is not written (it means nothing)
  await page.getByLabel('Experience percent: Hard', { exact: true }).fill('100')
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list[2]).not.toHaveProperty('xpPercent')

  // The change rule and the default.
  await page.getByRole('radio', { name: 'Any time' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.change).toBe('free')
  await page.getByRole('radio', { name: 'Only to an easier one' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties).not.toHaveProperty('change')
  await page.getByLabel('Chosen unless the player picks', { exact: true }).selectOption({ label: 'Hard' })
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.default).toBe('hard')

  // Add one, move it first, rename it; ids stay.
  await page.getByLabel('Add a difficulty', { exact: true }).fill('Nightmare')
  await page.getByRole('button', { name: 'Add difficulty' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list.map((d: { id: string }) => d.id)).toEqual(['easy', 'normal', 'hard', 'nightmare'])
  await page.getByRole('button', { name: 'Move Nightmare up' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list.map((d: { id: string }) => d.id)).toEqual(['easy', 'normal', 'nightmare', 'hard'])
  await page.getByLabel('Name of difficulty 3', { exact: true }).fill('Brutal')
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list[2]).toMatchObject({ id: 'nightmare', name: 'Brutal' })
  expect(await game.problems(id)).toEqual([])

  // Delete asks first; deleting the default moves it.
  await page.getByRole('button', { name: 'Delete Hard' }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Delete the difficulty “Hard”?')
  await expect(dlg).toContainText('It is the game’s default')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list.map((d: { id: string }) => d.id)).toEqual(['easy', 'normal', 'nightmare'])
  expect(readDisk(id, 'game.json').difficulties.default).toBe('easy')
  expect(await game.problems(id)).toEqual([])
})

test('@smoke a map\'s own numbers and a unit\'s "Only on": they are written, replace the game\'s, and delete follows through', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'DiffMap')
  withDifficulties(id)
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  await page.getByTestId('map-difficulty').getByText('Difficulty on this map').click()
  await page.getByTestId('map-difficulty-hard').getByText('Hard', { exact: true }).click()
  await page.getByLabel('Enemy levels: Hard (this map)', { exact: true }).fill('3')
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').difficulty).toEqual({ hard: { enemy: { levels: 3 } } })
  expect(await game.problems(id)).toEqual([])

  // A new red soldier: only on Normal and Hard, and never shifted.
  await tab(page, 'Tool').click()
  await tool(page, 'Units').click()
  await pickSide(page, 'Red').click()
  await pickUnitType(page, 'Soldier').click()
  await clickCell(page, 6, 1)
  const only = page.getByTestId('difficulty-only')
  await expect(only).toContainText('Only on these difficulties')
  await only.getByRole('checkbox', { name: 'Easy' }).uncheck()
  await only.getByRole('checkbox', { name: /Keep the same on every difficulty/ }).check()
  await saved(page)
  const placed = readDisk(id, 'maps/ford.json').units.at(-1)
  expect(placed).toMatchObject({ type: 'soldier', side: 'red', x: 6, y: 1, difficulty: ['normal', 'hard'], difficultyShift: false })
  await only.getByRole('checkbox', { name: 'Easy' }).check()  // all ticked = no limit
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').units.at(-1)).not.toHaveProperty('difficulty')
  await only.getByRole('checkbox', { name: 'Normal' }).uncheck()
  await only.getByRole('checkbox', { name: 'Easy' }).uncheck()
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').units.at(-1).difficulty).toEqual(['hard'])
  expect(await game.problems(id)).toEqual([])

  // A computer side can opt out of the enemy numbers.
  await tab(page, 'Sides').click()
  await page.getByTestId('side-red').getByRole('checkbox', { name: /Difficulty changes this side/ }).uncheck()
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').sides.find((s: { id: string }) => s.id === 'red').difficultyShift).toBe(false)

  // Deleting "Hard" from the Rules screen takes the map's numbers and the limit with it, and says so first.
  await gotoSection(page, 'Rules')
  await entry(page, 'hard').getByRole('button', { name: 'Delete Hard' }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('its own numbers for “Hard” are removed')
  await expect(dlg).toContainText('was only on “Hard” and now appears on every difficulty')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  const m = readDisk(id, 'maps/ford.json')
  expect(m).not.toHaveProperty('difficulty')
  expect(m.units.at(-1)).not.toHaveProperty('difficulty')
  expect(await game.problems(id)).toEqual([])
})

test('a unit gets variants for other difficulties: they share the square and the name, the panel steps through them, and the loader agrees', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'DiffVariants')
  withDifficulties(id)
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await pickSide(page, 'Red').click()
  await pickUnitType(page, 'Soldier').click()
  await clickCell(page, 6, 1)
  const only = page.getByTestId('difficulty-only')
  await page.getByLabel('Story name').fill('gorm')
  await only.getByRole('checkbox', { name: 'Easy' }).uncheck()
  await only.getByRole('checkbox', { name: 'Normal' }).uncheck()  // now only on Hard
  await saved(page)
  // Add a variant: a copy for the difficulties nothing on the square covers, with the same story name.
  await page.getByRole('button', { name: 'Add a variant for another difficulty' }).click()
  await saved(page)
  const at = (u: { x: number; y: number }) => u.x === 6 && u.y === 1
  let stack = readDisk(id, 'maps/ford.json').units.filter(at)
  expect(stack.map((u: { difficulty?: string[] }) => u.difficulty)).toEqual([['hard'], ['easy', 'normal']])
  expect(stack.map((u: { id: string }) => u.id)).toEqual(['gorm', 'gorm'])
  expect(await game.problems(id)).toEqual([])
  // The copy is selected; the panel lists both and picking one switches what it edits.
  const variants = page.getByTestId('unit-variants')
  await expect(variants.getByRole('button', { name: /Variant 2 of 2/ })).toHaveAttribute('aria-pressed', 'true')
  await variants.getByRole('button', { name: /Variant 1 of 2/ }).click()
  await expect(only.getByRole('checkbox', { name: 'Hard' })).toBeChecked()
  await expect(only.getByRole('checkbox', { name: 'Easy' })).not.toBeChecked()
  // Clicking the selected square again steps to the next variant.
  await clickCell(page, 6, 1)
  await expect(variants.getByRole('button', { name: /Variant 2 of 2/ })).toHaveAttribute('aria-pressed', 'true')
  // Ticking a difficulty the other variant has makes them overlap: the editor says so and so does the loader.
  await only.getByRole('checkbox', { name: 'Hard' }).check()
  await saved(page)
  await expect(page.getByText(/shares its square with/).first()).toBeVisible()
  expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('shares a cell with') || p.message.includes('already used by'))).toBe(true)
  stack = readDisk(id, 'maps/ford.json').units.filter(at)
  expect(stack).toHaveLength(2)
})

test('a placement still limited to a difficulty that is gone is pointed out', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'DiffGone')
  withDifficulties(id)
  const m = readDisk(id, 'maps/ford.json')
  m.units.push({ type: 'soldier', side: 'red', x: 6, y: 1, name: 'Ghost', difficulty: ['extreme'] })
  writeDisk(id, 'maps/ford.json', JSON.stringify(m, null, 2))
  await openMap(page, id, 'ford')
  await expect(page.getByText(/is limited to the difficulty “extreme”, which does not exist/)).toBeVisible()
})

test('the Rules card and a unit\'s limits fit a 360 px phone', async ({ page, game }) => {
  await page.setViewportSize({ width: 360, height: 740 })
  const id = await game.create('strategy-rpg', 'DiffPhone')
  withDifficulties(id)
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Rules')
  await entry(page, 'hard').getByText('What it changes').click()
  await expect(page.getByLabel('Enemy levels: Hard', { exact: true })).toBeVisible()
  await expectNoHScroll(page)
  await page.getByLabel('Enemy levels: Hard', { exact: true }).fill('2')
  await saved(page)
  expect(readDisk(id, 'game.json').difficulties.list[2].enemy.levels).toBe(2)
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  await page.getByTestId('map-difficulty').getByText('Difficulty on this map').click()
  await expectNoHScroll(page)
  await tab(page, 'Tool').click()
  await tool(page, 'Units').click()
  await pickSide(page, 'Red').click()
  await pickUnitType(page, 'Soldier').click()
  await clickCell(page, 6, 1)
  await expect(page.getByTestId('difficulty-only')).toBeVisible()
  await expectNoHScroll(page)
})

test('Play offers the difficulty (Ask me, or one of the list) only for a game that has some', async ({ page, game }) => {
  requirePlayer()
  const id = await game.create('strategy-rpg', 'DiffPlay')
  await page.goto(editorUrl(id, 'rules'))
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const panel = page.getByRole('dialog', { name: 'Play', exact: true })
  await expect(panel).toBeVisible()
  await expect(panel.getByLabel('Difficulty')).toHaveCount(0)  // no difficulties: no choice
  await panel.getByRole('button', { name: 'Close' }).click()
  withDifficulties(id)
  await page.reload()
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(panel.getByLabel('Difficulty')).toHaveValue('')
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=ford`)
  await panel.getByLabel('Difficulty').selectOption({ label: 'Hard' })
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=ford&difficulty=hard`)
})
