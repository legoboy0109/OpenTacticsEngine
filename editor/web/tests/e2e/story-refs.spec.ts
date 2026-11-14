// What the story holds about other things: renaming a unit's story name rewrites the events, deleting a unit or a
// switch asks first with a "used by" list and cleans up in ONE undo step, and the in-browser checks name what the
// loader would refuse. Also reordering events and commands.
import type { APIRequestContext } from '@playwright/test'
import { expect, readDisk, saved, test } from './fixtures'
import { clickCell, openMap, tool } from './mapkit'
import { addCommand, editor, eventsTab, openEvent } from './storykit'

const MAP = 'maps/crossroads.json'

/** Writes a story into the map (and names two units) through the API, like a second author would. */
async function seedStory(request: APIRequestContext, id: string, story: { events?: unknown[]; objectives?: unknown; switches?: unknown[] }): Promise<void> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const m = doc.maps.crossroads
  m.units[0].id = 'eirik'
  m.units[3].id = 'gorm'
  if (story.events) m.events = story.events
  if (story.objectives) m.objectives = story.objectives
  const game = doc.game
  if (story.switches) game.switches = story.switches
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game, maps: { crossroads: m } } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

const STORY = {
  switches: [{ id: 'village_saved', name: 'Village saved' }],
  events: [
    { id: 'ambush', when: { defeated: { unit: 'gorm' } }, do: [{ say: 'Gorm is down', speaker: 'gorm' }, { setSwitch: 'village_saved', to: true }, { say: 'Done' }] },
    { id: 'chat', when: { talk: 'eirik', to: 'gorm' }, do: [{ say: 'Hello', speaker: 'eirik' }] },
    { id: 'later', when: { battle: 'won' }, if: [{ switch: 'village_saved' }], do: [{ say: 'Won' }] },
  ],
  objectives: { text: 'Defeat Gorm', win: [{ defeated: { unit: 'gorm' } }, { round: 9 }] },
}

test('@smoke renaming a story name rewrites the events; deleting the unit asks, lists what goes and is one undo step', async ({ page, game, request }) => {
  test.setTimeout(90_000)
  const id = await game.create('economy-tactics', 'Refs')
  await seedStory(request, id, STORY)
  const before = readDisk(id, MAP)
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await clickCell(page, 9, 2)
  const props = page.getByTestId('unit-props')
  const field = props.getByLabel('Story name')
  await expect(field).toHaveValue('gorm')

  // A name that cannot be used is explained and never reaches the file.
  await field.fill('bad!')
  await expect(props.getByRole('alert')).toContainText('letters, digits, _ and -')
  await field.fill('eirik')
  await expect(props.getByRole('alert')).toContainText('already has this name')
  await field.fill('')
  await expect(props.getByRole('alert')).toContainText('Events or objectives point at this name')
  await saved(page)
  expect(readDisk(id, MAP).units[3].id).toBe('gorm')

  // A valid new name: every place that named the old one follows (spaces become _ as typed).
  await field.fill('gorm the great')
  await expect(field).toHaveValue('gorm_the_great')
  await saved(page)
  const renamed = JSON.stringify(readDisk(id, MAP))
  expect(renamed).not.toContain('"gorm"')
  expect(readDisk(id, MAP).events[0].when).toEqual({ defeated: { unit: 'gorm_the_great' } })
  expect(readDisk(id, MAP).events[1].when).toEqual({ talk: 'eirik', to: 'gorm_the_great' })
  expect(readDisk(id, MAP).objectives.win[0]).toEqual({ defeated: { unit: 'gorm_the_great' } })
  expect(await game.problems(id)).toEqual([])
  // Undo puts the old name back everywhere in one step.
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, MAP)).toEqual(before)
  await page.getByRole('button', { name: 'Redo' }).click()
  await saved(page)

  // Delete: it asks, and says exactly what is removed.
  await page.getByRole('button', { name: 'Remove unit' }).click()
  const dlg = page.getByRole('dialog', { name: /Delete the unit/ })
  await expect(dlg).toContainText('event “ambush” is removed')
  await expect(dlg).toContainText('event “chat” is removed')
  await expect(dlg).toContainText('“Infantry is defeated” is removed')
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  expect(readDisk(id, MAP).units.some((u: any) => u.id === 'gorm_the_great')).toBe(true)
  await page.getByRole('button', { name: 'Remove unit' }).click()
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  const after = readDisk(id, MAP)
  expect(after.units.some((u: any) => u.id === 'gorm_the_great')).toBe(false)
  expect(after.events.map((e: any) => e.id)).toEqual(['later'])
  expect(after.objectives).toEqual({ text: 'Defeat Gorm', win: [{ round: 9 }] })
  expect(JSON.stringify(after)).not.toContain('gorm')
  expect(await game.problems(id)).toEqual([])

  // ONE undo brings back the unit, the events and the objective.
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  const restored = readDisk(id, MAP)
  expect(restored.units.some((u: any) => u.id === 'gorm_the_great')).toBe(true)
  expect(restored.events.map((e: any) => e.id)).toEqual(['ambush', 'chat', 'later'])
  expect(restored.objectives.win).toHaveLength(2)
})

