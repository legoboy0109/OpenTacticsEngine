// The campaign timeline (docs/EDITOR_UI.md "Campaign timeline") through the real editor: the boxes and arrows of a campaign with a
// choice branch, reordering by buttons and by dragging, adding / removing chapters and scenes, jumping into a box's editor and back,
// a level's events, problem badges, the keyboard, and a phone. Every change is checked in the saved campaign.json and by the real
// game loader (/problems is empty).
import type { Locator, Page } from '@playwright/test'
import { diskPath, expect, expectNoHScroll, readDisk, saved, test, writeDisk } from './fixtures'
import { branchingCampaign, box, chip, edge, openTimeline, timelineGame, timelineTab, view } from './timelinekit'

const order = (id: string): string[] => readDisk(id, 'data/campaign.json').chapters.map((c: any) => c.id)
const fit = (page: Page) => page.getByRole('button', { name: 'Fit all' }).click()
const transform = (page: Page) => page.locator('.tl-world').evaluate((el) => (el as HTMLElement).style.transform)
const openButton = (page: Page, nodeId: string): Locator => page.locator(`[data-node-open="${nodeId}"]`)

async function drag(page: Page, from: Locator, to: { x: number; y: number }): Promise<void> {
  const b = (await from.boundingBox())!
  await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2)
  await page.mouse.down()
  await page.mouse.move(b.x + b.width / 2 + 20, b.y + b.height / 2 + 6, { steps: 3 })
  await page.mouse.move(to.x, to.y, { steps: 12 })
  await page.mouse.up()
}

test('@smoke the timeline shows the boxes and arrows of a campaign with a choice branch, and the view is remembered', async ({ page, game }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  for (const b of ['crossing:before', 'crossing:level', 'crossing:after', 'tutorial:before', 'tutorial:level', 'valley:before', 'valley:level', 'summit:before', 'summit:level', 'home:level', 'home:after']) {
    await expect(box(page, b), b).toBeAttached()
  }
  await expect(page.locator('[data-testid^="box-"]')).toHaveCount(11)
  // the "Play the tutorial?" answer that jumps is its own labelled arrow; "No" falls through, so the order arrow stays
  await expect(edge(page, 'crossing:before', 'tutorial:before')).toHaveAttribute('aria-label', /Jump: .*The Crossing.* to Learning the Ropes.*“Yes”/)
  await expect(edge(page, 'crossing:before', 'crossing:level')).toHaveAttribute('data-kind', 'order')
  // "Which road next?": every answer jumps, so only the two labelled arrows leave the after scene
  await expect(page.locator('[data-testid^="edge-crossing:after>"]')).toHaveCount(2)
  await expect(edge(page, 'crossing:after', 'valley:before')).toHaveAttribute('aria-label', /The valley road/)
  await expect(edge(page, 'crossing:after', 'summit:before')).toHaveAttribute('aria-label', /The summit path/)
  await expect(page.getByTestId('label-crossing:after>summit:before')).toHaveText('“The summit path”')
  // "what changes" signs, with text alternatives; no scene summary (no first line of what is said)
  await expect(box(page, 'crossing:before').getByRole('img', { name: '1 question asked' })).toBeVisible()
  await expect(box(page, 'crossing:before').getByRole('img', { name: '1 switch or variable set' })).toBeVisible()
  await expect(box(page, 'crossing:before')).not.toContainText('We reach the ridge')
  await expect(box(page, 'crossing:level').getByRole('img', { name: /questions? asked/ })).toBeVisible()
  await expect(box(page, 'crossing:level')).toContainText('The Ridge')
  await expect(box(page, 'crossing:level').locator('img.tl-thumb')).toBeVisible()
  // the legend
  await page.getByTestId('timeline-legend').locator('summary').click()
  await expect(page.getByTestId('timeline-legend')).toContainText('A question is asked')
  expect(await game.problems(id)).toEqual([])
  // the view is remembered; the list view still works
  await page.reload()
  await expect(page.getByTestId('timeline')).toBeVisible()
  await expect(timelineTab(page)).toHaveAttribute('aria-selected', 'true')
  await page.getByRole('tab', { name: 'List' }).click()
  await expect(page.getByRole('heading', { name: 'Chapters', level: 2 })).toBeVisible()
  await expect(page.getByTestId('timeline')).toHaveCount(0)
})

test('@smoke reordering by the buttons: the saved order changes, the game still loads, Undo brings it back', async ({ page, game }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  await chip(page, 'summit').click()
  await expect(page.getByTestId('timeline-selected')).toContainText('Selected: The Summit')
  await page.getByRole('button', { name: 'Move earlier' }).click()
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'summit', 'valley', 'home'])
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Move later' }).click()
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'summit', 'home'])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'summit', 'valley', 'home'])
  // the first chapter cannot move earlier, the last not later
  await chip(page, 'crossing').click()
  await expect(page.getByRole('button', { name: 'Move earlier' })).toBeDisabled()
})

