// Four small additions through the real editor (docs/EDITOR_UI.md "Small extras"): a condition whose effects fade as it runs out (Conditions screen),
// a link between two strike blocks (Skills: the effect "Leaves a mark for this fight" and the condition "Has a mark from this fight"), the event
// condition "The unit that set this off" and the command "Send a unit's items to storage" (Events). The proof that matters: the exact JSON the forms
// wrote, that the real loader accepts the game (/problems is empty), the picks the loader would refuse cannot be made, and the 360 px phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'
import { newSkill, openSkills, part, rule } from './skillskit'
import { addCommand, editor, eventsTab, form, newEvent, when } from './storykit'

const conds = (id: string) => readDisk(id, 'data/conditions.json').conditions as any[]
const skills = (id: string) => readDisk(id, 'data/skills.json').skills as any[]
const events = (id: string) => readDisk(id, 'maps/ford.json').events as any[]

/** A strategy-RPG game with no starter skills / conditions, and a shared storage (the storeItems command needs one). */
async function prepare(page: Page, id: string, storage: 'anywhere' | 'off' = 'anywhere'): Promise<void> {
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  const game = JSON.parse(JSON.stringify(doc.game))
  game.rules = game.rules ?? {}
  game.rules.items = { ...(game.rules.items ?? {}), storage }
  const items = [...(doc.items ?? []), { id: 'rope', name: 'Rope', kind: 'usable', heal: 3, uses: 2 }]
  const res = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game, items, conditions: [] } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

async function newCondition(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New condition' }).click()
  const dlg = page.getByRole('dialog', { name: 'New condition' })
  await dlg.getByLabel('Name of the condition').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

const fade = (page: Page) => page.getByRole('checkbox', { name: 'Its effects fade as it runs out' })

async function buildCondition(page: Page, id: string): Promise<void> {
  await page.goto(editorUrl(id, 'conditions'))
  await expect(page.getByRole('heading', { name: 'Conditions', level: 1 })).toBeVisible()
  await newCondition(page, 'Torch')
  await saved(page)
  expect(conds(id)[0].scale).toBeUndefined()
  await fade(page).check()
  await saved(page)
  expect(conds(id)[0]).toMatchObject({ id: 'torch', turns: 3, scale: 'turnsLeft' })
  await expect(page.getByTestId('condition-summary')).toContainText('as many times as it has turns left')
  expect(await game_problems(page, id)).toEqual([])
  // A condition that never counts turns cannot fade: the box goes away and the key with it.
  await page.getByLabel('It lasts', { exact: true }).selectOption({ label: 'Until it is cured' })
  await saved(page)
  expect(conds(id)[0].scale).toBeUndefined()
  expect(conds(id)[0].until).toBe('cured')
  await expect(fade(page)).toHaveCount(0)
  await page.getByLabel('It lasts', { exact: true }).selectOption({ label: 'A number of the unit’s own turns' })
  await fade(page).check()
  await page.getByLabel('Number of turns', { exact: true }).fill('4')
  await saved(page)
  expect(conds(id)[0]).toMatchObject({ turns: 4, scale: 'turnsLeft' })
  // With no rule that has a number to fade the form says so (and the loader would refuse it: the problems list names it).
  await expect(fade(page)).toBeChecked()
}

async function game_problems(page: Page, id: string): Promise<string[]> {
  const res = await page.request.get(`/api/projects/${id}/problems`)
  return ((await res.json()).problems as Array<{ message: string }>).map((p) => p.message)
}

async function buildSkill(page: Page, id: string): Promise<void> {
  await openSkills(page, id)
  await newSkill(page, 'Linked')
  // Rule 1: the first strike, with a chance: +3 damage and a mark.
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('strike')
  await r1.getByLabel('Add a condition…').selectOption('strike')
  await r1.getByLabel('Add an effect…').selectOption('mark')
  await expect(part(r1, 'Effect 2').getByLabel('Name of the mark')).toHaveValue('mark')
  await part(r1, 'Effect 2').getByLabel('Name of the mark').fill('hit')
  await saved(page)
  await expect(r1.getByTestId('skill-rule-summary')).toContainText('leaves the mark “hit”')
  // Rule 2: the second strike, only when marked.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('strike')
  await r2.getByLabel('Add a condition…').selectOption('marked')
  await part(r2, 'Condition 1').getByLabel('Name of the mark').fill('hit')
  await saved(page)
  await expect(r2.getByTestId('skill-rule-summary')).toContainText('it has the mark “hit”')
  // Rule 3: after combat, the mark can be asked the other way round.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r3 = rule(page, 3)
  await r3.getByLabel('When', { exact: true }).selectOption('afterCombat')
  await r3.getByLabel('Add a condition…').selectOption('marked')
  await part(r3, 'Condition 1').getByLabel('Name of the mark').fill('hit')
  await part(r3, 'Condition 1').getByLabel('The unit').selectOption('no')
  await saved(page)
  // Where the loader would refuse them the pickers do not offer them.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r4 = rule(page, 4)
  await r4.getByLabel('When', { exact: true }).selectOption('combat')
  await expect(r4.getByLabel('Add a condition…').locator('option', { hasText: 'Has a mark from this fight' })).toHaveCount(0)
  await expect(r4.getByLabel('Add an effect…').locator('option', { hasText: 'Leaves a mark for this fight' })).toHaveCount(0)
}

test('@smoke the fading condition, linked strike blocks, the actor condition and storeItems: the files are exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(180_000)
  const id = await game.create('strategy-rpg', 'Extras', { srpgContent: true })
  await prepare(page, id)

  await buildCondition(page, id)

  await buildSkill(page, id)
  const linked = skills(id).find((s) => s.id === 'linked')
  expect(linked.blocks.slice(0, 3)).toEqual([
    { when: 'strike', if: [{ strike: 'first' }], do: [{ hit: 10 }, { mark: 'hit' }] },
    { when: 'strike', if: [{ marked: 'hit' }], do: [{ damage: 3 }] },
    { when: 'afterCombat', if: [{ marked: 'hit', is: false }], do: [{ heal: 5 }] },
  ])

  // Events: a reach event that branches on who arrived, and sends the items of one unit to the storage.
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'arrive')
  await when(page, 'A unit reaches an area')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'The unit that set this off' })
  const cond = editor(page).getByRole('group', { name: 'Only if: Condition 1' })
  await expect(cond.getByLabel('Which unit', { exact: true })).toBeVisible()
  await addCommand(page, 'Then', 'Send a unit’s items to storage')
  await form(page).getByRole('checkbox', { name: 'Rope' }).check()
  await saved(page)
  const ev = events(id).find((e) => e.id === 'arrive')
  expect(ev.if).toHaveLength(1)
  expect(Object.keys(ev.if[0])).toEqual(['actor'])
  expect(typeof ev.if[0].actor).toBe('object')
  expect(ev.do[0]).toMatchObject({ keep: ['rope'] })
  expect(ev.do[0].storeItems).toBeDefined()
  expect(await game_problems(page, id)).toEqual([])
  // A turn event with an actor test: nobody sets it off, and the check says so.
  await newEvent(page, 'turntwo')
  await when(page, 'A particular turn')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'The unit that set this off' })
  await saved(page)
  await page.getByRole('button', { name: 'All events' }).click()
  await expect(page.getByTestId('story-checks')).toContainText('never true')
})

