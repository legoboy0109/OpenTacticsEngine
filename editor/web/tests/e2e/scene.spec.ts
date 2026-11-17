// Story scene presentation (docs/EVENTS.md "Scene presentation") through the real editor: the Backgrounds list on the Art screen (add,
// name, picture, reorder), the "Scene" group of commands in a map event (and in a campaign scene, where weather is not offered), the
// new keys on a line (place, goes on by itself), on Move the view (scroll time, wait) and on Move a unit (walk), the checks that mirror
// the loader, rename and delete following through, the exact JSON that was written and that the real game loader accepts, and a phone.
import type { Page } from '@playwright/test'
import { openArt, seedArt, solidPng } from './artkit'
import { addSceneCommand, openCampaign, openChapter, scene, sceneForm } from './campaignkit'
import { expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { openMap, tool } from './mapkit'
import { addCommand, editor, eventsTab, form, nameUnit, newEvent, shot } from './storykit'

/** A game with two images (a whole-picture backdrop and a sheet of pieces) and no backgrounds yet. */
async function prepare(game: { create: (t: string, n?: string) => Promise<string> }, request: Parameters<typeof seedArt>[0], name: string): Promise<string> {
  const id = await game.create('economy-tactics', name)
  await seedArt(request, id, { forest: solidPng(240, 135, [40, 90, 60, 255]), places: solidPng(240, 68, [120, 90, 70, 255]) }, {
    sheets: [
      { id: 'forest', file: 'art/forest.png', cellWidth: 240, cellHeight: 135, name: 'Forest' },
      { id: 'places', file: 'art/places.png', cellWidth: 120, cellHeight: 68, name: 'Places' },
    ],
  })
  return id
}

const backgroundsCard = (page: Page) => page.getByTestId('backgrounds')

test('@smoke the Backgrounds list: add, name, choose a whole image or one piece, reorder', async ({ page, game, request }) => {
  test.setTimeout(90_000)
  const id = await prepare(game, request, 'Backgrounds')
  await page.goto(`/p/${id}/art`)
  const card = backgroundsCard(page)
  await expect(card).toContainText('No backgrounds yet')
  await card.getByLabel('Name of a new background').fill('forest_dusk')
  await card.getByRole('button', { name: 'Add background' }).click()
  await card.getByLabel('Name of a new background').fill('hall')
  await card.getByLabel('Name of a new background').press('Enter')
  await saved(page)
  // A new background is the first image, whole (no "cell" key).
  expect(readDisk(id, 'data/graphics.json').backgrounds).toEqual({ forest_dusk: { sheet: 'forest' }, hall: { sheet: 'forest' } })
  // A bad or repeated name is refused where it is typed.
  await card.getByLabel('Name of a new background').fill('hall')
  await expect(card.getByRole('alert')).toContainText('already a background')
  await card.getByLabel('Name of a new background').fill('')
  // The second one becomes one piece of the second image.
  await card.getByRole('button', { name: 'Choose picture…' }).nth(1).click()
  const dlg = page.getByRole('dialog', { name: 'Picture for “hall”' })
  await dlg.getByLabel('Image').selectOption({ label: 'Places' })
  await dlg.getByLabel('Backgrounds').selectOption({ label: 'One piece of the image' })
  const grid = dlg.locator('canvas').first()
  await expect(grid).toBeVisible()
  const box = (await grid.boundingBox())!
  await page.mouse.click(box.x + box.width * 0.75, box.y + box.height * 0.5)  // the second of two pieces
  await shot(page, 'background-picker')
  await dlg.getByRole('button', { name: 'Use this picture' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').backgrounds.hall).toEqual({ sheet: 'places', cell: [1, 0] })
  // And back to the whole image.
  await card.getByRole('button', { name: 'Choose picture…' }).nth(1).click()
  await page.getByRole('dialog', { name: 'Picture for “hall”' }).getByLabel('Backgrounds').selectOption({ label: 'The whole image' })
  await page.getByRole('dialog', { name: 'Picture for “hall”' }).getByRole('button', { name: 'Use this picture' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').backgrounds.hall).toEqual({ sheet: 'places' })
  await card.getByRole('button', { name: 'Move background hall up' }).click()
  await saved(page)
  expect(Object.keys(readDisk(id, 'data/graphics.json').backgrounds)).toEqual(['hall', 'forest_dusk'])
  await shot(page, 'backgrounds')
  expect(await game.problems(id)).toEqual([])
})

test('@smoke a scene in a map event: every new command, the keys on a line, on moving the view and on moving a unit: the file is exactly that and the game loads', async ({ page, game, request }) => {
  test.setTimeout(180_000)
  const id = await prepare(game, request, 'SceneMap')
  // Two backgrounds through the API (the list is covered above) and two named units.
  let doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.graphics.backgrounds = { forest_dusk: { sheet: 'forest' }, hall: { sheet: 'places', cell: [0, 0] } }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics: doc.graphics } })).ok()).toBeTruthy()

  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 2, 2, 'eirik')
  await eventsTab(page).click()
  await newEvent(page, 'intro')

  // --- the Scene group of the command picker
  const menu = editor(page).getByLabel('Then: Add a command…', { exact: true })
  for (const label of ['Show a background', 'Fade the screen', 'Wait', 'Shake the screen', 'Send a speaker away', 'Clear the stage', 'Set the weather', 'Narrate'])
    await expect(menu.locator('option', { hasText: label })).toHaveCount(1)

  await addCommand(page, 'Then', 'Fade the screen')
  await form(page).getByLabel('How long it takes').fill('300')
  await form(page).getByLabel('Hex code').fill('#102030')
  await addCommand(page, 'Then', 'Show a background')
  await form(page).getByLabel('Background', { exact: true }).selectOption({ label: 'forest_dusk' })
  await addCommand(page, 'Then', 'Fade the screen')
  await form(page).getByLabel('The screen goes').selectOption({ label: 'Back in' })
  await form(page).getByLabel('How long it takes').fill('300')
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('Who speaks').selectOption({ label: 'Infantry (eirik)' })
  await form(page).getByLabel('Where the speaker stands').selectOption({ label: 'Left' })
  await form(page).getByLabel('What is said').fill('The forest is quiet.')
  await form(page).getByLabel('Goes on by itself after').fill('900')
  await addCommand(page, 'Then', 'Wait')
  await form(page).getByLabel('Pause').fill('400')
  await addCommand(page, 'Then', 'Shake the screen')
  await form(page).getByLabel('How long').fill('300')
  await form(page).getByLabel('How hard').fill('4')
  await addCommand(page, 'Then', 'Narrate')
  await form(page).getByLabel('What is narrated').fill('Something stirs.')
  await addCommand(page, 'Then', 'Send a speaker away')
  await form(page).getByLabel('Who leaves').fill('eirik')
  await addCommand(page, 'Then', 'Show a background')
  await form(page).getByLabel('Background', { exact: true }).selectOption({ label: 'Take the background away (back to the map)' })
  await form(page).getByLabel('How it changes').selectOption({ label: 'A cross-fade' })
  await form(page).getByLabel('Cross-fade time').fill('250')
  await addCommand(page, 'Then', 'Clear the stage')
  await addCommand(page, 'Then', 'Set the weather')
  await form(page).getByLabel('Weather').selectOption({ label: 'Snow' })
  await addCommand(page, 'Then', 'Move the view')
  await form(page).getByLabel('Scroll time').fill('600')
  await form(page).getByLabel('The next command waits for the scroll').uncheck()
  await addCommand(page, 'Then', 'Move a unit')
  await form(page).getByLabel('Which unit').selectOption({ label: 'Infantry (eirik)' })
  await form(page).getByLabel('The unit walks there').check()
  await form(page).getByLabel('The next command waits until it has walked').uncheck()
  await shot(page, 'scene-move-form')
  await saved(page)

  const events = readDisk(id, 'maps/crossroads.json').events
  expect(events).toEqual([{ id: 'intro', when: { battle: 'start' }, do: [
    { fade: 'out', ms: 300, color: '#102030' },
    { background: 'forest_dusk' },
    { fade: 'in', ms: 300 },
    { say: 'The forest is quiet.', speaker: 'eirik', position: 'left', auto: 900 },
    { wait: 400 },
    { shake: { ms: 300, strength: 4 } },
    { narrate: 'Something stirs.' },
    { leave: 'eirik' },
    { background: null, transition: 'fade', ms: 250 },
    { clearStage: true },
    { weather: 'snow' },
    { focus: { x: 0, y: 0 }, ms: 600, wait: false },
    { moveUnit: 'eirik', x: 0, y: 0, walk: true, wait: false },
  ] }])
  expect(await game.problems(id)).toEqual([])  // the real game loader accepts it
  await expect(page.getByTestId('commands-Then')).toContainText('Walks')
  await shot(page, 'scene-event')
})

