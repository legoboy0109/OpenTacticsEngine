// E3 of docs/SRPG.md: promotion (item switch, the Promotion card, the Rules card, the story command and trigger) and carrying / pair up
// (the Rules card, "Gives when behind" on a unit type), through the real editor. The proof that matters: the exact JSON the editor
// wrote, and that the game loader accepts it (/problems is empty).
import type { Page } from '@playwright/test'
import { expectNoHScroll, editorUrl, expect, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'
import { addCommand, createItem, editor, eventsTab, form, newEvent, when } from './storykit'

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
const unitsFile = (id: string) => readDisk(id, 'data/units.json').units as any[]

/** A promotion item. */
async function promotionItem(page: Page, id: string): Promise<void> {
  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Officer Writ')
  await page.getByTestId('item-promotion').getByRole('switch', { name: 'Promotion item' }).check()
  await saved(page)
}

/** The Archer starts with the item, so a promotion that needs it can happen. */
async function handOut(page: Page, id: string): Promise<void> {
  await page.goto(editorUrl(id, 'units', 'archer'))
  await page.getByTestId('unit-start-items').getByLabel('Starting items: Add an item…').selectOption('officer_writ')
  await saved(page)
}

test('@smoke a promotion item and a promotion: the files are exactly that, the clean-up works, and the game loads', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Promotion')
  await promotionItem(page, id)
  expect(readDisk(id, 'data/items.json').items.find((i: any) => i.id === 'officer_writ')).toMatchObject({ kind: 'usable', promotes: true })
  await shot(page, 'item-promotion-card')

  // --- The Lord's promotion: new rows start valid (the first other unit type) and write nothing else.
  await page.goto(editorUrl(id, 'units', 'lord'))
  const card = page.getByTestId('unit-promotion')
  await expect(card).toContainText('No promotions')
  await card.getByRole('button', { name: 'Add a promotion' }).click()
  await saved(page)
  expect(unitsFile(id)[0].promotions).toEqual([{ to: 'cavalier' }])
  // A promotion that only items can start, and nobody has the item: the card says so.
  await card.getByRole('checkbox', { name: 'Officer Writ' }).check()
  await expect(page.getByTestId('promotion-checks')).toContainText('nobody is ever given its promotion item')
  await shot(page, 'promotion-problem')
  // Handing the item out (the Archer starts with it) clears it.
  await handOut(page, id)
  await page.goto(editorUrl(id, 'units', 'lord'))
  await card.getByLabel('From level').fill('5')
  await card.getByLabel('On reaching the level').selectOption('offer')
  await card.getByLabel('Level afterwards').selectOption('keep')
  await expect(page.getByTestId('promotion-checks')).toHaveCount(0)
  await card.getByLabel('Stat changes').selectOption('table')
  await card.getByLabel('Strength Change').fill('3')
  await card.getByLabel('Speed Change').fill('-1')
  await saved(page)
  const promo = unitsFile(id)[0].promotions[0]
  expect(promo).toMatchObject({ to: 'cavalier', fromLevel: 5, items: ['officer_writ'], atLevel: 'offer', levelAfter: 'keep' })
  expect(promo.gains).toMatchObject({ str: 3, spd: -1 })
  await shot(page, 'unit-promotion-card')
  expect(await game.problems(id)).toEqual([])

  // Back to "the difference" removes the table; the game-wide rule is on the Rules screen.
  await card.getByLabel('Stat changes').selectOption('diff')
  await saved(page)
  expect(unitsFile(id)[0].promotions[0]).not.toHaveProperty('gains')
  await page.goto(editorUrl(id, 'rules'))
  const rules = page.getByTestId('promotion-rules')
  await expect(rules.getByLabel('After promoting, the level')).toHaveValue('reset')
  await rules.getByLabel('After promoting, the level').selectOption('keep')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.promotion).toEqual({ levelAfter: 'keep' })
  await rules.getByLabel('After promoting, the level').selectOption('reset')
  await saved(page)
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('promotion')
  await shot(page, 'rules-promotion-card')

  // --- Switching the item's promotion off lets the promotion go of it; deleting the unit type removes the promotion (with a question).
  await page.goto(editorUrl(id, 'items', 'officer_writ'))
  await page.getByTestId('item-promotion').getByRole('switch', { name: 'Promotion item' }).uncheck()
  await saved(page)
  expect(unitsFile(id)[0].promotions[0]).not.toHaveProperty('items')
  await page.goto(editorUrl(id, 'units', 'cavalier'))
  await page.getByRole('button', { name: 'Delete' }).click()
  await expect(page.getByRole('dialog')).toContainText('can promote into it (that promotion is removed)')
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(unitsFile(id).find((u) => u.id === 'lord')).not.toHaveProperty('promotions')
  expect(await game.problems(id)).toEqual([])
})

