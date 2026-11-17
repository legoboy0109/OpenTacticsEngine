// The Conditions screen and the places that use a condition (docs/SRPG.md section 9, docs/EDITOR_UI.md "Conditions"), through the real editor. The proof that
// matters: the exact JSON the forms wrote, that the loader accepts the game (/problems is empty) and the real engine loads it (Play), what deleting a used
// condition does, that the choices the loader would refuse cannot be made, and the 360 px phone layout.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, requirePlayer, saved, section, test } from './fixtures'
import { shot, shotEl } from './rankskit'
import { part, rule } from './skillskit'
import { addCommand, createItem, editor, eventsTab, form, newEvent } from './storykit'
import { openMap } from './mapkit'

const conds = (id: string) => readDisk(id, 'data/conditions.json').conditions as any[]
const cond = (id: string, cid: string) => conds(id).find((c) => c.id === cid)
const items = (id: string) => readDisk(id, 'data/items.json').items as any[]
const item = (id: string, iid: string) => items(id).find((i) => i.id === iid)
const weapon = (id: string, wid: string) => (readDisk(id, 'data/weapons.json').weapons as any[]).find((w) => w.id === wid)
const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })

/** The template ships the starter set: these tests start from "conditions switched on, none made yet" (an empty list is written, as a switched-on feature is). */
async function emptyConditions(page: Page, id: string): Promise<void> {
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  const put = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, conditions: [] } })
  expect(put.ok(), await put.text()).toBeTruthy()
}

