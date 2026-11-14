// Story events (docs/EVENTS.md) through the real editor: one event of each trigger, one command of each kind (with a
// three-level nested "If"), conditions, objectives, a switch and a variable. The proof that matters: the exact JSON the
// editor wrote, and that the real game loader accepts all of it (/problems is empty).
import type { Page } from '@playwright/test'
import { expect, readDisk, saved, test } from './fixtures'
import { canvas, clickCell, dragThrough, openMap, tool } from './mapkit'
import { addCommand, createItem, editor, eventsTab, form, nameUnit, newEvent, shot, when } from './storykit'

const pickOnMap = (page: Page, label: string) => form(page).getByRole('button', { name: `${label}: Pick on the map` })

test('@smoke an event of every trigger and a command of every kind, a nested If, objectives, a switch and a variable: the files are exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await game.create('economy-tactics', 'Story')
  const mapFile = 'maps/crossroads.json'

  // --- Rules: a switch and a variable. Their ids are made from the names.
  await page.goto(`/p/${id}/rules`)
  await page.getByLabel('Add a switch').fill('Village saved')
  await page.getByLabel('Add a switch').press('Enter')
  await page.getByLabel('Add a variable').fill('Gold found')
  await page.getByLabel('Add a variable').press('Enter')
  await saved(page)
  expect(readDisk(id, 'game.json').switches).toEqual([{ id: 'village_saved', name: 'Village saved' }])
  expect(readDisk(id, 'game.json').variables).toEqual([{ id: 'gold_found', name: 'Gold found' }])
  await shot(page, 'rules-switches-variables')

  // --- Items: one item to give.
  await page.goto(`/p/${id}/items`)
  await createItem(page, 'Potion')
  await saved(page)

  // --- Map: story names for three units (two typed, one made from the unit's name).
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 2, 2, 'eirik')
  await nameUnit(page, 9, 2, 'gorm')
  await nameUnit(page, 3, 3, { unitName: 'Tanky' })
  await saved(page)
  const units = readDisk(id, mapFile).units
  expect(units.slice(0, 6).map((u: any) => u.id)).toEqual(['eirik', undefined, 'tanky', 'gorm', undefined, undefined])
  expect(units[2]).toMatchObject({ type: 'tank', name: 'Tanky', id: 'tanky' })

  await eventsTab(page).click()
  await expect(page.getByText('No events yet')).toBeVisible()

  // ===== 1. Battle starts: two kinds of speaker, a portrait, a multi-line text, the view =====
  await newEvent(page, 'opening')
  await expect(editor(page).getByLabel('When', { exact: true })).toHaveValue('start')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('Who speaks').selectOption({ label: 'Infantry (eirik)' })
  await form(page).getByLabel('Portrait').fill('eirik')
  await form(page).getByLabel('What is said').fill('Line one\nLine two')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await form(page).getByLabel('Name shown').fill('Villager')
  await form(page).getByLabel('What is said').fill('Who goes there?')
  await addCommand(page, 'Then', 'Move the view')
  await pickOnMap(page, 'A square').click()
  await expect(form(page).getByRole('status')).toContainText('Click a square on the map')
  await clickCell(page, 5, 5)
  await expect(form(page).getByLabel('A square: Column')).toHaveValue('6')
  await expect(form(page).getByLabel('A square: Row')).toHaveValue('6')
  await addCommand(page, 'Then', 'Move the view')
  await form(page).getByLabel('The view goes to').selectOption({ label: 'A unit' })
  await form(page).getByLabel('A unit', { exact: true }).selectOption({ label: 'Infantry (gorm)' })
  await addCommand(page, 'Then', 'Add a unit')
  await expect(form(page).getByLabel('Unit type', { exact: true })).toHaveValue('infantry')
  await form(page).getByLabel('Unit type', { exact: true }).selectOption({ label: 'Tank' })
  await form(page).getByLabel('Side', { exact: true }).selectOption({ label: 'Red Army' })
  await pickOnMap(page, 'Appears at').click()
  await clickCell(page, 10, 6)
  await form(page).getByLabel('Name', { exact: true }).fill('Reinforcement')
  await form(page).getByRole('checkbox', { name: 'Leader' }).check()
  await form(page).getByRole('button', { name: 'Fill from the name' }).click()
  await form(page).getByRole('switch', { name: 'Own items' }).check()
  await form(page).getByLabel('Items: Add an item…').selectOption({ label: 'Potion' })
  await saved(page)
  expect(readDisk(id, mapFile).events).toEqual([
    {
      id: 'opening', when: { battle: 'start' },
      do: [
        { say: 'Line one\nLine two', speaker: 'eirik', portrait: 'eirik' },
        { say: 'Who goes there?', speakerName: 'Villager' },
        { focus: { x: 5, y: 5 } },
        { focus: 'gorm' },
        { addUnit: { type: 'tank', side: 'red', x: 10, y: 6, name: 'Reinforcement', leader: true, id: 'reinforcement', items: ['potion'] } },
      ],
    },
  ])
  await shot(page, 'event-open')

  // ===== 2. Battle is won / lost / ends: switch, variables, a gift =====
  await newEvent(page, 'victory')
  await when(page, 'Battle is won')
  await addCommand(page, 'Then', 'Turn a switch on or off')
  await form(page).getByLabel('Switch', { exact: true }).selectOption({ label: 'Village saved' })
  await addCommand(page, 'Then', 'Add to a variable')
  await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Gold found' })
  await form(page).getByLabel('Add', { exact: true }).fill('5')

  await newEvent(page, 'defeat')
  await when(page, 'Battle is lost')
  await addCommand(page, 'Then', 'Set a variable')
  await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Gold found' })
  await form(page).getByLabel('Set it to').fill('7')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('We lost.')

  await newEvent(page, 'finish')
  await when(page, 'Battle ends (won or lost)')
  await addCommand(page, 'Then', 'Give resources')
  await form(page).getByLabel('Which side').selectOption({ label: 'Blue Army' })
  await form(page).getByLabel('Amount of Funds').fill('500')
  await addCommand(page, 'Then', 'Give an item')
  await form(page).getByLabel('Item', { exact: true }).selectOption({ label: 'Potion' })
  await form(page).getByLabel('Who gets it', { exact: true }).selectOption({ label: 'A specific unit' })
  await form(page).getByLabel('Who gets it: unit').selectOption({ label: 'Infantry (eirik)' })

  // ===== 3. A turn of a side, with a condition and Repeat; unit commands =====
  await newEvent(page, 'ambush')
  await when(page, 'A particular turn')
  await editor(page).getByLabel('Turn number').fill('4')
  await editor(page).getByLabel('On the turn of').selectOption({ label: 'Red Army' })
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A switch is on or off' })
  const cond = editor(page).getByRole('group', { name: 'Only if: Condition 1' })
  await cond.getByLabel('Switch', { exact: true }).selectOption({ label: 'Village saved' })
  await cond.getByLabel('Is', { exact: true }).selectOption({ label: 'Off' })
  await editor(page).getByRole('checkbox', { name: 'Repeat' }).check()
  await addCommand(page, 'Then', 'Move a unit')
  await form(page).getByLabel('Which unit', { exact: true }).selectOption({ label: 'Infantry (eirik)' })
  await pickOnMap(page, 'Goes to').click()
  await clickCell(page, 4, 4)
  await addCommand(page, 'Then', 'Remove a unit')
  await form(page).getByLabel('Which unit leaves', { exact: true }).selectOption({ label: 'Any unit of type…' })
  await form(page).getByLabel('Which unit leaves: unit type').selectOption({ label: 'Artillery' })
  await addCommand(page, 'Then', 'Change a unit’s side')
  await form(page).getByLabel('Which units', { exact: true }).selectOption({ label: 'A specific unit' })
  await form(page).getByLabel('Which units: unit').selectOption({ label: 'Infantry (gorm)' })
  await form(page).getByLabel('Join side').selectOption({ label: 'Blue Army' })
  await addCommand(page, 'Then', 'Change the objectives')
  await form(page).getByLabel('Objective text').fill('Hold out')
  await form(page).getByLabel('Add a way to win…').selectOption({ label: 'Survive some rounds' })
  await form(page).getByLabel('Add a way to lose…').selectOption({ label: 'A round begins' })
  await addCommand(page, 'Then', 'A team wins')
  await form(page).getByLabel('Which team wins').selectOption({ label: 'Blue Army' })
  await addCommand(page, 'Then', 'A team loses')
  await form(page).getByLabel('Which team loses').selectOption({ label: 'Red Army' })

  // ===== 4. Every turn: conditions of every kind, and an If three levels deep =====
  await newEvent(page, 'tick')
  await when(page, 'Every turn')
  await expect(editor(page).getByRole('checkbox', { name: 'Repeat' })).toBeChecked()
  await expect(editor(page).getByRole('checkbox', { name: 'Repeat' })).toBeDisabled()
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A variable compares to a number' })
  const c1 = editor(page).getByRole('group', { name: 'Only if: Condition 1' })
  await c1.getByLabel('Variable', { exact: true }).selectOption({ label: 'Gold found' })
  await c1.getByLabel('Comparison').selectOption({ label: 'is at least' })
  await c1.getByLabel('Number').fill('3')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A unit is alive' })
  const c2 = editor(page).getByRole('group', { name: 'Only if: Condition 2' })
  await c2.getByLabel('Which unit', { exact: true }).selectOption({ label: 'A specific unit' })
  await c2.getByLabel('Which unit: unit').selectOption({ label: 'Infantry (eirik)' })
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A unit is defeated' })
  const c3 = editor(page).getByRole('group', { name: 'Only if: Condition 3' })
  await c3.getByLabel('Which unit', { exact: true }).selectOption({ label: 'Any unit of side…' })
  await c3.getByLabel('Which unit: side').selectOption({ label: 'Red Army' })
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'The round number compares to a number' })
  const c4 = editor(page).getByRole('group', { name: 'Only if: Condition 4' })
  await c4.getByLabel('Comparison').selectOption({ label: 'is above' })
  await c4.getByLabel('Round number').fill('1')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A unit carries an item' })
  const c5 = editor(page).getByRole('group', { name: 'Only if: Condition 5' })
  await c5.getByLabel('Item', { exact: true }).selectOption({ label: 'Potion' })
  await addCommand(page, 'Then', 'If… then… otherwise…')
  await form(page).getByLabel('If 1: Add a condition…').selectOption({ label: 'A switch is on or off' })
  await form(page).getByLabel('Switch', { exact: true }).selectOption({ label: 'Village saved' })
  await addCommand(page, 'If 1: Then', 'If… then… otherwise…')
  await addCommand(page, 'If 1: Then, If 1: Then', 'If… then… otherwise…')
  await addCommand(page, 'If 1: Then, If 1: Then, If 1: Then', 'Say something')
  await form(page).getByLabel('What is said').fill('three levels deep')
  await addCommand(page, 'If 1: Then, If 1: Then, If 1: Otherwise', 'Say something')
  await form(page).getByLabel('What is said').fill('else of the third level')
  await addCommand(page, 'If 1: Otherwise', 'Say something')
  await form(page).getByLabel('What is said').fill('else of the first level')

  // ===== 5. A unit reaches an area: the area is dragged out on the map =====
  await newEvent(page, 'arrive')
  await when(page, 'A unit reaches an area')
  await editor(page).getByLabel('Which unit reaches it', { exact: true }).selectOption({ label: 'Any unit of side…' })
  await editor(page).getByLabel('Which unit reaches it: side').selectOption({ label: 'Blue Army' })
  await editor(page).getByRole('button', { name: 'When: area: Pick on the map' }).click()
  await dragThrough(page, [[4, 3], [5, 3], [6, 4]])
  await expect(editor(page).getByLabel('When: area: Left column')).toHaveValue('5')
  await expect(editor(page).getByLabel('When: area: Top row')).toHaveValue('4')
  await expect(editor(page).getByLabel('When: area: Width')).toHaveValue('3')
  await expect(editor(page).getByLabel('When: area: Height')).toHaveValue('2')
  await expect(canvas(page)).toHaveAttribute('data-areas', 'area:4,3,3,2')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('Arrived!')

  // ===== 6. A unit is defeated =====
  await newEvent(page, 'gorm_falls')
  await when(page, 'A unit is defeated')
  await editor(page).getByLabel('Which unit is defeated', { exact: true }).selectOption({ label: 'A specific unit' })
  await editor(page).getByLabel('Which unit is defeated: unit').selectOption({ label: 'Infantry (gorm)' })
  await editor(page).getByLabel('Defeated by', { exact: true }).selectOption({ label: 'Any unit of team…' })
  await editor(page).getByLabel('Defeated by: team').selectOption({ label: 'Blue Army' })
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('Gorm falls.')

  // ===== 7. A fight begins (by tag) =====
  await newEvent(page, 'clash')
  await when(page, 'A fight begins')
  await editor(page).getByLabel('One of the fighters is', { exact: true }).selectOption({ label: 'Any unit with tag…' })
  await editor(page).getByLabel('One of the fighters is: tag').fill('flier')
  await editor(page).getByRole('checkbox', { name: 'Repeat' }).check()
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('Fight!')

  // ===== 8. Two units talk: the only place "End the turn" can go =====
  await newEvent(page, 'parley')
  await expect(editor(page).getByRole('option', { name: /End the turn/ })).toBeDisabled()
  await when(page, 'Two units talk')
  await editor(page).getByLabel('Who talks', { exact: true }).selectOption({ label: 'Infantry (eirik)' })
  await editor(page).getByLabel('Talks to', { exact: true }).selectOption({ label: 'Infantry (gorm)' })
  await editor(page).getByLabel('Menu button').fill('Parley')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('Peace?')
  await addCommand(page, 'Then', 'End the turn')
  await saved(page)

  // ---------- the exact JSON ----------
  const written = readDisk(id, mapFile)
  expect(written.events).toEqual([
    {
      id: 'opening', when: { battle: 'start' },
      do: [
        { say: 'Line one\nLine two', speaker: 'eirik', portrait: 'eirik' },
        { say: 'Who goes there?', speakerName: 'Villager' },
        { focus: { x: 5, y: 5 } },
        { focus: 'gorm' },
        { addUnit: { type: 'tank', side: 'red', x: 10, y: 6, name: 'Reinforcement', leader: true, id: 'reinforcement', items: ['potion'] } },
      ],
    },
    { id: 'victory', when: { battle: 'won' }, do: [{ setSwitch: 'village_saved', to: true }, { addVariable: 'gold_found', by: 5 }] },
    { id: 'defeat', when: { battle: 'lost' }, do: [{ setVariable: 'gold_found', to: 7 }, { say: 'We lost.' }] },
    { id: 'finish', when: { battle: 'end' }, do: [{ giveResource: { funds: 500 }, side: 'blue' }, { giveItem: 'potion', to: { unit: 'eirik' } }] },
    {
      id: 'ambush', when: { turn: 4, side: 'red' }, if: [{ switch: 'village_saved', is: false }], repeat: true,
      do: [
        { moveUnit: 'eirik', x: 4, y: 4 },
        { removeUnit: { type: 'artillery' } },
        { changeSide: { unit: 'gorm' }, side: 'blue' },
        { objective: { text: 'Hold out', win: [{ survive: 5 }], lose: [{ round: 5 }] } },
        { win: 'blue' },
        { lose: 'red' },
      ],
    },
    {
      id: 'tick', when: { turn: 'every' },
      if: [
        { variable: 'gold_found', op: '>=', value: 3 },
        { alive: { unit: 'eirik' } },
        { dead: { side: 'red' } },
        { round: { op: '>', value: 1 } },
        { carries: 'potion' },
      ],
      do: [{
        if: [{ round: { op: '>=', value: 2 } }, { switch: 'village_saved' }],
        then: [{ if: [{ round: { op: '>=', value: 2 } }], then: [{ if: [{ round: { op: '>=', value: 2 } }], then: [{ say: 'three levels deep' }], else: [{ say: 'else of the third level' }] }] }],
        else: [{ say: 'else of the first level' }],
      }],
    },
    { id: 'arrive', when: { reach: { x: 4, y: 3, w: 3, h: 2 }, by: { side: 'blue' } }, do: [{ say: 'Arrived!' }] },
    { id: 'gorm_falls', when: { defeated: { unit: 'gorm' }, by: { team: 'blue' } }, do: [{ say: 'Gorm falls.' }] },
    { id: 'clash', when: { fight: { tag: 'flier' } }, repeat: true, do: [{ say: 'Fight!' }] },
    { id: 'parley', when: { talk: 'eirik', to: 'gorm', label: 'Parley' }, do: [{ say: 'Peace?' }, { endTurn: true }] },
  ])
  expect(Object.keys(written).slice(-2)).toEqual(['units', 'events'])
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByRole('button', { name: 'No problems' })).toBeVisible()
  // The only hint left is about the portrait that has no picture yet (the loader does not mind).
  await expect(page.getByTestId('map-warnings').getByRole('listitem')).toHaveCount(1)
  await expect(page.getByTestId('map-warnings')).toContainText('portrait “eirik” has no picture')

  // The list says what each event does, in plain words.
  await page.getByRole('button', { name: 'All events' }).click()
  await expect(page.getByTestId('event-opening')).toContainText('The battle starts → Infantry says “Line one Line two”, Villager says “Who goes there?” and 3 more')
  await expect(page.getByTestId('event-ambush')).toContainText('Turn 4, Red Army’s turn → Moves Infantry to 5, 5, removes any Artillery and 4 more')
  await expect(page.getByTestId('event-arrive')).toContainText('Any unit of side Blue Army reaches the area 5, 4 (3 × 2) → Says “Arrived!”')
  await shot(page, 'events-list')
})
