// The scripted-style computer player through the editor's own forms (docs/AI.md "Scripted-style computer players", docs/EDITOR_UI.md "Behaviour" and "Computer
// player"): the Computer player card of the Rules screen (turn order, sets of attack weights), the new fields of a computer unit's behaviour (patrol route, acting
// order, weights, caution, idle turns, retreat), the same fields in the "Change units’ behaviour" command, and the 360 px phone layout. The proof that matters: the
// exact JSON the forms wrote and that the real loader accepts it (/problems is empty).
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'
import { clickCell, openMap, tab, tool } from './mapkit'
import { addCommand, eventsTab, newEvent } from './storykit'

test.use({ actionTimeout: 10_000 })

const unit = (id: string, name: string) => readDisk(id, 'maps/ford.json').units.find((u: any) => u.name === name)
const box = (page: Page) => page.getByTestId('unit-behaviour')
const mode = (page: Page) => box(page).getByRole('combobox', { name: 'What it does' })
const rules = (id: string) => readDisk(id, 'game.json').rules

test('@smoke the Computer player card and the behaviour fields write what docs/AI.md says and the loader accepts it', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Scripted')
  await page.goto(editorUrl(id, 'rules'))

  // ===== The Rules card =====
  const card = page.getByTestId('ai-rules')
  await expect(card).toBeVisible()
  expect(rules(id).ai).toBeUndefined()
  await card.getByRole('combobox', { name: 'Order its units act in' }).selectOption({ label: 'One unit at a time, in order' })
  await saved(page)
  expect(rules(id).ai).toEqual({ turnOrder: 'order' })
  await card.getByRole('button', { name: 'Add a set' }).click()
  await page.getByRole('dialog').getByLabel('Name').fill('bad name!')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('dialog')).toContainText('letters, digits')
  await page.getByRole('dialog').getByLabel('Name').fill('classic')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await saved(page)
  expect(rules(id).ai.weights).toEqual({ classic: {} })
  const set = card.getByTestId('weight-card-classic')
  await set.getByText('Set “classic”').click()
  await set.getByLabel('Damage', { exact: true }).fill('3')
  await set.getByLabel('Counterattack taken', { exact: true }).fill('2')
  await set.getByLabel('Kill value').fill('70')
  await set.getByRole('checkbox', { name: /Attack even when the best score is 0/ }).check()
  await saved(page)
  await set.getByRole('combobox', { name: 'Add a unit type' }).selectOption({ index: 1 })
  await saved(page)
  const written = rules(id).ai.weights.classic
  expect(written).toMatchObject({ dmg: 3, counter: 2, killValue: 70, attackAtZero: true })
  expect(Object.keys(written.priority)).toHaveLength(1)
  await set.getByLabel('Damage', { exact: true }).fill('1')  // the default: the key goes
  await saved(page)
  expect('dmg' in rules(id).ai.weights.classic).toBe(false)
  await set.getByLabel('Damage', { exact: true }).fill('3')
  await saved(page)

  // ===== A computer unit =====
  await openMap(page, id, 'ford')
  await tool(page, 'Select').click()
  await clickCell(page, 8, 4) // Gorm, red
  await expect(box(page)).toBeVisible()
  await mode(page).selectOption({ label: 'Walks a route' })
  await expect(box(page).getByTestId('unit-behaviour-route')).toBeVisible()
  await saved(page)
  const first = unit(id, 'Gorm').behaviour
  expect(first.mode).toBe('patrol')
  expect(first.route).toHaveLength(2)
  await box(page).getByRole('button', { name: 'Add a square' }).click()
  await box(page).getByLabel('Square 3: column').fill('2')
  await box(page).getByLabel('Square 3: row').fill('2')
  await box(page).getByRole('combobox', { name: 'Chooses its attacks by' }).selectOption({ label: 'classic' })
  await box(page).getByRole('combobox', { name: 'When walking toward the enemy' }).selectOption({ label: 'Goes straight at them' })
  await box(page).getByLabel('Acts in this order').fill('-2')
  await box(page).getByLabel('Does not walk for its first turns').fill('2')
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toMatchObject({ mode: 'patrol', weights: 'classic', caution: 'none', order: -2, idleTurns: 2 })
  expect(unit(id, 'Gorm').behaviour.route[2]).toEqual({ x: 1, y: 1 })
  // Retreat.
  await box(page).getByRole('checkbox', { name: /Falls back to heal when hurt/ }).check()
  await saved(page)
  expect(unit(id, 'Gorm').behaviour.retreat).toEqual({ below: 30, until: 80 })
  await box(page).getByLabel('Comes back at (% of its hit points)').fill('90')
  await box(page).getByRole('checkbox', { name: /Drinks a healing item first/ }).uncheck()
  await box(page).getByTestId('unit-behaviour-retreat').getByRole('checkbox').nth(1).check() // the first terrain
  await saved(page)
  const retreat = unit(id, 'Gorm').behaviour.retreat
  expect(retreat).toMatchObject({ below: 30, until: 90, useItems: false })
  expect(retreat.to.terrain).toHaveLength(1)
  expect(await game.problems(id)).toEqual([])
  // A healer that serves only retreating allies ("heals"): written only when chosen, and the loader accepts it.
  await box(page).getByRole('combobox', { name: 'Heals with its healing staves' }).selectOption({ label: 'Only allies that are falling back' })
  await saved(page)
  expect(unit(id, 'Gorm').behaviour.heals).toBe('retreating')
  expect(await game.problems(id)).toEqual([])
  await box(page).getByRole('combobox', { name: 'Heals with its healing staves' }).selectOption({ label: 'Any hurt ally' })
  await saved(page)
  expect('heals' in unit(id, 'Gorm').behaviour).toBe(false)

  // A bad value is said in plain words; the weight set it uses cannot be removed without a warning.
  await box(page).getByLabel('Acts in this order').fill('')
  await box(page).getByRole('combobox', { name: 'Chooses its attacks by' }).selectOption({ label: 'The usual scoring' })
  await saved(page)
  expect('weights' in unit(id, 'Gorm').behaviour).toBe(false)
  expect('order' in unit(id, 'Gorm').behaviour).toBe(false)

  // Back to advancing: the route goes, the other keys stay.
  await mode(page).selectOption({ label: 'Advances and attacks' })
  await saved(page)
  const back = unit(id, 'Gorm').behaviour
  expect('route' in back).toBe(false)
  expect(back).toMatchObject({ caution: 'none', idleTurns: 2 })
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the "Change units’ behaviour" command has the same fields', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Scripted command')
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'turncoat')
  await addCommand(page, 'Then', 'Change units’ behaviour')
  const form = page.getByTestId('set-behaviour')
  await form.getByRole('combobox', { name: 'What it does' }).selectOption({ label: 'Waits until someone comes near or into an area' })
  await form.getByLabel('Or within this many squares').fill('4')
  await form.getByRole('combobox', { name: 'Distance counted in' }).selectOption({ label: 'Walking cost' })
  await form.getByLabel('Acts in this order').fill('5')
  await saved(page)
  const ev = readDisk(id, 'maps/ford.json').events.find((e: any) => e.id === 'turncoat')
  expect(ev.do[0].setBehaviour).toEqual({ mode: 'guardArea', within: 4, measure: 'moveCost', order: 5 })
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the Computer player card and the new fields fit a 360 px phone', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Scripted phone')
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  doc.game.rules = { ...doc.game.rules, ai: { turnOrder: 'order', weights: { classic: { dmg: 3, counter: 2, priority: { [doc.units[0].id]: 4 } } } } }
  const ford = doc.maps.ford
  ford.units = ford.units.map((u: any) => (u.name === 'Gorm'
    ? { ...u, behaviour: { mode: 'patrol', route: [{ x: 3, y: 2 }, { x: 5, y: 2 }, { x: 5, y: 4 }], weights: 'classic', caution: 'none', order: 2, idleTurns: 2, retreat: { below: 30, until: 80 } } }
    : u))
  const res = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game, maps: doc.maps } })
  expect(res.ok(), await res.text()).toBeTruthy()
  expect(await game.problems(id)).toEqual([])

  await page.setViewportSize({ width: 360, height: 800 })
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByTestId('ai-rules')).toBeVisible()
  await page.getByTestId('weight-card-classic').getByText('Set “classic”').click()
  await expect(page.getByTestId('weight-set-classic')).toBeVisible()
  await expectNoHScroll(page)
  await page.getByTestId('ai-rules').scrollIntoViewIfNeeded()
  await gotoSection(page, 'Maps')
  await openMap(page, id, 'ford')
  await tab(page, 'Tool').click()
  await tool(page, 'Select').click()
  await clickCell(page, 8, 4)
  await expect(box(page)).toBeVisible()
  await expect(box(page).getByTestId('unit-behaviour-route')).toBeVisible()
  await expect(box(page).getByTestId('unit-behaviour-retreat')).toBeVisible()
  await expectNoHScroll(page)
})

