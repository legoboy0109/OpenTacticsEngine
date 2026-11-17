// Doors, chests, visiting places and keys through the editor's own forms (docs/SRPG.md section 10, docs/EDITOR_UI.md "Doors, chests and places"):
// a door kind, a chest kind and a visit kind on the Structures screen, a key and an unlock staff on the Items screen, an unlock ability, the Rules card,
// placed ones with a story name and contents on a map, a computer unit's "goes for", and an event with a `place` trigger and an openDoor command.
// The proof that matters: the exact JSON the forms wrote (compared with what docs/SRPG.md section 10 specifies), that the real loader accepts it
// (/problems is empty), that the real engine loads it (Play), and the 360 px phone layout.
import type { APIRequestContext, Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, requirePlayer, saved, test } from './fixtures'
import { clickCell, openMap, pickStructure, tab, tool } from './mapkit'
import { shot, shotEl } from './rankskit'
import { addCommand, editor, eventsTab, form, newEvent, when } from './storykit'

test.use({ actionTimeout: 10_000 }) // a wrong label fails in seconds, not minutes

const structures = (id: string) => readDisk(id, 'data/structures.json').structures as any[]
const kind = (id: string, k: string) => structures(id).find((s) => s.id === k)
const items = (id: string) => readDisk(id, 'data/items.json').items as any[]
const item = (id: string, k: string) => items(id).find((i) => i.id === k)
const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const playPanel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })
const part = (page: Page, which: 'door' | 'chest' | 'visit') => page.getByTestId(`part-${which}`)
const partSwitch = (page: Page, which: 'door' | 'chest' | 'visit', label: string) => part(page, which).getByRole('checkbox', { name: label, exact: false }).first()

