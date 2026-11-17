// Upgrades, research, repair, fuel, ammunition and supply (docs/ECONOMY.md) through the real editor: a town that becomes a
// city once Masonry is researched, a structure that allows research, two technologies (prerequisite, unlock, bonus), a
// repair rule, a unit with fuel that loses some every turn, a supplier, a weapon with ammunition, a "has researched"
// condition and a "give a technology" command. The proof that matters: the exact files the editor wrote, and that the real
// game loader accepts all of it (/problems is empty). Then deleting a technology that is used, and a phone.
import type { Page } from '@playwright/test'
import { openSectionMenu, editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'
import { addCommand, editor, eventsTab, form, newEvent, shot } from './storykit'

const open = async (page: Page, id: string, section: string, item?: string) => {
  await page.goto(editorUrl(id, section, item))
  await expect(page.getByRole('heading', { level: 1 }).first()).toBeVisible()
}

async function create(page: Page, noun: string, name: string): Promise<void> {
  await page.getByRole('button', { name: `New ${noun}` }).click()
  const dlg = page.getByRole('dialog', { name: `New ${noun}` })
  await dlg.getByLabel(`Name of the ${noun}`).fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

const undo = (page: Page) => page.getByRole('button', { name: 'Undo' })
const group = (page: Page, name: string) => page.getByRole('group', { name, exact: true })

test('@smoke a town that becomes a city with a technology, research, repair, fuel, a supplier, ammunition, a condition and a command: the files are exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(180_000)
  const id = await game.create('economy-tactics', 'Economy')

  // --- Research is empty at first: it explains itself. Nothing is written.
  await open(page, id, 'research')
  await expect(page.getByText('No technologies yet.').first()).toBeVisible()
  await expect(page.getByText(/Research lets a side pay and wait/)).toBeVisible()
  await shot(page, 'research-empty')
  await expect(page.getByRole('link', { name: 'Open Structures' })).toHaveCount(0) // research needs no structure by default
  await open(page, id, 'structures')
  await expect(page.getByRole('heading', { name: 'Structures', level: 1 })).toBeVisible()

  // --- Structures: a Town, and a Library where research is done.
  await create(page, 'structure', 'Town')
  await expect(page.getByTestId('hidden-settings')).toContainText('repairs') // no repair rule yet: one line says so
  await page.getByRole('button', { name: 'Add an upgrade' }).click()
  const up = group(page, 'Upgrade 1')
  await expect(up.getByLabel('Becomes')).toHaveValue('city')
  await expect(up.getByRole('option', { name: 'Headquarters' })).toHaveCount(0) // a different footprint cannot be picked
  await up.getByLabel('Funds Cost').fill('3000')
  await create(page, 'structure', 'Library')
  // Research needs no structure by default: no checkbox, one line that points at the rule.
  await expect(page.getByRole('checkbox', { name: /Research can be done here/ })).toHaveCount(0)
  await expect(page.getByTestId('research-rule-off')).toBeVisible()
  await saved(page)
  expect(await game.problems(id)).toEqual([]) // an upgrade with no technology needed is fine on its own

  // --- Research: Tools (free of turns, a bonus) and Masonry (needs Tools, unlocks the Tank).
  await open(page, id, 'research')
  await expect(page.getByTestId('no-research-structure')).toHaveCount(0) // the default rule needs no structure
  await create(page, 'technology', 'Tools')
  await page.getByLabel('Description').fill('Basic tools.')
  await page.getByLabel('Funds Cost').fill('1000')
  await page.getByLabel('Turns it takes').fill('0')
  await expect(page.getByTestId('tech-turns-text')).toHaveText('Finished at once')
  await page.getByRole('switch', { name: /Makes units stronger/ }).check()
  await page.getByLabel('Which units').selectOption({ label: 'Only these unit types' })
  await group(page, 'Only these unit types').getByRole('checkbox', { name: 'Infantry' }).check()
  await page.getByLabel('HP Change').fill('')
  await page.getByLabel('Move Change').fill('1')
  await saved(page)

  await create(page, 'technology', 'Masonry')
  await page.getByLabel('Funds Cost').fill('4000')
  await page.getByLabel('Turns it takes').fill('2')
  await expect(page.getByTestId('tech-turns-text')).toContainText('2 turns later')
  await group(page, 'Needs first').getByRole('checkbox', { name: 'Tools' }).check()
  await group(page, 'Unlocks unit types').getByRole('checkbox', { name: 'Tank' }).check()
  await expect(page.getByText('cannot be built without this technology')).toBeVisible()
  await shot(page, 'research-masonry')
  await saved(page)

  // Tools can no longer need Masonry: that would be a loop. The option says why.
  await page.getByRole('list', { name: 'Research' }).getByRole('button', { name: /Tools/ }).click()
  const loop = group(page, 'Needs first').getByRole('checkbox', { name: /Masonry/ })
  await expect(loop).toBeDisabled()
  await expect(loop.locator('xpath=ancestor::label')).toContainText('loop')
  await shot(page, 'research-tools-open')

  // --- The Town's upgrade now needs Masonry; Masonry's form says where it is used.
  await open(page, id, 'structures', 'town')
  await group(page, 'Upgrade 1').getByLabel('Needs technology').selectOption({ label: 'Masonry' })
  await saved(page)
  await open(page, id, 'research', 'masonry')
  await expect(page.getByTestId('tech-uses')).toContainText('the upgrade of “Town” to “City”')

  // --- Rules: repair, its price, and resupply on structures. Then the City repairs only Tanks.
  await open(page, id, 'rules')
  await page.getByRole('switch', { name: /Repair on own structures/ }).check()
  await page.getByLabel('Hit points healed per turn').fill('25')
  await page.getByLabel('Share of the unit’s price paid').fill('50')
  await page.getByRole('checkbox', { name: /Own structures also refill fuel and ammunition/ }).check()
  await shot(page, 'rules-supply')
  // The Research group appears now that the game has technologies; this game wants a structure for research.
  await expect(page.getByTestId('research-rule')).toBeVisible()
  await page.getByRole('checkbox', { name: /Research needs a structure/ }).check()
  await page.getByLabel('When a unit walks into a hidden enemy').selectOption({ label: 'It can still act' })
  await shot(page, 'rules-research-fog')
  await open(page, id, 'research')
  await expect(page.getByTestId('no-research-structure')).toBeVisible() // rule on, and no kind allows it yet
  await open(page, id, 'structures', 'library')
  await page.getByRole('checkbox', { name: /Research can be done here/ }).check()
  await open(page, id, 'research')
  await expect(page.getByTestId('no-research-structure')).toHaveCount(0)
  await open(page, id, 'structures', 'city')
  await expect(page.getByTestId('hidden-settings')).toHaveCount(0)
  await page.getByLabel('Repairs', { exact: true }).selectOption({ label: 'Only these' })
  await expect(page.getByTestId('repairs-nothing')).toBeVisible()
  await group(page, 'Unit types and labels it repairs').getByRole('checkbox', { name: 'Tank' }).check()
  await shot(page, 'structures-upgrades-repairs')

  // --- Units: a tank with fuel that loses some each turn, and a new supply truck.
  await open(page, id, 'units', 'tank')
  await page.getByRole('checkbox', { name: /Uses fuel/ }).check()
  await expect(page.getByLabel('Fuel tank')).toHaveValue('60')
  await page.getByRole('checkbox', { name: /Loses fuel every turn/ }).check()
  await expect(page.getByLabel('Fuel lost each turn')).toHaveValue('5')
  await shot(page, 'units-fuel')
  await page.getByRole('button', { name: 'New unit' }).click()
  await page.getByRole('dialog', { name: 'New unit' }).getByLabel('Name of the unit').fill('Supply Truck')
  await page.getByRole('dialog', { name: 'New unit' }).getByRole('button', { name: 'Create' }).click()
  await page.getByRole('checkbox', { name: /Resupplies units next to it/ }).check()

  // --- Weapons: the artillery cannon has six shots.
  await open(page, id, 'weapons', 'cannon')
  await page.getByRole('checkbox', { name: /Limited ammunition/ }).check()
  await expect(page.getByLabel('Ammunition', { exact: true })).toHaveValue('6')
  await shot(page, 'weapons-ammo')

  // --- Story: a condition and a command, in a map event.
  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await newEvent(page, 'gate')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A side has researched a technology' })
  const cond = editor(page).getByRole('group', { name: 'Only if: Condition 1' })
  await cond.getByLabel('Technology', { exact: true }).selectOption({ label: 'Masonry' })
  await cond.getByLabel('Side', { exact: true }).selectOption({ label: 'Blue Army' })
  await addCommand(page, 'Then', 'Give a technology')
  await form(page).getByLabel('Technology', { exact: true }).selectOption({ label: 'Tools' })
  await form(page).getByLabel('Side that gets it').selectOption({ label: 'Red Army' })
  await expect(editor(page).getByTestId('command-grantResearch')).toContainText('Gives side Red Army the technology “Tools”')
  await saved(page)

  // --- The files are exactly that, and the real loader accepts them.
  const structures = readDisk(id, 'data/structures.json').structures as Array<Record<string, any>>
  const kind = (k: string) => structures.find((s) => s.id === k)!
  expect(kind('town')).toMatchObject({ id: 'town', name: 'Town', upgrades: [{ to: 'city', cost: { funds: 3000 }, requires: 'masonry' }] })
  expect(kind('library')).toMatchObject({ id: 'library', research: true })
  expect(kind('city').repairs).toEqual(['tank'])
  expect(readDisk(id, 'data/research.json')).toEqual({
    technologies: [
      { id: 'tools', name: 'Tools', description: 'Basic tools.', cost: { funds: 1000 }, turns: 0, bonus: { stats: { mov: 1 }, types: ['infantry'] } },
      { id: 'masonry', name: 'Masonry', cost: { funds: 4000 }, turns: 2, requires: ['tools'], unlocks: { units: ['tank'] } },
    ],
  })
  expect(readDisk(id, 'game.json').rules.supply).toEqual({ repair: { amount: 25, costShare: 50 }, structures: true })
  expect(readDisk(id, 'game.json').rules.research).toEqual({ needsStructure: true })
  expect(readDisk(id, 'game.json').rules.fog).toEqual({ ambush: 'canAct' })
  const units = readDisk(id, 'data/units.json').units as Array<Record<string, any>>
  expect(units.find((u) => u.id === 'tank')).toMatchObject({ fuel: 60, fuelPerTurn: 5 })
  expect(units.find((u) => u.id === 'supply_truck')).toMatchObject({ supplies: true })
  expect((readDisk(id, 'data/weapons.json').weapons as Array<Record<string, any>>).find((w) => w.id === 'cannon')).toMatchObject({ ammo: 6 })
  expect(readDisk(id, 'maps/crossroads.json').events).toEqual([
    { id: 'gate', when: { battle: 'start' }, if: [{ researched: 'masonry', side: 'blue' }], do: [{ grantResearch: 'tools', side: 'red' }] },
  ])
  expect(await game.problems(id)).toEqual([])

  // --- Delete a technology that is used: the confirm lists the uses; the files follow; one undo brings it all back.
  const before = {
    structures: JSON.stringify(readDisk(id, 'data/structures.json')), research: JSON.stringify(readDisk(id, 'data/research.json')),
    map: JSON.stringify(readDisk(id, 'maps/crossroads.json')),
  }
  await open(page, id, 'research', 'masonry')
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog', { name: /Delete the technology “Masonry”\?/ })
  await expect(dlg).toContainText('Town')
  await expect(dlg).toContainText('gate')
  await shot(page, 'research-delete-confirm')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(readDisk(id, 'data/research.json').technologies.map((t: any) => t.id)).toEqual(['tools'])
  expect(kind2(readDisk(id, 'data/structures.json').structures, 'town').upgrades).toEqual([{ to: 'city', cost: { funds: 3000 } }])
  expect(readDisk(id, 'maps/crossroads.json')).not.toHaveProperty('events') // the event waited for Masonry, so it went with it
  expect(await game.problems(id)).toEqual([])
  await undo(page).click()
  await saved(page)
  expect(JSON.stringify(readDisk(id, 'data/structures.json'))).toBe(before.structures)
  expect(JSON.stringify(readDisk(id, 'data/research.json'))).toBe(before.research)
  expect(JSON.stringify(readDisk(id, 'maps/crossroads.json'))).toBe(before.map)
  expect(await game.problems(id)).toEqual([])
})

const kind2 = (list: Array<Record<string, any>>, k: string) => list.find((s) => s.id === k)!

test('the two option rules: the Research group follows the game, the flag on a structure is kept, defaults are not written', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Options')
  // A game with no technologies and no rule: no Research group; the Fog group is always there.
  await open(page, id, 'rules')
  await expect(page.getByTestId('research-rule')).toHaveCount(0)
  await expect(page.getByTestId('fog-rule')).toBeVisible()
  await expect(page.getByLabel('When a unit walks into a hidden enemy')).toHaveValue('endsTurn')
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('fog')
  // Choosing the non-default writes the one key; choosing the default again removes the block.
  await page.getByLabel('When a unit walks into a hidden enemy').selectOption({ label: 'It can still act' })
  await saved(page)
  expect(readDisk(id, 'game.json').rules.fog).toEqual({ ambush: 'canAct' })
  await page.getByLabel('When a unit walks into a hidden enemy').selectOption({ label: 'Its turn ends' })
  await saved(page)
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('fog')
  // A structure that already says research: true keeps it while the rule is off; the checkbox is hidden then.
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  doc.structures[0].research = true
  doc.research = [{ id: 'tools', name: 'Tools' }]
  const res = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, structures: doc.structures, research: doc.research } })
  expect(res.ok()).toBeTruthy()
  await open(page, id, 'structures', doc.structures[0].id)
  await expect(page.getByTestId('research-rule-off')).toBeVisible()
  await expect(page.getByRole('checkbox', { name: /Research can be done here/ })).toHaveCount(0)
  await page.getByTestId('research-rule-off').getByRole('link', { name: 'Open Rules' }).click()
  await expect(page).toHaveURL(new RegExp(`/p/${id}/rules`))
  // Now the game has technologies: the Research group shows. On: the checkbox is back and ticked; off again: key gone, flag kept.
  const needs = page.getByRole('checkbox', { name: /Research needs a structure/ })
  await expect(needs).not.toBeChecked()
  await needs.check()
  await saved(page)
  expect(readDisk(id, 'game.json').rules.research).toEqual({ needsStructure: true })
  await open(page, id, 'structures', doc.structures[0].id)
  await expect(page.getByRole('checkbox', { name: /Research can be done here/ })).toBeChecked()
  await open(page, id, 'rules')
  await page.getByRole('checkbox', { name: /Research needs a structure/ }).uncheck()
  await saved(page)
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('research')
  expect(readDisk(id, 'data/structures.json').structures[0].research).toBe(true)
  expect(await game.problems(id)).toEqual([])
})