test('deleting a switch that events use asks first, lists them, cleans up and undoes in one step', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'SwitchDel')
  await seedStory(request, id, STORY)
  await page.goto(`/p/${id}/rules`)
  await page.getByRole('button', { name: 'Delete Village saved' }).click()
  const dlg = page.getByRole('dialog', { name: /Delete the switch/ })
  await expect(dlg).toContainText('event “later” is removed')
  await expect(dlg).toContainText('“Turns the switch “Village saved” on” is removed')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  const m = readDisk(id, MAP)
  expect(m.events.map((e: any) => e.id)).toEqual(['ambush', 'chat'])
  expect(m.events[0].do).toEqual([{ say: 'Gorm is down', speaker: 'gorm' }, { say: 'Done' }])
  expect(readDisk(id, 'game.json')).not.toHaveProperty('switches')
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, MAP).events).toHaveLength(3)
  expect(readDisk(id, 'game.json').switches).toHaveLength(1)
})

test('the checks name what the loader would refuse, in the amber style', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Checks')
  await seedStory(request, id, {
    events: [
      { id: 'empty', when: { battle: 'start' }, do: [{ say: '' }] },
      { id: 'ghost', when: { talk: 'eirik', to: 'nobody' }, do: [{ endTurn: true }, { moveUnit: 'eirik', x: 3, y: 3 }] },
      { id: 'early', when: { battle: 'start' }, do: [{ endTurn: true }, { setSwitch: 'gone', to: true }] },
    ],
    objectives: { win: [{ reach: { x: 40, y: 0, w: 2, h: 1 } }] },
  })
  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  const checks = page.getByTestId('story-checks')
  await expect(checks).toContainText('Event “empty”, command 1: the text to say is empty.')
  await expect(checks).toContainText('Event “ghost”, “When”: “nobody” is not a named unit of this map')
  await expect(checks).toContainText('Event “early”, command 1: “End the turn” only works')
  await expect(checks).toContainText('Event “early”, command 2: “gone” is not a switch of this game.')
  await expect(checks).toContainText('The objectives, “Win” entry 1: an area does not fit inside the map.')
  // The same list sits above the map, and the loader agrees that the game cannot load.
  await expect(page.getByTestId('map-warnings')).toContainText('the text to say is empty')
  expect((await game.problems(id)).some((p) => p.file === MAP)).toBe(true)

  // Fixing the first mistake clears its line.
  await openEvent(page, 'empty')
  await expect(page.getByTestId('event-checks')).toContainText('the text to say is empty')
  await editor(page).locator('.command-sentence').first().click()
  await editor(page).getByLabel('What is said').fill('Hello')
  await expect(page.getByTestId('event-checks')).toHaveCount(0)
  await expect(page.getByTestId('map-warnings')).not.toContainText('the text to say is empty')
})

test('events and commands can be reordered and deleted with the buttons', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Order')
  await seedStory(request, id, { ...STORY })
  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Move event later up' }).click()
  await page.getByRole('button', { name: 'Move event later up' }).click()
  await saved(page)
  expect(readDisk(id, MAP).events.map((e: any) => e.id)).toEqual(['later', 'ambush', 'chat'])
  await page.getByRole('button', { name: 'Delete event chat' }).click()
  await openEvent(page, 'ambush')
  await addCommand(page, 'Then', 'Say something')
  await editor(page).getByLabel('What is said').fill('Last')
  await editor(page).getByRole('button', { name: 'Move Then command 4 up' }).click()
  await editor(page).getByRole('button', { name: 'Move Then command 3 up' }).click()
  await editor(page).getByRole('button', { name: 'Delete Then command 1' }).click()
  await saved(page)
  expect(readDisk(id, MAP).events.map((e: any) => e.id)).toEqual(['later', 'ambush'])
  expect(readDisk(id, MAP).events[1].do).toEqual([{ say: 'Last' }, { setSwitch: 'village_saved', to: true }, { say: 'Done' }])
  expect(await game.problems(id)).toEqual([])
})