async function newKind(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New structure' }).click()
  const dlg = page.getByRole('dialog', { name: 'New structure' })
  await dlg.getByLabel('Name of the structure').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

async function newItem(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New item' }).click()
  const dlg = page.getByRole('dialog', { name: 'New item' })
  await dlg.getByLabel('Name of the item').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

/** Gold, so a chest can hold an amount (the template has no resources). Written through the API like a second author would. */
async function addGold(request: APIRequestContext, id: string): Promise<void> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.game.resources = [{ id: 'gold', name: 'Gold' }]
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

test('@smoke build a door, a chest, a place and a key through the forms, place them with contents and a story name, add a place event: the files are what docs/SRPG.md says and the game loads', async ({ page, game, request }) => {
  test.setTimeout(180_000)
  requirePlayer()
  const id = await game.create('strategy-rpg', 'Places', { srpgContent: true })
  await addGold(request, id)

  // ===== Structures: the kinds (every look is its own structure kind) =====
  await page.goto(editorUrl(id, 'structures'))
  for (const name of ['Iron Gate', 'Open Gate', 'Treasure Chest', 'Open Chest', 'Village', 'Quiet Village', 'Burnt Village']) await newKind(page, name)

  // The door: nothing yet to say of it; it opens into the open gate and stops every kind of unit until the maker says otherwise.
  await page.goto(editorUrl(id, 'structures', 'iron_gate'))
  await expect(page.getByTestId('place-parts')).toBeVisible()
  await partSwitch(page, 'door', 'It is a door').check()
  await page.getByLabel('When opened it becomes', { exact: true }).selectOption({ label: 'Open Gate' })
  await saved(page)
  expect(kind(id, 'iron_gate').door).toEqual({ opens: 'open_gate' })
  expect(kind(id, 'iron_gate').moveCost).toEqual({ foot: 0, horse: 0 })
  // A structure is only one of the three: the others are greyed, with the reason.
  await expect(partSwitch(page, 'chest', 'It is a chest')).toBeDisabled()
  await expect(part(page, 'chest')).toContainText('A structure is only one of these')
  // Breaking it down gives it hit points (and what its damage reads); fog: it blocks sight; it is used from next to it.
  await partSwitch(page, 'door', 'Breaking it down').check()
  await page.getByLabel('Hit points', { exact: true }).fill('20')
  await part(page, 'door').getByLabel('Defense Value').fill('3')
  await part(page, 'door').getByLabel('Where a unit stands to use it').selectOption('next')
  await part(page, 'door').getByRole('checkbox', { name: /It blocks sight/ }).check()
  await saved(page)
  expect(kind(id, 'iron_gate')).toMatchObject({ blocksSight: true, door: { opens: 'open_gate', hp: 20, stats: { def: 3 }, from: 'next' } })
  await shotEl(page.getByTestId('place-parts'), 'desktop-door-part')

  // The chest.
  await page.goto(editorUrl(id, 'structures', 'treasure_chest'))
  await partSwitch(page, 'chest', 'It is a chest').check()
  await page.getByLabel('When opened it becomes', { exact: true }).selectOption({ label: 'Open Chest' })
  await saved(page)
  expect(kind(id, 'treasure_chest').chest).toEqual({ opens: 'open_chest' })
  await shotEl(page.getByTestId('place-parts'), 'desktop-chest-part')

  // The place: becomes the quiet village after a visit, the burnt one when destroyed.
  await page.goto(editorUrl(id, 'structures', 'village'))
  await partSwitch(page, 'visit', 'It is a place that can be visited').check()
  await page.getByLabel('After a visit it becomes', { exact: true }).selectOption({ label: 'Quiet Village' })
  await page.getByLabel('When destroyed it becomes', { exact: true }).selectOption({ label: 'Burnt Village' })
  await saved(page)
  expect(kind(id, 'village').visit).toEqual({ becomes: 'quiet_village', ruin: 'burnt_village' })
  // Visiting again and again cannot go with becoming something else.
  await expect(part(page, 'visit').getByRole('checkbox', { name: /visited again and again/ })).toBeDisabled()
  await shotEl(page.getByTestId('place-parts'), 'desktop-visit-part')

  // ===== Items: a key (a master key), a chest key, an unlock staff =====
  await page.goto(editorUrl(id, 'items'))
  await newItem(page, 'Brass Key')
  await page.getByTestId('item-key').getByRole('checkbox', { name: /It opens doors and chests/ }).check()
  await saved(page)
  expect(item(id, 'brass_key')).toMatchObject({ kind: 'usable', opens: ['door'] })
  expect(item(id, 'brass_key')).not.toHaveProperty('heal') // a key is not also a potion
  await page.getByTestId('item-key').getByRole('checkbox', { name: 'Any chest' }).check()
  await saved(page)
  expect(item(id, 'brass_key').opens).toEqual(['door', 'chest'])
  await shotEl(page.getByTestId('item-key'), 'desktop-key-part')
  await newItem(page, 'Chest Key')
  await page.getByTestId('item-key').getByRole('checkbox', { name: /It opens doors and chests/ }).check()
  await page.getByTestId('item-key').getByRole('checkbox', { name: 'Treasure Chest (chest)' }).check()
  await page.getByTestId('item-key').getByRole('checkbox', { name: 'Any door' }).uncheck() // the last tick cannot be removed first: an empty list is refused
  await saved(page)
  expect(item(id, 'chest_key').opens).toEqual(['treasure_chest'])
  await newItem(page, 'Unlock Staff')
  await page.getByLabel('Kind', { exact: true }).selectOption('staff')
  await page.getByRole('checkbox', { name: /It unlocks doors and chests/ }).check()
  await page.getByLabel('What it unlocks', { exact: true }).selectOption('door')
  await saved(page)
  const staff = item(id, 'unlock_staff')
  expect(staff).toMatchObject({ kind: 'staff', group: 'staff', effects: [{ unlock: ['door'] }] })
  for (const k of ['target', 'area', 'if']) expect(staff).not.toHaveProperty(k)
  await shotEl(page.getByTestId('staff-effect'), 'desktop-unlock-staff')

  // The door's own list of keys: only the brass key (the chest key does not open doors, so it is not offered).
  await page.goto(editorUrl(id, 'structures', 'iron_gate'))
  const keys = part(page, 'door').getByRole('group', { name: 'Only these keys fit' })
  await expect(keys.getByRole('checkbox', { name: 'Chest Key' })).toHaveCount(0)
  await keys.getByRole('checkbox', { name: 'Brass Key' }).check()
  await saved(page)
  expect(kind(id, 'iron_gate').door.keys).toEqual(['brass_key'])

  // ===== Abilities: the thief's trick =====
  await page.goto(editorUrl(id, 'abilities'))
  await page.getByRole('button', { name: 'New ability' }).click()
  await page.getByRole('dialog', { name: 'New ability' }).getByLabel('Name of the ability').fill('Pick Lock')
  await page.getByRole('dialog', { name: 'New ability' }).getByRole('button', { name: 'Create' }).click()
  await page.getByLabel('What it does', { exact: true }).selectOption('unlock')
  await page.getByLabel('What it unlocks', { exact: true }).selectOption('chest')
  await saved(page)
  expect(readDisk(id, 'data/abilities.json').abilities.find((a: any) => a.id === 'pick_lock')).toMatchObject({ target: 'cell', effect: { unlock: ['chest'] } })
  await shotEl(page.getByTestId('ability-form'), 'desktop-unlock-ability')

  // ===== Rules =====
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('How they are used', { exact: true }).selectOption('auto')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.places).toEqual({ actions: 'auto' })
  await expect(page.getByTestId('places-rule-explain')).toContainText('it happens by itself')
  await shotEl(page.getByTestId('places-rule'), 'desktop-rules-card')

  // ===== The map: place them, give them story names and contents =====
  await openMap(page, id, 'ford')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Iron Gate').click()
  await clickCell(page, 4, 3)
  const props = page.getByTestId('place-props')
  await expect(props).toBeVisible()
  await props.getByLabel('Story name').fill('cell_door')
  await props.getByLabel('Locked', { exact: true }).selectOption('no')
  await pickStructure(page, 'Treasure Chest').click()
  await clickCell(page, 3, 6)
  await props.getByLabel('Story name').fill('treasure')
  await saved(page)
  // A new chest starts with the game's first item inside; make it a potion-free box: a brass key and 50 gold.
  await expect(props.getByTestId('loot-item')).toHaveCount(1)
  await props.getByRole('button', { name: 'Remove thing 1' }).click()
  await props.getByRole('button', { name: 'Add an item' }).click()
  await props.getByLabel('Thing 1: item', { exact: true }).selectOption({ label: 'Brass Key' })
  await props.getByLabel('Uses left').fill('2')
  await props.getByRole('button', { name: 'Add an amount' }).click()
  await props.getByLabel('Amount').fill('50')
  await pickStructure(page, 'Village').click()
  await clickCell(page, 6, 3)
  await props.getByLabel('Story name').fill('hamlet')
  await props.getByRole('group', { name: 'Sides' }).getByRole('checkbox', { name: 'Blue Company' }).check()
  await saved(page)
  const ford = readDisk(id, 'maps/ford.json')
  expect(ford.structures).toEqual([
    { type: 'iron_gate', x: 4, y: 3, id: 'cell_door', locked: false },
    { type: 'treasure_chest', x: 3, y: 6, id: 'treasure', contents: [{ item: 'brass_key', uses: 2 }, { resource: 'gold', amount: 50 }] },
    { type: 'village', x: 6, y: 3, id: 'hamlet', visitSides: ['blue'] },
  ])
  await shotEl(page.getByTestId('structure-props'), 'desktop-map-place-panel')

  // A computer unit goes for chests; a unit of a side a person plays has no such control.
  await tool(page, 'Select').click()
  await clickCell(page, 8, 5) // the soldier "Tane" of the red side
  await page.getByTestId('goes-for').getByRole('checkbox', { name: /^Chests/ }).check()
  await page.getByTestId('goes-for').getByRole('checkbox', { name: /^Villages/ }).check()
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').units.find((u: any) => u.name === 'Tane').goesFor).toEqual(['chests', 'villages'])
  await shotEl(page.getByTestId('goes-for'), 'desktop-goes-for')
  await clickCell(page, 1, 4)
  await expect(page.getByTestId('goes-for')).toHaveCount(0)

  // ===== Events: when the hamlet is visited the cell door opens =====
  await eventsTab(page).click()
  await newEvent(page, 'hamlet_visited')
  await when(page, 'A door, chest or place is used')
  await editor(page).getByLabel('What happens to it', { exact: true }).selectOption('visited')
  await editor(page).getByLabel('Which one', { exact: true }).selectOption({ label: 'hamlet (Village)' })
  await addCommand(page, 'Then', 'Open a door')
  await form(page).getByLabel('Which one', { exact: true }).selectOption({ label: 'cell_door (Iron Gate)' })
  await addCommand(page, 'Then', 'Open a chest')
  await form(page).last().getByLabel('Which one', { exact: true }).selectOption({ label: 'treasure (Treasure Chest)' })
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').events).toEqual([
    { id: 'hamlet_visited', when: { place: 'hamlet', is: 'visited' }, do: [{ openDoor: 'cell_door' }, { openChest: 'treasure' }] },
  ])
  await shotEl(editor(page), 'desktop-place-event')
  await shot(page, 'desktop-place-event-page')

  // Renaming a story name in the map's panel changes the event with it (one edit).
  await tab(page, 'Tool').click()
  await tool(page, 'Structures').click()
  await clickCell(page, 4, 3)
  await page.getByTestId('place-props').getByLabel('Story name').fill('gate_a')
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').events[0].do[0]).toEqual({ openDoor: 'gate_a' })
  await page.getByTestId('place-props').getByLabel('Story name').fill('cell_door')
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').events[0].do[0]).toEqual({ openDoor: 'cell_door' })

  // ===== The files and the loader =====
  expect(await game.problems(id)).toEqual([])
  await page.goto(editorUrl(id, 'maps', 'ford'))
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
  await playButton(page).click()
  await expect(playPanel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})