test('@smoke the checks, rename and delete: an unknown background and a wrong pause are reported, a rename is followed, a delete takes the commands with it', async ({ page, game, request }) => {
  test.setTimeout(150_000)
  const id = await prepare(game, request, 'SceneRefs')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.graphics.backgrounds = { forest_dusk: { sheet: 'forest' }, hall: { sheet: 'places', cell: [0, 0] } }
  doc.maps.crossroads.events = [{ id: 'intro', when: { battle: 'start' }, do: [{ background: 'hall' }, { wait: 300 }, { background: 'forest_dusk' }] }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics: doc.graphics, maps: { crossroads: doc.maps.crossroads } } })).ok()).toBeTruthy()

  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Open event intro', exact: true }).click()
  // Rename in Art: both commands follow.
  await openArt(page, id)
  await backgroundsCard(page).getByLabel('Name of background 2').fill('great_hall')
  await saved(page)
  expect(Object.keys(readDisk(id, 'data/graphics.json').backgrounds)).toEqual(['forest_dusk', 'great_hall'])
  expect(readDisk(id, 'maps/crossroads.json').events[0].do.map((c: any) => c.background)).toEqual(['great_hall', undefined, 'forest_dusk'])

  // Delete one that a command uses: asks first, names what goes, and takes the command out in the same step.
  await backgroundsCard(page).getByRole('button', { name: 'Delete background great_hall' }).click()
  const dlg = page.getByRole('dialog', { name: 'Delete the background “great_hall”?' })
  await expect(dlg).toContainText('Shows the background “great_hall”')
  await dlg.getByRole('button', { name: 'Delete background' }).click()
  await saved(page)
  expect(Object.keys(readDisk(id, 'data/graphics.json').backgrounds)).toEqual(['forest_dusk'])
  expect(readDisk(id, 'maps/crossroads.json').events[0].do).toEqual([{ wait: 300 }, { background: 'forest_dusk' }])
  await page.getByRole('button', { name: 'Undo' }).click()  // one undo step brings the background and the command back
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').events[0].do.map((c: any) => c.background)).toEqual(['great_hall', undefined, 'forest_dusk'])
  expect(await game.problems(id)).toEqual([])

  // An unknown background is named in the event's checks (written by hand, as another tool might).
  const d2 = await (await request.get(`/api/projects/${id}`)).json()
  d2.maps.crossroads.events[0].do[1] = { background: 'swamp' }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: d2.revision, maps: { crossroads: d2.maps.crossroads } } })).ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Open event intro', exact: true }).click()
  await expect(page.getByTestId('event-checks')).toContainText('the background “swamp” is not one of this game’s')
  await shot(page, 'scene-check')
  // ... and a pause out of range, which the number box itself would not let a person type
  const d3 = await (await request.get(`/api/projects/${id}`)).json()
  d3.maps.crossroads.events[0].do[2] = { wait: 70000 }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: d3.revision, maps: { crossroads: d3.maps.crossroads } } })).ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Open event intro', exact: true }).click()
  await expect(page.getByTestId('event-checks')).toContainText('the wait is milliseconds, a whole number from 0 to 60000')
  // Fixing them in the forms clears the checks.
  await page.getByRole('button', { name: /^2\. / }).click()
  await form(page).getByLabel('Background', { exact: true }).selectOption({ label: 'hall' })
  await page.getByRole('button', { name: /^3\. / }).click()
  await form(page).getByLabel('Pause').fill('300')
  await expect(page.getByTestId('event-checks')).toHaveCount(0)
})