async function newCondition(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New condition' }).click()
  const dlg = page.getByRole('dialog', { name: 'New condition' })
  await dlg.getByLabel('Name of the condition').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

async function newStaff(page: Page, name: string): Promise<void> {
  await createItem(page, name)
  await page.getByLabel('Kind', { exact: true }).selectOption('staff')
  await expect(page.getByTestId('staff-use')).toBeVisible()
}

const sw = (page: Page, name: string) => page.getByRole('switch', { name })
const check = (page: Page, name: string) => page.getByRole('checkbox', { name, exact: false })

test('@smoke a condition through the form, the starter set, an inflict staff, a cure item and a poison weapon; the loader and the engine accept it; deleting a used condition follows through', async ({ page, game }) => {
  requirePlayer()
  const id = await game.create('strategy-rpg', 'Conditions', { srpgContent: true })
  await emptyConditions(page, id)
  await page.goto(editorUrl(id, 'conditions'))
  await expect(page.getByRole('heading', { name: 'Conditions', level: 1 })).toBeVisible()
  await expect(section(page, 'Conditions')).toBeVisible()
  await expect(page.getByText('No conditions yet').first()).toBeVisible()
  await shot(page, 'desktop-conditions-empty')

  // A new condition lasts 3 turns and lowers a stat: valid as it stands. Then the form: tag, turns, wearing off, ends when struck, cannot move, a rule.
  await newCondition(page, 'Dizzy')
  await saved(page)
  expect(conds(id)).toEqual([{ id: 'dizzy', name: 'Dizzy', turns: 3, blocks: [{ when: 'always', do: [{ stats: { str: -1 } }] }] }])
  expect(await game.problems(id)).toEqual([])
  await page.getByLabel('Tag on the map').fill('Dz')
  await page.getByLabel('Number of turns', { exact: true }).fill('2')
  await check(page, 'It can wear off early').check()
  await page.getByLabel('Chance each turn that it wears off').fill('30')
  await check(page, 'It ends when the unit is struck').check()
  await sw(page, 'The unit cannot move').check()
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('turnStart')
  const fx = part(r2, 'Effect 1')
  await fx.getByLabel('Effect 1', { exact: true }).selectOption('hurt')
  await fx.getByLabel('Who is hurt').selectOption('self')
  await fx.getByLabel('Hurts', { exact: true }).selectOption('percent')
  await fx.getByLabel('Amount').fill('10')
  await saved(page)
  expect(cond(id, 'dizzy')).toEqual({
    id: 'dizzy', name: 'Dizzy', tag: 'Dz', turns: 2, wearOff: 30, endsWhenStruck: true,
    blocks: [
      { when: 'always', do: [{ stats: { str: -1 } }] },
      { when: 'always', do: [{ cannotMove: true }] },
      { when: 'turnStart', do: [{ hurt: { percent: 10 }, to: 'self' }] },
    ],
  })
  await expect(page.getByTestId('condition-summary')).toContainText('Lasts 2 of the unit’s own turns. Each turn there is a 30% chance it wears off early. It ends when the unit is struck.')
  await expect(page.getByTestId('condition-summary')).toContainText('loses 10% of its maximum HP (never below 1)')
  await shot(page, 'desktop-condition-form')
  expect(await game.problems(id)).toEqual([])

  // "It can defeat the unit" turns the hurt into the object form.
  await fx.getByRole('checkbox', { name: 'It can defeat the unit' }).check()
  await saved(page)
  expect(cond(id, 'dizzy').blocks[2].do).toEqual([{ hurt: { percent: 10, canKill: true }, to: 'self' }])
  await fx.getByRole('checkbox', { name: 'It can defeat the unit' }).uncheck()
  await saved(page)
  expect(cond(id, 'dizzy').blocks[2].do).toEqual([{ hurt: { percent: 10 }, to: 'self' }])

  // The starter set: six more in one step, and it can be asked again without doubling anything.
  await page.goto(editorUrl(id, 'conditions'))
  await page.getByTestId('starter-conditions').click()
  await expect(page.getByTestId('starter-conditions-result')).toContainText('Added 6 conditions.')
  await saved(page)
  expect(conds(id).map((c) => c.id)).toEqual(['dizzy', 'poison', 'sleep', 'silence', 'berserk', 'stun', 'curse'])
  await shot(page, 'desktop-conditions-starter-set')
  await page.getByTestId('starter-conditions').click()
  await expect(page.getByTestId('starter-conditions-result')).toContainText('already')
  await saved(page)
  expect(conds(id)).toHaveLength(7)
  // One Undo takes the whole set back out (the second click changed nothing).
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(conds(id).map((c) => c.id)).toEqual(['dizzy'])
  await page.getByTestId('starter-conditions').click()
  await saved(page)
  expect(conds(id)).toHaveLength(7)

  // A staff that inflicts Poison for 4 turns, with a chance of working that reads both units.
  await page.goto(editorUrl(id, 'items'))
  await newStaff(page, 'Venom Staff')
  await check(page, 'It inflicts a condition').check()
  await check(page, 'It heals').uncheck()
  await page.getByLabel('Who it can be used on').selectOption('foe')
  const inflict = page.getByTestId('staff-inflict')
  await inflict.getByLabel('Condition', { exact: true }).selectOption('poison')
  await inflict.getByLabel('Number of turns').fill('4')
  await check(page, 'It can miss').check()
  await expect(page.getByTestId('staff-effect')).toContainText('a is the user and b the unit it is used on')
  await page.getByLabel('Chance of working, in percent').fill('70 + a.skl * 5 - b.def * 5 - dist * 2')
  await saved(page)
  expect(item(id, 'venom_staff')).toMatchObject({ kind: 'staff', target: 'foe', hit: '70 + a.skl * 5 - b.def * 5 - dist * 2', effects: [{ inflict: { condition: 'poison', turns: 4 } }] })
  await shot(page, 'desktop-staff-inflict')

  // The same staff also cures (area is allowed): switching the second part on adds it.
  await check(page, 'It cures').check()
  const cureBox = page.getByTestId('staff-cure')
  // It starts with nothing ticked and says so; nothing is written until one is picked (the loader refuses an empty list).
  await expect(cureBox.getByRole('checkbox', { checked: true })).toHaveCount(0)
  await expect(page.getByTestId('staff-cure-pick')).toContainText('Pick at least one condition')
  expect(item(id, 'venom_staff').effects).toEqual([{ inflict: { condition: 'poison', turns: 4 } }])
  expect(await game.problems(id)).toEqual([])
  await cureBox.getByRole('checkbox', { name: 'Poison' }).check()
  await expect(page.getByTestId('staff-cure-pick')).toHaveCount(0)
  await cureBox.getByRole('checkbox', { name: 'Sleep' }).check()
  await saved(page)
  expect(item(id, 'venom_staff').effects).toEqual([{ inflict: { condition: 'poison', turns: 4 } }, { cure: ['poison', 'sleep'] }])
  await page.getByLabel('What it affects').selectOption('radius')
  await check(page, 'It cures').uncheck()
  await saved(page)
  expect(item(id, 'venom_staff')).toMatchObject({ area: { radius: 1 }, effects: [{ inflict: { condition: 'poison', turns: 4 } }] })
  await shotEl(page.getByTestId('staff-effect'), 'desktop-staff-parts')

  // A cure item: cures Poison and Sleep, heals a little.
  await createItem(page, 'Antidote')
  const conditionsBox = page.getByTestId('item-conditions')
  await conditionsBox.getByLabel('Cures', { exact: true }).selectOption('list')
  await expect(conditionsBox.getByRole('checkbox', { checked: true })).toHaveCount(0) // nothing ticked yet, nothing written
  await expect(page.getByTestId('item-cure-pick')).toContainText('Pick at least one condition')
  await saved(page)
  expect(item(id, 'antidote')).not.toHaveProperty('cure')
  await conditionsBox.getByRole('checkbox', { name: 'Poison' }).check()
  await expect(page.getByTestId('item-cure-pick')).toHaveCount(0)
  await saved(page)
  expect(item(id, 'antidote')).toMatchObject({ kind: 'usable', cure: ['poison'], heal: 10 })
  await check(page, 'It puts a condition on the unit it is used on').check()
  await page.getByTestId('item-inflict').getByLabel('Condition', { exact: true }).selectOption('curse')
  await saved(page)
  expect(item(id, 'antidote').inflict).toEqual({ condition: 'curse' })
  await check(page, 'It puts a condition on the unit it is used on').uncheck()
  await saved(page)
  expect(item(id, 'antidote')).not.toHaveProperty('inflict')
  await shotEl(page.getByTestId('item-conditions'), 'desktop-item-cure')

  // A poison weapon: 30% on a hit, for 2 turns; then a formula.
  await page.goto(editorUrl(id, 'weapons'))
  await page.getByRole('button', { name: 'New weapon' }).click()
  const dlg = page.getByRole('dialog', { name: 'New weapon' })
  await dlg.getByLabel('Name of the weapon').fill('Poison Dagger')
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name: 'Poison Dagger', level: 2 })).toBeVisible()
  await check(page, 'It may inflict a condition').check()
  const wbox = page.getByTestId('weapon-inflict')
  await wbox.getByLabel('Condition', { exact: true }).selectOption('poison')
  await wbox.getByLabel('Number of turns').fill('2')
  await saved(page)
  expect(weapon(id, 'poison_dagger').inflict).toEqual({ condition: 'poison', chance: 30, turns: 2 })
  await wbox.getByLabel('How often').selectOption('formula')
  await wbox.getByLabel('Formula for the chance, in percent').fill('a.skl * 4 - b.def')
  await saved(page)
  expect(weapon(id, 'poison_dagger').inflict).toEqual({ condition: 'poison', chance: 'a.skl * 4 - b.def', turns: 2 })
  await shotEl(page.getByTestId('weapon-inflict'), 'desktop-weapon-inflict')

  // The loader accepts everything, and the real engine loads it.
  expect(await game.problems(id)).toEqual([])
  await page.goto(editorUrl(id, 'conditions'))
  await playButton(page).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  await page.keyboard.press('Escape')

  // Deleting Poison: the question lists every use, and everything follows in one step.
  await page.goto(editorUrl(id, 'conditions', 'poison'))
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const ask = page.getByRole('dialog')
  await expect(ask).toContainText('Staff “Venom Staff” inflicts it')
  await expect(ask).toContainText('Item “Antidote” cures it')
  await expect(ask).toContainText('Weapon “Poison Dagger” may inflict it')
  await shot(page, 'desktop-condition-delete')
  await ask.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(conds(id).map((c) => c.id)).not.toContain('poison')
  expect(item(id, 'venom_staff').effects).toEqual([{ heal: 10 }])
  expect(item(id, 'venom_staff')).not.toHaveProperty('target')
  expect(item(id, 'antidote')).toMatchObject({ heal: 10 })
  expect(item(id, 'antidote')).not.toHaveProperty('cure')
  expect(weapon(id, 'poison_dagger')).not.toHaveProperty('inflict')
  expect(await game.problems(id)).toEqual([])
  // Undo brings all of it back.
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(item(id, 'venom_staff').effects).toEqual([{ inflict: { condition: 'poison', turns: 4 } }])
  expect(weapon(id, 'poison_dagger').inflict.condition).toBe('poison')
  expect(item(id, 'antidote').cure).toEqual(['poison'])
})