test('the storage entry is greyed in a game with no shared storage, and the check names it when it is there anyway', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Extras no storage', { srpgContent: true })
  await prepare(page, id, 'off')
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'arrive')
  const add = editor(page).getByLabel('Then: Add a command…')
  await expect(add.locator('option', { hasText: 'Send a unit’s items to storage (no shared storage)' })).toBeDisabled()
  // Written by hand into the file anyway: the editor flags it and the real loader refuses it in plain words.
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  doc.maps.ford.events = [{ id: 'oops', when: { battle: 'start' }, do: [{ storeItems: {} }] }]
  const res = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: { ford: doc.maps.ford } } })
  expect(res.ok()).toBeTruthy()
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await expect(page.getByTestId('story-checks')).toContainText('no shared storage')
  expect((await game_problems(page, id)).join(' | ')).toContain('"storeItems" needs a shared storage')
})

test('@smoke on a 360 px phone the four additions fit without sideways scrolling', async ({ page, game }) => {
  test.setTimeout(180_000)
  const id = await game.create('strategy-rpg', 'Extras phone', { srpgContent: true })
  await prepare(page, id)
  await page.setViewportSize({ width: 360, height: 800 })
  await page.goto(editorUrl(id, 'conditions'))
  await newCondition(page, 'Torch')
  await fade(page).check()
  await saved(page)
  await expectNoHScroll(page)
  await gotoSection(page, 'Skills')
  await newSkill(page, 'Linked')
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('strike')
  await r1.getByLabel('Add an effect…').selectOption('mark')
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('strike')
  await r2.getByLabel('Add a condition…').selectOption('marked')
  await saved(page)
  await expectNoHScroll(page)
  await gotoSection(page, 'Maps')
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'arrive')
  await when(page, 'A unit reaches an area')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'The unit that set this off' })
  await addCommand(page, 'Then', 'Send a unit’s items to storage')
  await saved(page)
  await expectNoHScroll(page)
  expect(events(id).find((e) => e.id === 'arrive').do[0].storeItems).toBeDefined()
  expect(skills(id).find((s) => s.id === 'linked').blocks[1].if).toEqual([{ marked: 'mark' }])
  expect(conds(id)[0].scale).toBe('turnsLeft')
  expect(await game_problems(page, id)).toEqual([])
})
