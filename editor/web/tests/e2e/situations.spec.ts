// What this round added, through the real editor and helper: the stage commands (bring a speaker on stage, move one, a fade on leaving),
// the move-the-view speed, changing a side's music from an event, music by situation and steps by movement type on the Sound screen,
// the preparation / promotion / chapter-card slots, sounds on the frames of a battle animation, and a portrait's facing and talk /
// blink frames. The files written are checked on disk and the game's own loader (the helper's check) must accept them; and a phone.
import { gotoSection, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'
import { addCommand, eventsTab, form, newEvent, shot } from './storykit'
import { openSound, soundGame } from './soundkit'
import { openArt, seedArt, solidPng } from './artkit'

test('@smoke music by situation and steps by movement type on the Sound screen: the file is exactly that and the game loads', async ({ page, game, request }) => {
  test.setTimeout(120_000)
  const id = await soundGame(request, game, 'Situations')
  await openSound(page, id)
  const card = page.getByTestId('situations-card')
  await expect(card).toBeVisible()
  // A side's own turn music (the sides of the maps), and few enemies with its number.
  const sideRows = card.locator('[data-testid^="phase-"]')
  expect(await sideRows.count()).toBeGreaterThan(0)
  const firstSide = ((await sideRows.first().getAttribute('data-testid')) ?? '').replace('phase-', '')
  await sideRows.first().getByRole('combobox').selectOption({ label: 'win' })
  await card.getByLabel('Track', { exact: true }).selectOption({ label: 'battle' })
  await card.getByLabel('Hostile units left (at most)').fill('3')
  // The new slots of the standard set: the preparation screen's music, and a jingle for a chapter's title card (a track that plays once).
  await page.getByTestId('slot-prepMusic').getByRole('combobox').selectOption({ label: 'battle' })
  await page.getByTestId('slot-chapterCard').getByRole('combobox').selectOption({ label: 'win' })
  // Steps by movement type: a sound and the time between steps.
  const steps = page.getByTestId('steps-by-move-type').locator('li').first()
  await steps.getByRole('combobox').selectOption({ label: 'step' })
  await steps.getByLabel(/time between steps/).fill('0.25')
  await saved(page)
  const use = readDisk(id, 'data/audio.json').use
  expect(use.phaseMusic).toEqual({ [firstSide]: 'win' })
  expect(use.fewEnemies).toEqual({ music: 'battle', count: 3 })
  expect(use.prepMusic).toBe('battle')
  expect(use.chapterCard).toBe('win')
  const mt = readDisk(id, 'game.json').moveTypes[0]
  expect(mt.sounds).toEqual({ step: 'step', stepInterval: 0.25 })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'situations-card')
  // A track that repeats cannot be a jingle: the check says so before Play.
  await page.getByTestId('slot-promotion').getByRole('combobox').selectOption({ label: 'battle' })
  await expect(page.getByText('repeats, but this plays once').first()).toBeVisible()
  await page.getByTestId('slot-promotion').getByRole('combobox').selectOption({ label: 'Nothing' })
})