test('the checks name everything the loader would refuse, at once, on the screen that holds it', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Checks')
  // A game made by hand that breaks several rules at once (the editor itself keeps the data valid).
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  doc.structures[0].upgrades = [{ to: 'hq' }, { to: 'nowhere' }]
  doc.structures[0].repairs = ['wat']
  doc.units[1].fuel = 10
  doc.units[1].fuelPerTurn = 10
  doc.weapons[0].ammo = 500
  doc.game.rules.supply = { repair: { amount: 150 } }
  const res = await page.request.put(`/api/projects/${id}`, {
    data: {
      baseRevision: doc.revision, game: doc.game, structures: doc.structures, units: doc.units, weapons: doc.weapons,
      research: [{ id: 'a', name: 'A', requires: ['b'] }, { id: 'b', name: 'B', requires: ['a'], bonus: { stats: {} } }],
    },
  })
  expect(res.ok()).toBeTruthy()
  const checks = (_section: string) => page.getByTestId('economy-checks').filter({ visible: true })

  await open(page, id, 'research')
  await expect(checks('research')).toContainText('in a loop: A → B → A')
  await expect(checks('research')).toContainText('at least one stat')
  await open(page, id, 'structures', 'city')
  await expect(checks('structures')).toContainText('keeps the size')
  await expect(checks('structures')).toContainText('“nowhere”, which is not a structure')
  await expect(checks('structures')).toContainText('it repairs “wat”')
  await open(page, id, 'units', 'tank')
  await expect(checks('units')).toContainText('holds only 10')
  await open(page, id, 'weapons', 'rifle')
  await expect(checks('weapons')).toContainText('from 1 to 99')
  await open(page, id, 'rules')
  await expect(checks('rules')).toContainText('from 1 to 100')
  await shot(page, 'economy-checks-rules')
  expect((await game.problems(id)).length).toBeGreaterThan(0) // and the real loader indeed refuses
})

