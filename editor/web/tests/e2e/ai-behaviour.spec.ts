// The computer unit's behaviour through the editor's own forms (docs/AI.md, docs/EDITOR_UI.md "Behaviour"): the "Behaviour" group of a computer unit's panel,
// the same control in the "Add a unit" command, and the commands "Change units’ behaviour" and "Wake a group". The proof that matters: the exact JSON the forms
// wrote, that the real loader accepts it (/problems is empty), the map checks that say what is probably not meant, and the 360 px phone layout.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'
import { clickCell, openMap, tab, tool } from './mapkit'
import { addCommand, editor, eventsTab, form, newEvent, shot } from './storykit'

test.use({ actionTimeout: 10_000 })

const unit = (id: string, name: string) => readDisk(id, 'maps/ford.json').units.find((u: any) => u.name === name)
const box = (page: Page) => page.getByTestId('unit-behaviour')
const mode = (page: Page) => box(page).getByRole('combobox', { name: 'What it does' })

test('@smoke set behaviours through the unit panel and the story commands: the files are what docs/AI.md says and the loader accepts them', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Behaviour')
  await openMap(page, id, 'ford')
  await tool(page, 'Select').click()

  // A unit of a side a person plays has no behaviour control; a computer unit has.
  await clickCell(page, 1, 3) // Eirik, blue
  await expect(page.getByTestId('unit-props')).toBeVisible()
  await expect(box(page)).toHaveCount(0)
  await clickCell(page, 8, 4) // Gorm, red
  await expect(box(page)).toBeVisible()
  await expect(mode(page)).toHaveValue('advance')
  expect(unit(id, 'Gorm').behaviour).toBeUndefined()

  // "Stays where it is" is written as the bare name; going back writes nothing.
  await mode(page).selectOption({ label: 'Stays where it is' })
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toBe('hold')
  await mode(page).selectOption({ label: 'Advances and attacks' })
  await saved(page)
  expect('behaviour' in unit(id, 'Gorm')).toBe(false)

  // A guard with a group and a start turn.
  await mode(page).selectOption({ label: 'Waits until someone comes into its range' })
  await box(page).getByLabel('Wakes with its group').fill('pack')
  await box(page).getByLabel('Does nothing before turn').fill('3')
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toEqual({ mode: 'guard', group: 'pack', fromTurn: 3 })
  await box(page).getByRole('checkbox', { name: /Never goes after anyone/ }).check()
  await saved(page)
  expect(unit(id, 'Gorm').behaviour).toEqual({ mode: 'guard', wake: 'react', group: 'pack', fromTurn: 3 })
  // A group of one is worth a look.
  await expect(page.getByTestId('map-warnings')).toContainText('only unit in the group “pack”')

  // The second unit joins the group (the box suggests the groups of the map); the warning goes.
  await clickCell(page, 8, 3) // Vex, red
  await mode(page).selectOption({ label: 'Waits until someone comes near or into an area' })
  await expect(page.locator('datalist option[value="pack"]')).toHaveCount(1)
  await box(page).getByLabel('Wakes with its group').fill('pack')
  await box(page).getByLabel('Or within this many squares').fill('4')
  await saved(page)
  expect(unit(id, 'Vex').behaviour).toEqual({ mode: 'guardArea', within: 4, group: 'pack' })
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
  // The area.
  await box(page).getByRole('button', { name: 'Add an area' }).click()
  await saved(page)
  expect(unit(id, 'Vex').behaviour.area).toEqual({ x: 0, y: 0, w: 3, h: 3 })
  await box(page).getByTestId('unit-behaviour-area').getByLabel('Left').fill('5')
  await saved(page)
  expect(unit(id, 'Vex').behaviour.area.x).toBe(4)

  // Heads for a square, with a favourite target.
  await mode(page).selectOption({ label: 'Heads for…' })
  await expect(box(page).getByRole('combobox', { name: 'Heads for' })).toHaveValue('square')
  await box(page).getByRole('combobox', { name: 'Favourite target' }).selectOption({ label: 'The most hurt' })
  await saved(page)
  const seek = unit(id, 'Vex').behaviour
  expect(seek).toMatchObject({ mode: 'seek', prefer: 'weakest' })
  await shot(page, 'desktop-behaviour-panel')
  expect(typeof seek.x === 'number' && typeof seek.y === 'number').toBe(true)
  expect('group' in seek || 'within' in seek).toBe(false) // the keys of the old mode went with it

  // ===== The story commands =====
  await eventsTab(page).click()
  await newEvent(page, 'ambush')
  await addCommand(page, 'Then', 'Change units’ behaviour')
  await expect(page.getByTestId('set-behaviour-form')).toBeVisible()
  await page.getByTestId('set-behaviour').getByRole('combobox', { name: 'What it does' }).selectOption({ label: 'Waits until someone comes into its range' })
  await addCommand(page, 'Then', 'Wake a group')
  await expect(page.getByTestId('wake-group-form').getByRole('combobox')).toHaveValue('pack')
  await addCommand(page, 'Then', 'Add a unit')
  const add = page.getByTestId('add-unit-form')
  await expect(add.getByTestId('add-unit-behaviour')).toHaveCount(0) // the first side is a person's
  await add.getByLabel('Side', { exact: true }).selectOption({ label: 'Red Warband' })
  await add.getByTestId('add-unit-behaviour-fields').getByRole('combobox', { name: 'What it does' }).selectOption({ label: 'Stays where it is' })
  await saved(page)
  const ev = readDisk(id, 'maps/ford.json').events.find((e: any) => e.id === 'ambush')
  expect(ev.do[0]).toEqual({ setBehaviour: 'guard', units: { side: expect.any(String) } })
  expect(ev.do[1]).toEqual({ wakeGroup: 'pack' })
  expect(ev.do[2].addUnit.behaviour).toBe('hold')
  await expect(editor(page).locator('.command-list li').first()).toContainText('Tells')

  // ===== The files and the loader =====
  await shot(page, 'desktop-behaviour-event')
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the behaviour control and the new commands fit a 360 px phone', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Behaviour phone')
  // Written through the API like a second author would; this test judges the layout, not the clicking.
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  const ford = doc.maps.ford
  ford.units = ford.units.map((u: any) => (u.name === 'Gorm' ? { ...u, behaviour: { mode: 'guardArea', area: { x: 3, y: 2, w: 3, h: 3 }, within: 4, group: 'pack', fromTurn: 2, prefer: 'weakest' } } : u))
  ford.events = [{ id: 'ambush', when: { turn: 2 }, do: [{ setBehaviour: { mode: 'seek', x: 2, y: 2 }, units: { side: 'red' } }, { wakeGroup: 'pack' }] }]
  const res = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })
  expect(res.ok(), await res.text()).toBeTruthy()
  expect(await game.problems(id)).toEqual([])

  await page.setViewportSize({ width: 360, height: 800 })
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Maps')
  await openMap(page, id, 'ford')
  await expectNoHScroll(page)
  await tab(page, 'Tool').click()
  await tool(page, 'Select').click()
  await clickCell(page, 8, 4)
  await expect(box(page)).toBeVisible()
  await expectNoHScroll(page)
  await box(page).scrollIntoViewIfNeeded()
  await shot(page, 'phone-behaviour-panel')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Open event ambush', exact: true }).click()
  await expect(editor(page)).toBeVisible()
  await expectNoHScroll(page)
  await page.getByTestId('command-setBehaviour').click()
  await expect(page.getByTestId('set-behaviour-form')).toBeVisible()
  await expectNoHScroll(page)
  await form(page).first().scrollIntoViewIfNeeded()
  await shot(page, 'phone-behaviour-event')
})
