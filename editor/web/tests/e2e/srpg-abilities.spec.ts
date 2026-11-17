// E3 of docs/SRPG.md: the Abilities screen and the Abilities card on a unit type, through the real editor. The proof that matters:
// the exact JSON the editor wrote, and that the game loader accepts it (/problems is empty).
import type { Page } from '@playwright/test'
import { gotoSection, expectNoHScroll, editorUrl, expect, readDisk, saved, test } from './fixtures'

/** Resizes the window to the page's height first: a full-page capture would smear the sticky sidebar over the content. */
const shot = async (page: Page, name: string) => {
  if (!process.env.OTE_SHOT_DIR) return
  const w = page.viewportSize()!.width
  await page.setViewportSize({ width: w, height: 900 })
  const h = await page.evaluate(() => document.documentElement.scrollHeight)
  await page.setViewportSize({ width: w, height: Math.min(h, 2600) })
  await page.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png` })
  await page.setViewportSize({ width: w, height: 900 })
}

async function newAbility(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New ability' }).click()
  const dlg = page.getByRole('dialog', { name: 'New ability' })
  await dlg.getByLabel('Name of the ability').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

async function switchOn(page: Page, id: string): Promise<void> {
  await page.goto(editorUrl(id, 'rules'))
  await page.getByTestId('srpg-switches').getByRole('switch', { name: 'This game has abilities' }).check()
  await gotoSection(page, 'Abilities')
}

test('@smoke a push and a summon: the file is exactly that, a unit type gets them, and the game loads', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Abilities')
  await switchOn(page, id)
  await expect(page.getByRole('heading', { name: 'Abilities', level: 1 })).toBeVisible()
  await expect(page.getByText('No abilities yet').first()).toBeVisible()
  await shot(page, 'abilities-empty')

  // --- A push: new abilities start valid (push 1, a unit next to it).
  await newAbility(page, 'Shove')
  await saved(page)
  expect(readDisk(id, 'data/abilities.json').abilities).toEqual([{ id: 'shove', name: 'Shove', target: 'unit', effect: { push: 1 } }])
  await expect(page.getByTestId('ability-summary')).toHaveText('Pushes a unit next to it 1 square away; uses the unit’s action.')
  await page.getByLabel('Push how many squares').fill('2')
  await page.getByLabel('Who or what it is used on').selectOption('enemy')
  await page.getByLabel('Cost', { exact: true }).selectOption('free')
  await page.getByLabel('Uses per battle').selectOption('n')
  await page.getByLabel('Number of uses').fill('3')
  await page.getByLabel('Wait between uses').selectOption('n')
  await page.getByLabel('Number of turns').fill('2')
  await page.getByLabel('Only before moving').check()
  await page.getByLabel('Description').fill('Pushes a neighbour away.')
  // A condition: only when its own HP is above half.
  await page.getByLabel('Add a condition…').selectOption({ label: 'Hit points' })
  await page.getByLabel('Compare').selectOption('>')
  await saved(page)
  expect(readDisk(id, 'data/abilities.json').abilities[0]).toEqual({
    id: 'shove', name: 'Shove', target: 'enemy', effect: { push: 2 }, description: 'Pushes a neighbour away.', cost: 'free', uses: 3, cooldown: 2, afterMove: false,
    if: [{ hp: { op: '>', percent: 50 } }],
  })
  await expect(page.getByTestId('ability-summary')).toContainText('Only if its HP is above 50%.')
  await shot(page, 'abilities-push-form')

  // --- A summon: it needs a square, the form for the summoned unit appears.
  await newAbility(page, 'Call Hound')
  await page.getByLabel('What it does', { exact: true }).selectOption('summon')
  await expect(page.getByLabel('Who or what it is used on')).toHaveValue('cell')
  const sf = page.getByTestId('summon-fields')
  await sf.getByLabel('Unit type').selectOption('soldier')
  await sf.getByLabel('Its level').selectOption('fixed')
  await sf.getByLabel('Level', { exact: true }).fill('3')
  await sf.getByLabel('How many at once').fill('2')
  await sf.getByLabel('How long it stays').selectOption('turns')
  await sf.getByLabel('Turns', { exact: true }).fill('4')
  await sf.getByLabel('Can act at once').check()
  await sf.getByLabel('Gives experience').uncheck()
  await sf.getByLabel('Vanish when the summoner is defeated').uncheck()
  await page.getByLabel('From', { exact: true }).fill('1')
  await page.getByLabel('Up to').fill('3')
  await saved(page)
  expect(readDisk(id, 'data/abilities.json').abilities[1]).toEqual({
    id: 'call_hound', name: 'Call Hound', target: 'cell', range: [1, 3],
    effect: { summon: { type: 'soldier', level: 3, limit: 2, vanish: false, lifetime: 4, canAct: true, givesXp: false } },
  })
  await expect(page.getByTestId('ability-summary')).toContainText('Summons up to 2 Soldier on a free square 1 to 3 squares away')
  await shot(page, 'abilities-summon-form')

  // --- The Abilities card on a unit type: nobody has them yet, so the screen says so; then the Lord gets both.
  await expect(page.getByTestId('ability-checks')).toContainText('nobody has this ability yet')
  await page.goto(editorUrl(id, 'units', 'lord'))
  const card = page.getByTestId('unit-abilities')
  await expect(card).toContainText('No abilities.')
  await card.getByLabel('Abilities: Add an ability…').selectOption('call_hound')
  await card.getByLabel('Abilities: Add an ability…').selectOption('shove')
  await page.getByRole('button', { name: 'Move Shove up' }).click()
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord').abilities).toEqual(['shove', 'call_hound'])
  await shot(page, 'unit-abilities-card')
  expect(await game.problems(id)).toEqual([])
  await page.goto(editorUrl(id, 'abilities'))
  await expect(page.getByTestId('ability-checks')).toHaveCount(0)
})

test('deleting an ability says who has it and takes it from them in one undo step; a Problem for an ability whose unit type is gone', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Delete ability')
  await switchOn(page, id)
  await newAbility(page, 'Shove')
  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByTestId('unit-abilities').getByLabel('Abilities: Add an ability…').selectOption('shove')
  await saved(page)

  await page.goto(editorUrl(id, 'abilities', 'shove'))
  await page.getByRole('button', { name: 'Delete' }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Unit “Lord” has it')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord')).not.toHaveProperty('abilities')
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord').abilities).toEqual(['shove'])
  expect(readDisk(id, 'data/abilities.json').abilities).toHaveLength(1)

  // A summon whose unit type was deleted by hand shows the plain message on the screen.
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  doc.abilities = [{ id: 'call', name: 'Call', target: 'cell', effect: { summon: { type: 'ghost' } } }]
  const put = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, abilities: doc.abilities } })
  expect(put.ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'abilities'))
  await expect(page.getByTestId('ability-checks')).toContainText('“ghost”) is not a unit type of this game')
  await shot(page, 'abilities-problem')
})

test('@smoke on a 360 px phone: the list, the forms and the unit card fit without sideways scrolling', async ({ page, game }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Phone abilities')
  await page.goto(editorUrl(id, 'rules'))
  await page.getByTestId('srpg-switches').getByRole('switch', { name: 'This game has abilities' }).check()
  await page.goto(editorUrl(id, 'abilities'))
  await expect(page.getByText('No abilities yet').first()).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'abilities-empty-phone')
  await newAbility(page, 'Call Hound')
  await page.getByLabel('What it does', { exact: true }).selectOption('summon')
  await page.getByLabel('Add a condition…').selectOption({ label: 'Units nearby' })
  await expectNoHScroll(page)
  await shot(page, 'abilities-summon-phone')
  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByTestId('unit-abilities').getByLabel('Abilities: Add an ability…').selectOption('call_hound')
  await expectNoHScroll(page)
  await shot(page, 'unit-abilities-phone')
})