test('@smoke exits, escape, loot and "never attacks": the Map tab and the behaviour box write what docs/AI.md says and the loader accepts it', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Escape')
  await openMap(page, id, 'ford')
  const disk = () => readDisk(id, 'maps/ford.json')
  // --- The Map tab's Exits: a row of column / row boxes, counted from 1; the last one removed takes the key with it.
  await tab(page, 'Map').click()
  const exits = page.getByTestId('map-exits')
  expect('exits' in disk()).toBe(false)
  await exits.getByRole('button', { name: 'Add an exit' }).click()
  await saved(page)
  expect(disk().exits).toEqual([{ x: 0, y: 0 }])
  await exits.getByLabel('Exit 1: column').fill('3')
  await exits.getByLabel('Exit 1: row').fill('2')
  await exits.getByRole('button', { name: 'Add an exit' }).click()
  await saved(page)
  expect(disk().exits).toEqual([{ x: 2, y: 1 }, { x: 0, y: 0 }])
  expect(await game.problems(id)).toEqual([])
  await exits.getByRole('button', { name: 'Remove exit 2' }).click()
  await saved(page)
  expect(disk().exits).toEqual([{ x: 2, y: 1 }])

  // --- The behaviour box: Escapes (a bare name), Never attacks, Loots (a bare name).
  await tab(page, 'Tool').click()
  await tool(page, 'Select').click()
  await clickCell(page, 8, 4) // Gorm, red
  await mode(page).selectOption({ label: 'Escapes' })
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toBe('escape')
  expect(await game.problems(id)).toEqual([])
  await box(page).getByRole('checkbox', { name: /Never attacks/ }).check()
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toEqual({ mode: 'escape', attacks: false })
  expect(await game.problems(id)).toEqual([])
  await mode(page).selectOption({ label: 'Loots, then leaves' })
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toEqual({ mode: 'loot', attacks: false })  // the loader wants "goes for" with it: the map's Warnings say so (unit test)
  await box(page).getByRole('checkbox', { name: /Never attacks/ }).uncheck()
  await mode(page).selectOption({ label: 'Advances and attacks' })
  await saved(page)
  expect('behaviour' in unit(id, 'Gorm')).toBe(false)

  // --- The last exit removed takes the key with it.
  await tab(page, 'Map').click()
  await exits.getByRole('button', { name: 'Remove exit 1' }).click()
  await saved(page)
  expect('exits' in disk()).toBe(false)
})

test('@smoke the Rules screen: "When a unit is defeated" writes drops: last and the loader accepts it', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Drops')
  await page.goto(editorUrl(id, 'rules'))
  const drops = page.getByRole('combobox', { name: 'When a unit is defeated' })
  await expect(drops).toHaveValue('marked')
  await drops.selectOption({ label: 'Drops only its last item' })
  await saved(page)
  expect(rules(id).items.drops).toBe('last')
  expect(await game.problems(id)).toEqual([])
  await drops.selectOption({ label: 'Drops every marked item' })
  await saved(page)
  expect(rules(id).items?.drops).toBeUndefined()
  await expectNoHScroll(page)
})