test('@smoke a campaign scene: the Scene commands are there, weather is not, and the file is what was built', async ({ page, game, request }) => {
  test.setTimeout(150_000)
  const id = await prepare(game, request, 'SceneCampaign')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.graphics.backgrounds = { forest_dusk: { sheet: 'forest' }, hall: { sheet: 'places', cell: [0, 0] } }
  doc.campaign = { name: 'Scenes', chapters: [{ id: 'ch1', name: 'One', map: 'crossroads' }] }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics: doc.graphics, campaign: doc.campaign } })).ok()).toBeTruthy()

  await page.goto(`/p/${id}/campaign`)
  await openCampaign(page)
  await openChapter(page, 'One')
  const menu = scene(page, 'before').getByLabel('Before the battle: Add a command…', { exact: true })
  for (const label of ['Show a background', 'Fade the screen', 'Wait', 'Shake the screen', 'Send a speaker away', 'Clear the stage', 'Narrate'])
    await expect(menu.locator('option', { hasText: label })).toHaveCount(1)
  await expect(menu.locator('option', { hasText: 'Set the weather' })).toHaveCount(0)  // a scene has no map
  await expect(menu.locator('option', { hasText: 'Move the view' })).toHaveCount(0)

  await addSceneCommand(page, 'before', 'Before the battle', 'Fade the screen')
  await sceneForm(page, 'before').getByLabel('How long it takes').fill('200')
  await addSceneCommand(page, 'before', 'Before the battle', 'Show a background')
  await sceneForm(page, 'before').getByLabel('Background', { exact: true }).selectOption({ label: 'hall' })
  await addSceneCommand(page, 'before', 'Before the battle', 'Fade the screen')
  await sceneForm(page, 'before').getByLabel('The screen goes').selectOption({ label: 'Back in' })
  await addSceneCommand(page, 'before', 'Before the battle', 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'before').getByLabel('Name shown').fill('Eirik')
  await sceneForm(page, 'before').getByLabel('Where the speaker stands').selectOption({ label: 'In the middle' })
  await sceneForm(page, 'before').getByLabel('What is said').fill('We camp here.')
  await addSceneCommand(page, 'before', 'Before the battle', 'Narrate')
  await sceneForm(page, 'before').getByLabel('What is narrated').fill('Night.')
  await sceneForm(page, 'before').getByLabel('Goes on by itself after').fill('800')
  await addSceneCommand(page, 'before', 'Before the battle', 'Shake the screen')
  await saved(page)
  await shot(page, 'scene-campaign')
  const before = readDisk(id, 'data/campaign.json').chapters[0].before
  expect(before).toEqual([
    { fade: 'out', ms: 200, color: '#000000' },
    { background: 'hall' },
    { fade: 'in', ms: 500 },
    { say: 'We camp here.', speakerName: 'Eirik', position: 'center' },
    { narrate: 'Night.', auto: 800 },
    { shake: { ms: 400, strength: 3 } },
  ])
  expect(await game.problems(id)).toEqual([])
  // Deleting the background a scene shows asks first and takes that command out of the scene too.
  await openArt(page, id)
  await backgroundsCard(page).getByRole('button', { name: 'Delete background hall' }).click()
  await expect(page.getByRole('dialog', { name: 'Delete the background “hall”?' })).toContainText('Shows the background “hall”')
  await page.getByRole('dialog').getByRole('button', { name: 'Delete background' }).click()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters[0].before.some((c: any) => c.background === 'hall')).toBe(false)
  expect(await game.problems(id)).toEqual([])
})

