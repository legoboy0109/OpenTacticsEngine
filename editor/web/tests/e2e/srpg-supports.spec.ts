// E4 of docs/SRPG.md: the Supports screen (settings, pairs, levels, bonus, a conversation edited with the scene editor) and the story
// pieces about supports (command, condition, trigger, in a map's events and in a campaign scene), through the real editor. The proof
// that matters: the exact JSON the editor wrote, and that the game loader accepts it (/problems is empty).
import type { APIRequestContext, Page } from '@playwright/test'
import { expectNoHScroll, editorUrl, expect, readDisk, saved, section, test } from './fixtures'
import { newCharacter, switchOn } from './characterskit'
import { openCampaign, openChapter, addSceneCommand, sceneForm } from './campaignkit'
import { openMap } from './mapkit'
import { addCommand, editor, eventsTab, form, newEvent, when } from './storykit'

/** Resizes the window to the page's height first: a full-page capture would smear the sticky sidebar over the content. */
const shot = async (page: Page, name: string) => {
  if (!process.env.OTE_SHOT_DIR) return
  const w = page.viewportSize()!.width
  await page.setViewportSize({ width: w, height: 900 })
  const h = await page.evaluate(() => document.documentElement.scrollHeight)
  await page.setViewportSize({ width: w, height: Math.min(h, 3000) })
  await page.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png` })
  await page.setViewportSize({ width: w, height: 900 })
}

/** A strategy-rpg game with characters and supports switched on and three characters, two of them placed on the map. */
async function setup(page: Page, request: APIRequestContext, id: string): Promise<void> {
  await switchOn(page, id) // Rules -> characters on -> the Characters screen
  await page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name: 'Rules', exact: true }).click()
  await page.getByTestId('srpg-switches').getByRole('switch', { name: 'This game has supports' }).check()
  await section(page, 'Characters').click()
  for (const n of ['Eirik', 'Lyra', 'Bram']) await newCharacter(page, n)
  await saved(page)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.ford.units = [{ character: 'eirik', side: 'blue', x: 1, y: 3 }, { character: 'lyra', side: 'blue', x: 1, y: 4 }, ...doc.maps.ford.units.slice(2)]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })).ok()).toBeTruthy()
}

test('@smoke a pair with levels, a bonus and a conversation: the file is exactly that, and the game loads', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Supports')
  await setup(page, request, id)
  await page.goto(editorUrl(id, 'supports'))
  await expect(page.getByRole('heading', { name: 'Supports', level: 1 })).toBeVisible()
  await expect(page.getByText('No pairs yet').first()).toBeVisible()
  await shot(page, 'supports-empty')

  // --- A new pair: the first two characters, one level, nothing else written.
  await page.getByRole('button', { name: 'New pair' }).click()
  await expect(page.getByRole('heading', { name: 'Eirik and Lyra', level: 2 })).toBeVisible()
  await saved(page)
  expect(readDisk(id, 'data/supports.json')).toEqual({ pairs: [{ a: 'eirik', b: 'lyra', levels: [{ points: 20 }] }] })
  // The check: no way of earning points yet.
  await expect(page.getByTestId('support-checks')).toContainText('Level 1 can never be reached')

  // --- The settings card: defaults are not written.
  const rules = page.getByTestId('support-rules')
  await rules.getByLabel('Next to each other at the end of a turn').fill('1')
  await rules.getByLabel('Fighting side by side').fill('2')
  await rules.getByLabel('How close counts as near').fill('4')
  await rules.getByLabel('Most levels one character can hold').fill('4')
  await rules.getByLabel('Conversations are played').selectOption('both') // the default: nothing is written for it
  await saved(page)
  expect(readDisk(id, 'data/supports.json').rules).toEqual({ points: { endTurnAdjacent: 1, fightTogether: 2 }, range: 4, maxLevels: 4 })
  await expect(page.getByTestId('support-checks')).toHaveCount(0)

  // --- Levels: a second level, names for both, a bonus and a conversation for the first.
  await page.getByRole('button', { name: 'Add a level' }).click()
  await rules.getByLabel('Name of level 1').fill('C')
  await rules.getByLabel('Name of level 2').fill('B')
  await page.getByLabel('Level 2: points needed').fill('60')
  const l1 = page.getByTestId('support-level-1')
  await l1.getByLabel('Add a bonus').fill('Hit chance')
  await l1.getByLabel('Add a bonus').press('Enter')
  await l1.getByLabel('Hit chance Amount').fill('5')
  await l1.getByLabel('Add a bonus').fill('Damage taken')
  await l1.getByLabel('Add a bonus').press('Enter')
  await l1.getByLabel('Damage taken Amount').fill('-2')
  await l1.getByLabel('Add a bonus').fill('Strength')
  await l1.getByLabel('Add a bonus').press('Enter')
  await l1.getByLabel('Strength Amount').fill('1')

  const talk1 = page.getByTestId('support-talk-1')
  await expect(talk1.getByRole('option', { name: 'Add a unit' })).toHaveCount(0) // only lines of dialogue
  await expect(talk1.getByRole('option', { name: 'Say something' })).toHaveCount(1)
  await talk1.getByLabel('Level C: conversation: Add a command…', { exact: true }).selectOption({ label: 'Say something' })
  await talk1.locator('.command-form').getByLabel('Who speaks').selectOption({ label: 'Eirik (eirik)' })
  await talk1.locator('.command-form').getByLabel('What is said').fill('You never miss.')
  await talk1.getByLabel('Level C: conversation: Add a command…', { exact: true }).selectOption({ label: 'Say something' })
  await talk1.locator('.command-form').getByLabel('Who speaks').selectOption({ label: 'Lyra (lyra)' })
  await talk1.locator('.command-form').getByLabel('What is said').fill('I miss plenty. You just look away.')
  await saved(page)
  expect(readDisk(id, 'data/supports.json')).toEqual({
    rules: { levelNames: ['C', 'B'], points: { endTurnAdjacent: 1, fightTogether: 2 }, range: 4, maxLevels: 4 },
    pairs: [{
      a: 'eirik', b: 'lyra',
      levels: [
        { points: 20, bonus: { hit: 5, damageTaken: -2, stats: { str: 1 } }, talk: [{ say: 'You never miss.', speaker: 'eirik' }, { say: 'I miss plenty. You just look away.', speaker: 'lyra' }] },
        { points: 60 },
      ],
    }],
  })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'supports-pair-level')

  // Level 2 is not a conversation: the card says it is gained at once. A level can be removed, the last one cannot.
  await expect(page.getByTestId('support-level-2')).toContainText('No conversation')
  await page.getByRole('button', { name: 'Remove level B' }).click()
  await expect(page.getByRole('button', { name: /^Remove level/ })).toHaveCount(0)
  await saved(page)
  expect(readDisk(id, 'data/supports.json').pairs[0].levels).toHaveLength(1)
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, 'data/supports.json').pairs[0].levels).toHaveLength(2)

  // --- Another pair; a character cannot be paired with itself nor with a partner it already has.
  await page.getByRole('button', { name: 'New pair' }).click()
  await expect(page.getByRole('heading', { name: 'Eirik and Bram', level: 2 })).toBeVisible()
  await expect(page.getByLabel('Second character').locator('option')).toHaveText(['Bram'])
  await expect(page.getByLabel('First character').locator('option')).toHaveText(['Eirik', 'Lyra'])
  await page.getByLabel('First character').selectOption('lyra')
  await expect(page.getByRole('heading', { name: 'Lyra and Bram', level: 2 })).toBeVisible()
  expect(await game.problems(id)).toEqual([])
  await saved(page)
  expect(readDisk(id, 'data/supports.json').pairs.map((p: any) => `${p.a}.${p.b}`)).toEqual(['eirik.lyra', 'lyra.bram'])
  // Neither Lyra nor Bram is on the map together: the check says so (Bram is not placed at all).
  await expect(page.getByTestId('support-checks')).toContainText('Lyra and Bram: These two never meet')
  await expect(page.getByTestId('support-checks')).not.toContainText('Eirik and Lyra')

  // --- The game runs: the engine loads what the editor wrote (the loader's Problems are empty above).
})

test('the story pieces: Add support points, the support level condition and the trigger, on a map and in a scene', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Support story')
  await setup(page, request, id)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const put = await request.put(`/api/projects/${id}`, {
    data: {
      baseRevision: doc.revision,
      supports: { rules: { points: { heal: 1 } }, pairs: [{ a: 'eirik', b: 'lyra', levels: [{ points: 10 }, { points: 30 }] }] },
      campaign: { chapters: [{ id: 'ch1', name: 'The Ford', map: 'ford' }] },
    },
  })
  expect(put.ok(), await put.text()).toBeTruthy()

  // --- Map events.
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'bond')
  await when(page, 'A support level is reached')
  await editor(page).getByLabel('Which pair').selectOption({ label: 'Eirik and Lyra' })
  await editor(page).getByLabel('Which level').selectOption({ label: 'Level 2' })
  await addCommand(page, 'Then', 'Add support points')
  await expect(form(page).getByLabel('Which pair')).toHaveValue('eirik.lyra')
  await form(page).getByLabel('Points to add').fill('-5')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('What is said').fill('Closer.')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A support level compares to a number' })
  const cond = editor(page).getByRole('group', { name: 'Only if: Condition 1' })
  await expect(cond.getByLabel('Which pair')).toHaveValue('eirik.lyra')
  await cond.getByLabel('Their support level').selectOption('>')
  await cond.getByLabel('Level', { exact: true }).fill('0')
  await saved(page)
  const ev = readDisk(id, 'maps/ford.json').events[0]
  expect(ev.when).toEqual({ support: ['eirik', 'lyra'], level: 2 })
  expect(ev.do[0]).toEqual({ addSupport: ['eirik', 'lyra'], by: -5 })
  expect(ev.if).toEqual([{ support: ['eirik', 'lyra'], op: '>', value: 0 }])
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'supports-story-forms')

  // --- A campaign scene offers the command and the condition, nothing else new.
  await page.goto(editorUrl(id, 'campaign'))
  await openCampaign(page)
  await openChapter(page, 'The Ford')
  await addSceneCommand(page, 'before', 'Before the battle', 'Add support points')
  await sceneForm(page, 'before').getByLabel('Points to add').fill('7')
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toEqual([{ addSupport: ['eirik', 'lyra'], by: 7 }])
  expect(await game.problems(id)).toEqual([])
})

test('deleting a pair says what story it takes with it; deleting a character removes its pairs; one undo brings it all back', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Delete pair')
  await setup(page, request, id)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.ford.events = [{ id: 'bond', when: { battle: 'start' }, do: [{ addSupport: ['eirik', 'lyra'], by: 5 }] }]
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps, supports: { pairs: [{ a: 'eirik', b: 'lyra', levels: [{ points: 10 }] }] } } })
  expect(put.ok(), await put.text()).toBeTruthy()

  await page.goto(editorUrl(id, 'supports', 'eirik.lyra'))
  await page.getByRole('button', { name: 'Delete' }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Gives 5 support points to Eirik and Lyra')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(readDisk(id, 'data/supports.json')).toEqual({ pairs: [] })
  // The command about the pair goes; the event it was in stays (empty).
  expect(readDisk(id, 'maps/ford.json').events).toEqual([{ id: 'bond', when: { battle: 'start' }, do: [] }])
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, 'data/supports.json').pairs).toHaveLength(1)
  expect(readDisk(id, 'maps/ford.json').events[0].do).toHaveLength(1)

  // Deleting a character names the pair in the question.
  await page.goto(editorUrl(id, 'characters', 'lyra'))
  await page.getByRole('button', { name: 'Delete' }).click()
  await expect(page.getByRole('dialog')).toContainText('the pair “eirik” and “lyra”')
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(readDisk(id, 'data/supports.json')).toEqual({ pairs: [] })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'supports-problem-none')
})

test('@smoke the sticky switch: characters and supports on with nothing added, Problems is empty, and both stay on after a reload', async ({ page, game }) => {
  const id = await game.create('blank', 'Sticky')
  await page.goto(editorUrl(id, 'rules'))
  const sw = page.getByTestId('srpg-switches')
  await sw.getByRole('switch', { name: 'This game has characters' }).check()
  await sw.getByRole('switch', { name: 'This game has supports' }).check()
  await saved(page)
  expect(readDisk(id, 'data/supports.json')).toEqual({ pairs: [] })
  expect(await game.problems(id)).toEqual([])
  await page.reload()
  await expect(sw.getByRole('switch', { name: 'This game has characters' })).toBeChecked()
  await expect(sw.getByRole('switch', { name: 'This game has supports' })).toBeChecked()
  await section(page, 'Supports').click()
  await expect(page.getByText('No pairs yet').first()).toBeVisible()
  // With no characters a pair cannot be made: the screen says what to do first.
  await expect(page.getByText('A pair needs two characters')).toBeVisible()
  expect(await game.problems(id)).toEqual([])
})

test('@smoke on a 360 px phone: the screen, a pair with its levels and a conversation fit without sideways scrolling', async ({ page, game, request }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Phone supports')
  await setup(page, request, id)
  await page.goto(editorUrl(id, 'supports'))
  await expect(page.getByText('No pairs yet').first()).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'supports-empty-phone')
  await page.getByRole('button', { name: 'New pair' }).click()
  await expect(page.getByRole('heading', { name: 'Eirik and Lyra', level: 2 })).toBeVisible()
  await page.getByRole('button', { name: 'Add a level' }).click()
  const l1 = page.getByTestId('support-level-1')
  await l1.getByLabel('Add a bonus').fill('Hit chance')
  await l1.getByLabel('Add a bonus').press('Enter')
  await l1.getByLabel('Level 1: conversation: Add a command…', { exact: true }).selectOption({ label: 'Say something' })
  await l1.locator('.command-form').getByLabel('What is said').fill('A long line of talk that should wrap onto several lines instead of pushing the page sideways on a phone.')
  await expectNoHScroll(page)
  await shot(page, 'supports-pair-phone')
})