test('choices the loader would refuse cannot be made; skills use conditions; events give and cure them; what the form does not know is kept', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Conditions Rules', { srpgContent: true })
  await page.goto(editorUrl(id, 'conditions', 'sleep'))
  // Sleep cannot act: acting on its own is greyed, with the reason; the switch for counterattack and dodge are there.
  await expect(sw(page, 'The unit acts on its own')).toBeDisabled()
  await expect(page.getByTestId('condition-behaviour')).toContainText('cannot go together with “cannot act”')
  await check(page, 'It can still counterattack').check()
  await check(page, 'It can still dodge').check()
  await saved(page)
  expect(cond(id, 'sleep').blocks[0].do).toEqual([{ cannotAct: { counter: true, dodge: true } }, { cannotMove: true }])
  await check(page, 'It can still counterattack').uncheck()
  await check(page, 'It can still dodge').uncheck()
  await saved(page)
  expect(cond(id, 'sleep').blocks[0].do).toEqual([{ cannotAct: true }, { cannotMove: true }])
  // Berserk cannot be put with cannot act, the other way round.
  await page.goto(editorUrl(id, 'conditions', 'berserk'))
  await expect(sw(page, 'The unit cannot act')).toBeDisabled()
  // The last effect of a condition cannot be switched off.
  await expect(sw(page, 'The unit acts on its own')).toBeDisabled()
  await expect(page.getByTestId('condition-behaviour')).toContainText('has to do something')
  // Silence: forbid kinds and groups.
  await page.goto(editorUrl(id, 'conditions', 'silence'))
  const forbid = page.getByTestId('condition-forbid')
  await forbid.getByRole('checkbox', { name: 'Weapons' }).check()
  await saved(page)
  expect(cond(id, 'silence').blocks[0].do).toEqual([{ forbid: { kinds: ['weapon', 'staff'] } }])
  await forbid.getByRole('checkbox', { name: 'Staves' }).uncheck()
  await forbid.getByRole('checkbox', { name: 'Weapons' }).uncheck()
  await expect(page.getByTestId('condition-forbid-nothing')).toBeVisible()
  await expect(page.getByTestId('condition-issues')).toContainText('forbids nothing')
  await saved(page)
  expect((await game.problems(id)).map((p) => p.message).join()).toContain('"forbid" must name at least one')
  await forbid.getByRole('checkbox', { name: 'Staves' }).check()
  await saved(page)
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'desktop-silence-forbid')

  // A condition with no rule other than its switches: a stat rule cannot change hit points, and a new rule starts valid.
  await page.goto(editorUrl(id, 'conditions', 'curse'))
  const r1 = rule(page, 1)
  await part(r1, 'Effect 1').getByLabel('Add a stat…').click()
  await expect(part(r1, 'Effect 1').getByLabel('Add a stat…').locator('option', { hasText: /^HP$/ })).toHaveCount(0)

  // Skills: immune, the inflict effect of a strike, and the condition test.
  await page.goto(editorUrl(id, 'skills', 'tough'))
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const sr = rule(page, 2)
  await sr.getByLabel('When', { exact: true }).selectOption('always')
  await part(sr, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('immune')
  await part(sr, 'Effect 1').getByRole('checkbox', { name: 'Poison' }).check()
  await part(sr, 'Effect 1').getByRole('checkbox', { name: 'Sleep' }).check()
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const s3 = rule(page, 3)
  await s3.getByLabel('When', { exact: true }).selectOption('strike')
  await s3.getByLabel('Chance', { exact: true }).selectOption('percent')
  await s3.getByLabel('Percent', { exact: true }).fill('40')
  await part(s3, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('inflict')
  await part(s3, 'Effect 1').getByLabel('Condition', { exact: true }).selectOption('poison')
  await part(s3, 'Effect 1').getByLabel('Number of turns').fill('2')
  await s3.getByLabel('Add a condition…').selectOption('condition')
  await s3.getByRole('group', { name: 'Condition 1' }).getByLabel('Condition it has').selectOption('sleep')
  await s3.getByRole('group', { name: 'Condition 1' }).getByLabel('Who it is about').selectOption('foe')
  await saved(page)
  const tough = (readDisk(id, 'data/skills.json').skills as any[]).find((s) => s.id === 'tough')
  expect(tough.blocks.slice(1)).toEqual([
    { when: 'always', do: [{ immune: ['poison', 'sleep'] }] },
    { when: 'strike', if: [{ condition: 'sleep', who: 'foe' }], chance: 40, do: [{ inflict: { condition: 'poison', turns: 2 } }] },
  ])
  await expect(s3.getByTestId('skill-rule-summary')).toContainText('the foe has the condition Sleep')
  expect(await game.problems(id)).toEqual([])
  await shotEl(page.getByTestId('skill-form'), 'desktop-skill-immune-inflict')

  // Events: put a condition on units, cure them, and ask whether a unit has one.
  // (the template's first unit is the character "eirik": a character is known to events by its own id)
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'trap')
  await addCommand(page, 'Then', 'Put a condition on units')
  await form(page).getByLabel('Condition', { exact: true }).selectOption('poison')
  await form(page).getByLabel('Which units get it', { exact: true }).selectOption('unit')
  await form(page).getByLabel('Which units get it: unit').selectOption('eirik')
  await form(page).getByLabel('Number of turns').fill('2')
  await addCommand(page, 'Then', 'Cure units of a condition')
  await editor(page).getByLabel('Condition to take off').selectOption('all')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A unit has a status condition' })
  await editor(page).getByRole('group', { name: 'Only if: Condition 1' }).getByLabel('Condition', { exact: true }).selectOption('poison')
  await saved(page)
  const ev = readDisk(id, 'maps/ford.json').events[0]
  expect(ev.do[0]).toEqual({ addCondition: 'poison', to: { unit: 'eirik' }, turns: 2 })
  expect(ev.do[1]).toMatchObject({ removeCondition: 'all', from: expect.any(Object) })
  expect(ev.if).toEqual([{ hasCondition: 'poison' }])
  await expect(editor(page)).toContainText('Puts the condition “Poison” on')
  await shot(page, 'desktop-event-conditions')
  expect(await game.problems(id)).toEqual([])

  // A file the form cannot make: a staff with two of a kind. The Problems list says so, in plain words.
  const d2 = await (await request.get(`/api/projects/${id}`)).json()
  const odd = [...d2.items, { id: 'odd_staff', name: 'Odd Staff', kind: 'staff', group: 'staff', range: 2, effects: [{ inflict: { condition: 'poison' } }, { inflict: { condition: 'sleep' } }] }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: d2.revision, items: odd } })).ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'items', 'odd_staff'))
  await expect(page.getByTestId('item-checks')).toContainText('only one “inflicts a condition”')
  await expect(page.getByTestId('staff-more-effects')).toBeVisible() // the second inflict is kept, not touched
})