/** The same data as the forms above write, made through the API (so the phone test judges layout, not the clicking). */
async function placesGame(request: APIRequestContext, id: string): Promise<void> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.game.resources = [{ id: 'gold', name: 'Gold' }]
  const sq = (idv: string, name: string, extra: Record<string, unknown> = {}) => ({ id: idv, name, color: '#888888', glyph: name[0], ...extra })
  const structuresList = [
    sq('iron_gate', 'Iron Gate', { moveCost: { foot: 0, horse: 0 }, blocksSight: true, door: { opens: 'open_gate', hp: 20, stats: { def: 3 }, keys: ['brass_key'] } }),
    sq('open_gate', 'Open Gate'),
    sq('treasure_chest', 'Treasure Chest', { chest: { opens: 'open_chest' } }),
    sq('open_chest', 'Open Chest'),
    sq('village', 'Village', { visit: { becomes: 'quiet_village', ruin: 'burnt_village' } }),
    sq('quiet_village', 'Quiet Village'),
    sq('burnt_village', 'Burnt Village'),
  ]
  const itemList = [
    ...(doc.items ?? []),
    { id: 'brass_key', name: 'Brass Key', kind: 'usable', opens: ['door', 'chest'], uses: 3 },
    { id: 'unlock_staff', name: 'Unlock Staff', kind: 'staff', group: 'staff', uses: 3, range: 1, effects: [{ unlock: ['door'] }] },
  ]
  const abilities = [...(doc.abilities ?? []), { id: 'pick_lock', name: 'Pick Lock', target: 'cell', effect: { unlock: { kinds: ['chest'], needs: 'brass_key' } } }]
  const ford = doc.maps.ford
  ford.structures = [
    { type: 'iron_gate', x: 4, y: 3, id: 'cell_door', locked: false },
    { type: 'treasure_chest', x: 3, y: 6, id: 'treasure', contents: [{ item: 'brass_key', uses: 2 }, { resource: 'gold', amount: 50 }] },
    { type: 'village', x: 6, y: 3, id: 'hamlet', visitSides: ['blue'], visitUnits: ['eirik'] },
  ]
  ford.units = ford.units.map((u: any) => (u.name === 'Tane' ? { ...u, goesFor: ['chests', 'villages'] } : u))
  ford.events = [{ id: 'hamlet_visited', when: { place: 'hamlet', is: 'visited' }, do: [{ openDoor: 'cell_door' }, { openChest: 'treasure', by: 'eirik' }] }]
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game, structures: structuresList, items: itemList, abilities, maps: doc.maps } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