test('dragging a chapter\'s box moves the chapter: the saved order changes and the game loads', async ({ page, game }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  const home = (await box(page, 'home:after').boundingBox())!
  await drag(page, box(page, 'summit:level'), { x: home.x + home.width * 0.9, y: home.y + home.height / 2 })
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'home', 'summit'])
  expect(await game.problems(id)).toEqual([])
  // dragging empty space moves the picture, not a chapter
  const before = await transform(page)
  await drag(page, view(page), { x: 500, y: 700 })
  expect(await transform(page)).not.toBe(before)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'home', 'summit'])
})

test('adding: at the end, between two chapters (the story goes through it), and scenes', async ({ page, game }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  await page.getByRole('button', { name: 'Add a chapter at the end' }).click()
  await page.getByLabel('Name of the chapter').fill('Epilogue')
  await page.getByRole('button', { name: 'Create' }).click()
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'summit', 'home', 'epilogue'])
  // between Summit and Home: Summit's "Next chapter" (Home) now points at the new chapter
  await page.getByRole('button', { name: 'Add a chapter between The Summit and Home' }).click()
  await page.getByLabel('Name of the chapter').fill('Interlude')
  await page.getByRole('button', { name: 'Create' }).click()
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'summit', 'interlude', 'home', 'epilogue'])
  const camp = readDisk(id, 'data/campaign.json')
  expect(camp.chapters.find((c: any) => c.id === 'summit').next).toBe('interlude')
  await expect(page.getByTestId('timeline-unreached')).toHaveCount(0)
  // a scene is added empty (saved as []) and taken away again
  await chip(page, 'epilogue').click()
  await page.getByRole('button', { name: 'Add a scene after the battle' }).click()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters.find((c: any) => c.id === 'epilogue').after).toEqual([])
  await expect(box(page, 'epilogue:after')).toContainText('Empty scene')
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Remove the scene after the battle' }).click() // empty: no question
  await saved(page)
  expect('after' in readDisk(id, 'data/campaign.json').chapters.find((c: any) => c.id === 'epilogue')).toBe(false)
  // a scene with writing asks first
  await chip(page, 'summit').click()
  await page.getByRole('button', { name: 'Remove the scene before the battle' }).click()
  await expect(page.getByRole('dialog')).toContainText('Its 1 command will be removed')
  await page.getByRole('button', { name: 'Cancel' }).click()
  expect(readDisk(id, 'data/campaign.json').chapters.find((c: any) => c.id === 'summit').before).toHaveLength(1)
  await page.getByRole('button', { name: 'Remove the scene before the battle' }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Remove' }).click()
  await saved(page)
  expect('before' in readDisk(id, 'data/campaign.json').chapters.find((c: any) => c.id === 'summit')).toBe(false)
  expect(await game.problems(id)).toEqual([])
})

test('removing a chapter asks, names where it is used, and cleans the "go to"s and "next"s that named it', async ({ page, game }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  await chip(page, 'valley').click()
  await page.getByRole('button', { name: 'Remove the chapter' }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Delete the chapter “The Valley”?')
  await expect(dlg.locator('.usage-list')).toContainText('Goes on to chapter “The Valley”')
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  expect(order(id)).toContain('valley')
  await page.getByRole('button', { name: 'Remove the chapter' }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'summit', 'home'])
  expect(JSON.stringify(readDisk(id, 'data/campaign.json'))).not.toContain('"valley"')
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('timeline-selected')).toContainText('Select a chapter')
})

test('a scene box opens its editor, a level box the map; coming back keeps the view, the selection and the open level', async ({ page }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  await chip(page, 'valley').click()
  await page.getByTestId('expand-crossing').click()
  await drag(page, view(page), { x: 600, y: 650 }) // move the picture a bit
  const placed = await transform(page)
  await openButton(page, 'valley:before').click()
  await expect(page).toHaveURL(new RegExp(`/p/${id}/campaign/valley$`))
  await expect(page.getByTestId('scene-before')).toBeVisible()
  await expect(page.getByTestId('timeline-chapter')).toBeVisible()
  await page.getByTestId('back-to-timeline').click()
  await expect(page.getByTestId('timeline')).toBeVisible()
  expect(await transform(page)).toBe(placed)
  await expect(chip(page, 'valley')).toHaveAttribute('aria-pressed', 'true')
  await expect(openButton(page, 'valley:before')).toBeFocused()
  await expect(page.getByRole('button', { name: /Open the event stranger/ })).toBeVisible() // the level is still open
  // a level box: the map editor
  await openButton(page, 'valley:level').click()
  await expect(page).toHaveURL(new RegExp(`/p/${id}/maps/vale$`))
  await page.goBack()
  await expect(page.getByTestId('timeline')).toBeVisible()
  expect(await transform(page)).toBe(placed)
})