test('@smoke on a 360 px phone every part fits without sideways scrolling', async ({ page, game }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Conditions Phone', { srpgContent: true })
  await page.goto(editorUrl(id, 'conditions'))
  await expect(page.getByRole('heading', { name: 'Conditions', level: 1 })).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-conditions-list')
  await page.goto(editorUrl(id, 'conditions', 'sleep'))
  await expect(page.getByTestId('condition-form')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-condition-form')
  await page.goto(editorUrl(id, 'conditions', 'silence'))
  await expect(page.getByTestId('condition-forbid')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-condition-silence')
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r = rule(page, 1)
  await r.getByLabel('When', { exact: true }).selectOption('turnStart')
  await part(r, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('hurt')
  await part(r, 'Effect 1').getByLabel('Hurts', { exact: true }).selectOption('formula')
  await expectNoHScroll(page)
  await shot(page, 'phone-condition-hurt-formula')
  await page.goto(editorUrl(id, 'items'))
  await newStaff(page, 'Phone Staff')
  await check(page, 'It inflicts a condition').check()
  await check(page, 'It cures').check()
  await expectNoHScroll(page)
  await shot(page, 'phone-staff-conditions')
  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Phone Potion')
  await page.getByTestId('item-conditions').getByLabel('Cures', { exact: true }).selectOption('list')
  await expectNoHScroll(page)
  await shot(page, 'phone-item-conditions')
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await check(page, 'It may inflict a condition').check()
  await page.getByTestId('weapon-inflict').getByLabel('How often').selectOption('formula')
  await expectNoHScroll(page)
  await shot(page, 'phone-weapon-inflict')
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})