test('the stage commands and the music change in a map event, and the focus speed', async ({ page, game, request }) => {
  test.setTimeout(150_000)
  const id = await soundGame(request, game, 'StageCommands')
  await openMap(page, id, (await (await request.get(`/api/projects/${id}`)).json()).game.maps[0].id)
  await eventsTab(page).click()
  await newEvent(page, 'curtain')
  await addCommand(page, 'Then', 'Bring a speaker on stage')
  await form(page).getByLabel('Who comes on stage').fill('Mira')
  await form(page).getByLabel('Place on the stage').selectOption({ label: 'Far right' })
  await form(page).getByLabel('How', { exact: true }).selectOption({ label: 'Fade in' })
  await addCommand(page, 'Then', 'Move a speaker on stage')
  await form(page).getByLabel('Who moves').fill('Mira')
  await form(page).getByLabel('Place on the stage').selectOption({ label: 'Left of the middle' })
  await addCommand(page, 'Then', 'Send a speaker away')
  await form(page).getByLabel('Who leaves').fill('Mira')
  await form(page).getByLabel('How', { exact: true }).selectOption({ label: 'Fade out' })
  await addCommand(page, 'Then', 'Move the view')
  await form(page).getByLabel('How the view scrolls').selectOption({ label: 'At a set speed' })
  await form(page).getByLabel('Scroll speed').fill('4')
  await addCommand(page, 'Then', 'Change a side’s music')
  await form(page).getByLabel('What it plays').selectOption({ label: 'A track' })
  await form(page).getByLabel('Music', { exact: true }).selectOption({ label: 'win' })
  await saved(page)
  const mapId = (await (await request.get(`/api/projects/${id}`)).json()).game.maps[0].id
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const ev = readDisk(id, `maps/${mapId}.json`).events.find((e: any) => e.id === 'curtain' || e.name === 'curtain') ?? doc.maps[mapId].events.at(-1)
  const cmds = ev.do
  expect(cmds[0]).toMatchObject({ enter: 'Mira', position: 'farRight', transition: 'fade' })
  expect(cmds[1]).toMatchObject({ move: 'Mira', position: 'midLeft' })
  expect(cmds[2]).toMatchObject({ leave: 'Mira', transition: 'fade' })
  expect(cmds[3]).toMatchObject({ focus: { x: 0, y: 0 }, speed: 4 })
  expect(cmds[4].setPhaseMusic).toMatchObject({ music: 'win' })
  expect(typeof cmds[4].setPhaseMusic.side).toBe('string')
  await shot(page, 'stage-commands')
  expect(await game.problems(id)).toEqual([])
})

test('a portrait: which way it looks, and its talk and blink frames', async ({ page, game, request }) => {
  test.setTimeout(120_000)
  const id = await game.create('economy-tactics', 'PortraitFrames')
  await seedArt(request, id, { faces: solidPng(144, 48, [90, 60, 40, 255]) }, { sheets: [{ id: 'faces', file: 'art/faces.png', cellWidth: 48, cellHeight: 48, name: 'Faces' }] })
  await openArt(page, id)
  const portraits = page.getByTestId('portraits')
  await portraits.getByLabel('Name of a new portrait').fill('eirik')
  await portraits.getByRole('button', { name: 'Add portrait' }).click()
  await portraits.getByLabel('eirik: Looks to the').selectOption({ label: 'The left' })
  await portraits.getByLabel('eirik: Talking frames').fill('2,1  3,1')
  await portraits.getByLabel('eirik: Blinking frames').fill('x')
  await expect(portraits.getByRole('alert')).toContainText('is not a piece')
  await portraits.getByLabel('eirik: Blinking frames').fill('3,1')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').portraits.eirik).toMatchObject({ faces: 'left', talk: [[1, 0], [2, 0]], blink: [[2, 0]] })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'portrait-frames')
})

test('@phone the new cards and forms fit a phone', async ({ page, game, request }) => {
  test.setTimeout(150_000)
  await page.setViewportSize({ width: 360, height: 740 })
  const id = await soundGame(request, game, 'PhoneSituations')
  await openSound(page, id)
  await expect(page.getByTestId('situations-card')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'situations-360')
  const mapId = (await (await request.get(`/api/projects/${id}`)).json()).game.maps[0].id
  await openMap(page, id, mapId)
  await eventsTab(page).click()
  await newEvent(page, 'phone')
  await addCommand(page, 'Then', 'Bring a speaker on stage')
  await expect(form(page).getByLabel('Place on the stage')).toBeVisible()
  await expectNoHScroll(page)
  await addCommand(page, 'Then', 'Change a side’s music')
  await expect(form(page).getByLabel('What it plays')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'stage-commands-360')
  await gotoSection(page, 'Art')
  await expectNoHScroll(page)
})