test('@smoke carrying: pair up with a rule, rows and numbers; the unit type card; carry drops the pair-up numbers after asking', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Carrying')
  await page.goto(editorUrl(id, 'rules'))
  const card = page.getByTestId('carrying-card')
  await expect(card.getByLabel('How units help each other')).toHaveValue('off')
  await expect(card).not.toContainText('Who can carry whom')
  await shot(page, 'rules-carrying-off')
  await card.getByLabel('How units help each other').selectOption('pairUp')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.rescue).toEqual({ mode: 'pairUp' })

  // Who can carry whom: a preset, read back from the formula; the formula itself is under Advanced.
  await card.getByLabel('Who can carry whom').selectOption('stat')
  await card.getByLabel('Carrier’s stat').selectOption('str')
  await card.getByLabel('Other unit’s stat').selectOption('def')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.rescue.canCarry).toBe('a.str >= b.def')
  await card.locator('summary', { hasText: 'Advanced' }).click()
  await expect(card.getByLabel('Custom rule')).toHaveValue('a.str >= b.def')
  await card.getByLabel('Custom rule').fill('a.str >= b.str and not b.tags.mounted')
  await expect(card.getByLabel('Who can carry whom')).toHaveValue('custom')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.rescue.canCarry).toBe('a.str >= b.str and not b.tags.mounted')
  await card.getByLabel('Custom rule').fill('')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.rescue).not.toHaveProperty('canCarry')

  // While carrying: rows of stat + halved / minus N. Never the health stat.
  await card.getByRole('button', { name: 'Add a change' }).click()
  await expect(card.getByLabel('Stat').first().locator('option', { hasText: 'HP' })).toHaveCount(0)
  await card.getByLabel('Change', { exact: true }).first().selectOption('minus')
  await card.getByLabel('By how much').first().fill('2')
  await card.getByRole('button', { name: 'Add a change' }).click()
  await saved(page)
  const rescue = readDisk(id, 'game.json').rules.rescue
  expect(Object.values(rescue.carrierStats)).toEqual([-2, { percent: 50 }])
  // The partner behind gives.
  await card.getByLabel('Strength Bonus').fill('2')
  await card.getByLabel('Defense Bonus').fill('1')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.rescue.pairBonus).toEqual({ str: 2, def: 1 })
  await shot(page, 'rules-carrying-pairup')

  // A unit type's own numbers.
  await page.goto(editorUrl(id, 'units', 'cavalier'))
  const own = page.getByTestId('unit-pair-bonus')
  await own.getByLabel('Strength Bonus').fill('3')
  await saved(page)
  expect(unitsFile(id).find((u) => u.id === 'cavalier').pairBonus).toEqual({ str: 3 })
  await shot(page, 'unit-pair-bonus-card')
  expect(await game.problems(id)).toEqual([])

  // Carry: the pair-up numbers are refused there, so changing asks first (Cancel keeps everything).
  await page.goto(editorUrl(id, 'rules'))
  await card.getByLabel('How units help each other').selectOption('carry')
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('This removes 3 settings')
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  await expect(card.getByLabel('How units help each other')).toHaveValue('pairUp')
  await card.getByLabel('How units help each other').selectOption('carry')
  await dlg.getByRole('button', { name: 'Switch' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').rules.rescue).toEqual({ mode: 'carry', carrierStats: rescue.carrierStats })
  expect(unitsFile(id).find((u) => u.id === 'cavalier')).not.toHaveProperty('pairBonus')
  expect(await game.problems(id)).toEqual([])
  await card.getByLabel('How units help each other').selectOption('off')
  await dlg.getByRole('button', { name: 'Switch' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('rescue')
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the Promote command and the "A unit is promoted" trigger in an event: the file is exactly that and the game loads', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Promote story')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.ford.units[0].id = 'eirik'
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })).ok()).toBeTruthy()

  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'rise')
  await addCommand(page, 'Then', 'Promote a unit')
  await form(page).getByLabel('Which unit is promoted').selectOption({ label: 'Eirik (eirik)' })
  await form(page).getByLabel('It becomes').selectOption('cavalier')
  await newEvent(page, 'cheer')
  await when(page, 'A unit is promoted')
  await editor(page).getByLabel('Which unit', { exact: true }).selectOption('any')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('Well done!')
  await saved(page)
  const events = readDisk(id, 'maps/ford.json').events
  expect(events[0]).toMatchObject({ id: 'rise', do: [{ promote: 'eirik', to: 'cavalier' }] })
  expect(events[1]).toMatchObject({ id: 'cheer', when: { promoted: {} }, do: [{ say: 'Well done!' }] })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'story-promote')

  // Back to "its only promotion": `to` is left out (the loader then needs exactly one promotion on the type).
  await page.getByRole('button', { name: 'All events' }).click()
  await page.getByRole('button', { name: 'Open event rise', exact: true }).click()
  await editor(page).getByRole('button', { name: /^1\. Promotes/ }).click()
  await form(page).getByLabel('It becomes').selectOption({ label: 'Its only promotion' })
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').events[0].do[0]).toEqual({ promote: 'eirik' })
})

test('@smoke on a 360 px phone: the Promotion card, the item switch and both Rules cards fit without sideways scrolling', async ({ page, game }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Phone promotion')
  await promotionItem(page, id)
  await handOut(page, id)
  await expectNoHScroll(page)
  await page.goto(editorUrl(id, 'units', 'lord'))
  const card = page.getByTestId('unit-promotion')
  await card.getByRole('button', { name: 'Add a promotion' }).click()
  await card.getByLabel('Stat changes').selectOption('table')
  await expectNoHScroll(page)
  await shot(page, 'unit-promotion-phone')
  await page.goto(editorUrl(id, 'rules'))
  const carry = page.getByTestId('carrying-card')
  await carry.getByLabel('How units help each other').selectOption('pairUp')
  await carry.getByRole('button', { name: 'Add a change' }).click()
  await expectNoHScroll(page)
  await carry.scrollIntoViewIfNeeded()
  await shot(page, 'rules-carrying-phone')
})