test('a level opens to show its events in rough order; an event opens in the map editor\'s Events tab', async ({ page }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  await page.getByTestId('expand-crossing').click()
  const rows = page.getByTestId('box-crossing:level').locator('.tl-event')
  await expect(rows).toHaveCount(3)
  await expect(rows.nth(0)).toContainText('The battle starts') // start of battle first
  await expect(rows.nth(1)).toContainText('Turn 2') // then the turn-numbered one
  await expect(rows.nth(2)).toContainText('reaches the square 3, 2') // then the conditional one
  await expect(page.getByTestId('box-crossing:level').locator('.tl-group').first()).toHaveText('At the start of the battle')
  await expect(rows.nth(2).getByRole('img', { name: /questions? asked/ })).toBeVisible()
  await expect(rows.nth(2).getByRole('img', { name: /switch or variable set/ })).toBeVisible()
  await rows.nth(2).click()
  await expect(page).toHaveURL(new RegExp(`/p/${id}/maps/ridge$`))
  await expect(page.getByRole('tab', { name: 'Events' })).toHaveAttribute('aria-selected', 'true')
  await expect(page.getByTestId('event-editor')).toBeVisible()
  await expect(page.getByTestId('event-editor').getByLabel('Name', { exact: true })).toHaveValue('gate')
  await page.goBack()
  await page.getByTestId('expand-crossing').click() // collapses again
  await expect(page.getByTestId('box-crossing:level').locator('.tl-event')).toHaveCount(0)
})

test('a problem in a scene shows a badge on its box (and on the level for a map event); the badge lists it', async ({ page }) => {
  const camp = branchingCampaign()
  camp.chapters.find((c: any) => c.id === 'valley').before = [{ say: '', speakerName: 'Narrator' }, { choice: '', answers: [{ text: 'A' }, { text: 'B' }] }]
  const id = timelineGame(camp)
  // ... and an event of the Ridge map with a line that says nothing
  const ridge = readDisk(id, 'maps/ridge.json')
  ridge.events.push({ id: 'broken', when: { turn: 5 }, do: [{ say: '', speakerName: 'Scout' }] })
  writeDisk(id, 'maps/ridge.json', JSON.stringify(ridge, null, 2))
  await openTimeline(page, id)
  await fit(page)
  await expect(page.getByTestId('badge-crossing:level')).toHaveText('1')
  await expect(page.getByTestId('badge-crossing:level')).toHaveAttribute('title', /Event “broken”/)
  await page.getByTestId('expand-crossing').click()
  await expect(page.getByRole('button', { name: /Open the event broken.*1 problem/ })).toBeVisible()
  await page.getByTestId('expand-crossing').click()
  const badge = page.getByTestId('badge-valley:before')
  await expect(badge).toBeVisible()
  await expect(badge).toHaveText('2')
  await expect(badge).toHaveAttribute('aria-label', /2 problems: .*the text to say is empty/)
  await expect(badge).toHaveAttribute('title', /text to say is empty/i)
  await expect(page.getByTestId('badge-crossing:before')).toHaveCount(0)
  await expect(page.getByTestId('badge-valley:level')).toHaveCount(0)
  // the badge opens the Problems list
  await badge.click()
  await expect(page.getByRole('region', { name: 'Problems' })).toBeVisible()
})

test('the keyboard: arrow keys move between boxes in play order, Space selects the chapter, [ ] move it, Enter opens', async ({ page, game }) => {
  const id = timelineGame()
  await openTimeline(page, id)
  await fit(page)
  await openButton(page, 'crossing:before').focus()
  await page.keyboard.press('ArrowRight')
  await expect(openButton(page, 'crossing:level')).toBeFocused()
  await page.keyboard.press('ArrowRight')
  await expect(openButton(page, 'crossing:after')).toBeFocused()
  await page.keyboard.press('ArrowRight')
  await expect(openButton(page, 'tutorial:before')).toBeFocused()
  await page.keyboard.press('ArrowLeft')
  await expect(openButton(page, 'crossing:after')).toBeFocused()
  await page.keyboard.press('End')
  await expect(openButton(page, 'home:after')).toBeFocused()
  await page.keyboard.press('Home')
  await expect(openButton(page, 'crossing:before')).toBeFocused()
  // Up / Down: the row of the chapters that branch off
  await openButton(page, 'crossing:after').focus()
  await page.keyboard.press('ArrowDown')
  await expect(page.locator(':focus')).toHaveAttribute('data-node-open', /^(tutorial|summit|valley):/)
  // Space selects the chapter, ] moves it later
  await openButton(page, 'summit:level').focus()
  await page.keyboard.press('Space')
  await expect(chip(page, 'summit')).toHaveAttribute('aria-pressed', 'true')
  await page.keyboard.press(']')
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'home', 'summit'])
  await expect(openButton(page, 'summit:level')).toBeFocused()
  await page.keyboard.press('[')
  await saved(page)
  expect(order(id)).toEqual(['crossing', 'tutorial', 'valley', 'summit', 'home'])
  expect(await game.problems(id)).toEqual([])
  await page.keyboard.press('Enter')
  await expect(page).toHaveURL(new RegExp(`/p/${id}/maps/summit$`))
  // the keys are listed on screen
  await page.goBack()
  await page.getByRole('button', { name: 'Keys' }).click()
  await expect(page.getByRole('note', { name: 'Keys for the timeline' })).toContainText('[ moves it earlier')
})