test('a weapon carried as an item cannot have ammunition, and says why', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Carried')
  await open(page, id, 'rules')
  await page.getByLabel('Weapons', { exact: true }).selectOption({ label: 'Carried as items' })
  await open(page, id, 'items')
  await create(page, 'item', 'Cannon Item')
  await page.getByLabel('Kind').selectOption({ label: 'Weapon' })
  await page.getByLabel('Which weapon').selectOption({ label: 'Artillery Cannon' })
  await saved(page)
  await open(page, id, 'weapons', 'cannon')
  const box = page.getByRole('checkbox', { name: /Limited ammunition/ })
  await expect(box).toBeDisabled()
  await expect(page.getByText('carried as the item “Cannon Item”')).toBeVisible()
  await open(page, id, 'weapons', 'rifle')
  await expect(page.getByRole('checkbox', { name: /Limited ammunition/ })).toBeEnabled()
})

test.describe('on a phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
  test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

  test('the Research screen at 360 px: empty, list, an open technology with every card; no horizontal scroll', async ({ page, game }) => {
    const id = await game.create('economy-tactics', 'Phone Research')
    await page.goto(editorUrl(id, 'research'))
    await expect(page.getByText('No technologies yet.').first()).toBeVisible()
  await expect(page.getByText(/Research lets a side pay and wait/)).toBeVisible()
    await expectNoHScroll(page)
    await openSectionMenu(page)
    const link = page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name: 'Research', exact: true })
    await expect(link).toBeVisible()
    expect(await link.evaluate((el) => el.scrollWidth > el.clientWidth + 1), 'the "Research" label is whole').toBe(false)
    await page.keyboard.press('Escape')
    await shot(page, 'research-phone-empty')
    await create(page, 'technology', 'Armor Plating With A Rather Long Name')
    await page.getByRole('switch', { name: /Makes units stronger/ }).check()
    await page.getByLabel('Which units').selectOption({ label: 'Only these unit types' })
    for (const control of [page.getByLabel('Funds Cost'), page.getByLabel('Turns it takes'), group(page, 'Needs first'), group(page, 'Unlocks unit types'), page.getByLabel('Move Change')]) {
      await control.scrollIntoViewIfNeeded()
      await expectNoHScroll(page)
    }
    await shot(page, 'research-phone-open')
    await page.getByRole('button', { name: /^Research$/ }).click()
    await expectNoHScroll(page)
    await shot(page, 'research-phone-list')
    await page.goto(editorUrl(id, 'structures', 'city'))
    await page.getByRole('button', { name: 'Add an upgrade' }).click()
    await expectNoHScroll(page)
    await shot(page, 'structures-phone-upgrades')
  })
})