test('@smoke on a 360 px phone every new form fits without sideways scrolling', async ({ page, game, request }) => {
  test.setTimeout(120_000)
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Places Phone', { srpgContent: true })
  await placesGame(request, id)
  expect(await game.problems(id)).toEqual([])

  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Structures')
  await page.getByRole('button', { name: /Iron Gate/ }).first().click()
  await expect(page.getByTestId('place-parts')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-door-part')
  await page.goto(editorUrl(id, 'structures', 'treasure_chest'))
  await expect(page.getByTestId('place-parts')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-chest-part')
  await page.goto(editorUrl(id, 'structures', 'village'))
  await expect(page.getByTestId('place-parts')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-visit-part')

  await page.goto(editorUrl(id, 'items', 'brass_key'))
  await expect(page.getByTestId('item-key')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-key-part')
  await page.goto(editorUrl(id, 'items', 'unlock_staff'))
  await expect(page.getByTestId('staff-effect')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-unlock-staff')
  await page.goto(editorUrl(id, 'abilities', 'pick_lock'))
  await expect(page.getByTestId('ability-unlock')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-unlock-ability')
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByTestId('places-rule')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-rules-card')

  // The map editor on a phone: the structure panel with its story name and contents, a computer unit's goes-for, the place event.
  await openMap(page, id, 'ford')
  await expectNoHScroll(page)
  await tab(page, 'Tool').click()
  await tool(page, 'Structures').click()
  await clickCell(page, 3, 6)
  await expect(page.getByTestId('place-props')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-map-chest-panel')
  await tool(page, 'Select').click()
  await clickCell(page, 8, 5)
  await expect(page.getByTestId('goes-for')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-map-goes-for')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Open event hamlet_visited', exact: true }).click()
  await expect(editor(page)).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'phone-place-event')
})