test('@smoke on a 360px phone the Backgrounds card and the scene forms fit', async ({ page, game, request }) => {
  test.setTimeout(120_000)
  const id = await prepare(game, request, 'ScenePhone')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.graphics.backgrounds = { forest_dusk: { sheet: 'forest' }, hall: { sheet: 'places', cell: [0, 0] } }
  doc.maps.crossroads.units[0].id = 'eirik'
  doc.maps.crossroads.events = [{ id: 'intro', when: { battle: 'start' }, do: [
    { fade: 'out', ms: 300, color: '#102030' }, { background: 'hall', transition: 'fade', ms: 400 }, { say: 'Hello.', speaker: 'eirik', position: 'right', auto: 500 },
    { shake: { ms: 300, strength: 3 } }, { narrate: 'Night.' }, { focus: { x: 1, y: 1 }, ms: 500 }, { moveUnit: 'eirik', x: 2, y: 2, walk: true },
  ] }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics: doc.graphics, maps: { crossroads: doc.maps.crossroads } } })).ok()).toBeTruthy()
  await page.setViewportSize({ width: 360, height: 740 })
  await page.goto(`/p/${id}/art`)
  await expect(backgroundsCard(page)).toBeVisible()
  await backgroundsCard(page).scrollIntoViewIfNeeded()
  await expectNoHScroll(page)
  await shot(page, 'backgrounds-phone')
  await backgroundsCard(page).getByRole('button', { name: 'Choose picture…' }).first().click()
  await expectNoHScroll(page)
  await shot(page, 'background-picker-phone')
  await page.getByRole('dialog').getByRole('button', { name: 'Cancel' }).click()

  await openMap(page, id, 'crossroads')
  await eventsTab(page).click()
  await page.getByRole('button', { name: 'Open event intro', exact: true }).click()
  for (let n = 1; n <= 7; n++) {
    await page.getByRole('button', { name: new RegExp(`^${n}\\. `) }).click()
    await expectNoHScroll(page)
    if (n === 1 || n === 2 || n === 3 || n === 6 || n === 7) await shot(page, `scene-command-${n}-phone`)
  }
})