test('chapters nothing leads to are set apart in a band with a note; a loop back does not break the picture', async ({ page }) => {
  const camp = branchingCampaign()
  camp.chapters.push({ id: 'lost', name: 'The Lost Chapter', map: 'summit' })
  camp.chapters.find((c: any) => c.id === 'home').next = 'crossing' // a loop; "lost" is no longer after home
  camp.chapters.find((c: any) => c.id === 'home').after = [{ choice: 'Again?', answers: [{ text: 'Once more', do: [{ goto: 'crossing' }] }, { text: 'Enough' }] }]
  const id = timelineGame(camp)
  await openTimeline(page, id)
  await fit(page)
  await expect(page.getByTestId('timeline-unreached')).toContainText('Nothing leads to these chapters')
  await expect(chip(page, 'lost')).toContainText('Nothing leads here')
  await expect(box(page, 'lost:level')).toHaveAttribute('data-unreached', 'true')
  await expect(page.locator('[data-testid="edge-home:after>crossing:before"][data-kind="goto"]')).toHaveAttribute('data-back', 'true')
  const lost = (await box(page, 'lost:level').boundingBox())!
  const home = (await box(page, 'home:level').boundingBox())!
  expect(lost.y).toBeGreaterThan(home.y + home.height)
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
  test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

  test('360 px: no sideways page scroll, the picture pans and pinch-zooms by touch, buttons are 44 px, chapters move by button', async ({ page }) => {
    const id = timelineGame()
    await openTimeline(page, id)
    await expectNoHScroll(page)
    const cdp = await page.context().newCDPSession(page)
    await view(page).scrollIntoViewIfNeeded()
    const v = (await view(page).boundingBox())!
    const send = (type: 'touchStart' | 'touchMove' | 'touchEnd', pts: Array<{ x: number; y: number }>) =>
      cdp.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map((p, i) => ({ x: p.x, y: p.y, id: i })) })
    // one finger on empty space pans
    const before = await transform(page)
    const y = Math.min(v.y + v.height - 40, 700)
    await send('touchStart', [{ x: v.x + 200, y }])
    for (let i = 1; i <= 6; i++) await send('touchMove', [{ x: v.x + 200 - i * 20, y: y - i * 8 }])
    await send('touchEnd', [])
    await expect.poll(() => transform(page)).not.toBe(before)
    // two fingers pinch
    const k0 = await page.getByTestId('timeline-zoom').innerText()
    const cx = v.x + v.width / 2, cy = v.y + v.height / 2
    await send('touchStart', [{ x: cx - 30, y: cy }, { x: cx + 30, y: cy }])
    for (let i = 1; i <= 8; i++) await send('touchMove', [{ x: cx - 30 - i * 10, y: cy }, { x: cx + 30 + i * 10, y: cy }])
    await send('touchEnd', [])
    await expect.poll(() => page.getByTestId('timeline-zoom').innerText()).not.toBe(k0)
    await expectNoHScroll(page)
    // 44 px targets and the non-drag reorder
    await fit(page)
    await chip(page, 'summit').tap()
    await expect(page.getByTestId('timeline-selected')).toContainText('The Summit')
    for (const name of ['Move earlier', 'Move later', 'Fit all', 'Add a chapter at the end']) {
      const b = (await page.getByRole('button', { name }).boundingBox())!
      expect(b.height, name).toBeGreaterThanOrEqual(43.5)
    }
    await page.getByRole('button', { name: 'Move earlier' }).tap()
    await saved(page)
    expect(order(id)).toEqual(['crossing', 'tutorial', 'summit', 'valley', 'home'])
    await expectNoHScroll(page)
  })
})
