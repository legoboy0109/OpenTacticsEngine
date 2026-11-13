import type { Page } from '@playwright/test'
import { bareGame } from './artkit'
import { addSecondMap, editorUrl, expect, requirePlayer, saved, section, test } from './fixtures'
import { clickCell, openMap, pickTerrain, tab } from './mapkit'
import { countColor, decodePng, diffCount, distinctColors } from './png'

test.beforeEach(() => requirePlayer())

const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const panel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })
const frameShot = async (page: Page) => decodePng(await panel(page).locator('.play-frame').screenshot())

/** The game starts with a short intro (banner, fades) that ignores input; wait until the picture holds still. */
async function settle(page: Page): Promise<void> {
  let last = await frameShot(page)
  let calm = 0
  for (let i = 0; i < 40 && calm < 3; i++) {
    await page.waitForTimeout(250)
    const now = await frameShot(page)
    calm = diffCount(last, now) < 2500 ? calm + 1 : 0
    last = now
  }
  expect(calm, 'the game picture never settled').toBeGreaterThanOrEqual(3)
}

/** Press Play and wait until the player says it is ready (the loading veil goes away). */
async function play(page: Page): Promise<void> {
  await playButton(page).click()
  await expect(panel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
}

test('@smoke Play opens the embedded player on the map being edited; it posts ready and draws a real picture', async ({ page, game, request }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Play')
  const second = await addSecondMap(request, id)
  await page.addInitScript(() => {
    ;(window as unknown as { __msgs: string[] }).__msgs = []
    window.addEventListener('message', (e) => (window as unknown as { __msgs: string[] }).__msgs.push(String(e.data?.type)))
  })
  await openMap(page, id, second)
  await play(page)
  const src = await panel(page).locator('iframe').getAttribute('src')
  expect(src).toBe(`/player/embed.html?project=${id}&map=${second}`)
  expect(await page.evaluate(() => (window as unknown as { __msgs: string[] }).__msgs)).toContain('ote-player-ready')
  await expect(panel(page).getByLabel('Map')).toHaveValue(second)
  const img = await frameShot(page)
  expect(distinctColors(img)).toBeGreaterThan(8) // not a blank or single-colour frame
  expect(img.width).toBeGreaterThan(400)
  await expect(panel(page).getByText(/Controls:/)).toBeVisible()
})

test('Play from another section runs the first map', async ({ page, game, request }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'PlayFirst')
  await addSecondMap(request, id)
  await page.goto(editorUrl(id, 'rules'))
  await playButton(page).click()
  await expect(panel(page).locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=ford`)
})

test('@smoke keyboard input reaches the game without clicking it first', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'PlayKeys')
  await openMap(page, id, 'ford')
  await play(page)
  await settle(page)
  const before = await frameShot(page)
  await page.keyboard.press('z', { delay: 80 }) // confirm on the lord under the cursor: shows where he can move
  let during = before
  await expect.poll(async () => { during = await frameShot(page); return diffCount(before, during) }, { timeout: 10_000 }).toBeGreaterThan(5000)
  await page.keyboard.press('x', { delay: 80 }) // back out: the move range goes away again
  await expect.poll(async () => diffCount(during, await frameShot(page)), { timeout: 10_000 }).toBeGreaterThan(5000)
  // Esc went to the game, not to the dialog: the panel is still open.
  await expect(panel(page)).toBeVisible()
})

test('@smoke Restart runs the latest save; a changed side colour shows in the unit blocks', async ({ page, game, request }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'PlayRestart')
  await openMap(page, id, 'ford')
  await tab(page, 'Sides').click()
  await page.getByLabel('Blue Company colour (hex code)').fill('#ff00ff') // edit in the editor, then play at once: Play saves first
  await play(page)
  const magenta = await frameShot(page)
  expect(countColor(magenta, [255, 0, 255], 6)).toBeGreaterThan(150)

  // Change the colour behind the editor's back (another program), then Restart: the player reloads the saved files.
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.ford.sides[0].color = '#00ff00'
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })
  expect(put.ok()).toBeTruthy()
  await panel(page).getByRole('button', { name: 'Restart' }).click()
  await expect(page.getByTestId('play-loading')).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  const green = await frameShot(page)
  expect(countColor(green, [0, 255, 0], 6)).toBeGreaterThan(150)
  expect(countColor(green, [255, 0, 255], 6)).toBeLessThan(20)
})

test('the map picker switches maps; Close and Esc return focus to the Play button', async ({ page, game, request }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'PlayPick')
  const second = await addSecondMap(request, id)
  await page.goto(editorUrl(id, 'rules'))
  await play(page)
  await panel(page).getByLabel('Map').selectOption(second)
  await expect(panel(page).locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=${second}`)
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })

  await panel(page).getByRole('button', { name: 'Close' }).click()
  await expect(panel(page)).toBeHidden()
  await expect(playButton(page)).toBeFocused()

  await play(page)
  await panel(page).getByRole('button', { name: 'Restart' }).focus() // focus outside the frame: Esc closes
  await page.keyboard.press('Escape')
  await expect(panel(page)).toBeHidden()
  await expect(playButton(page)).toBeFocused()
})

test('Play with a problem shows the Problems panel instead and does not start', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'PlayProblem')
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'River').click()
  await clickCell(page, 1, 3) // a lord stands here: the game no longer loads
  await playButton(page).click()
  await expect(page.getByRole('region', { name: 'Problems' })).toContainText('Fix these before playing')
  await expect(page.getByRole('region', { name: 'Problems' })).toContainText('impassable')
  await expect(panel(page)).toHaveCount(0)
  expect(await game.problems(id)).not.toEqual([])
  // Fix it and Play works.
  await page.getByRole('button', { name: 'Close problems' }).click()
  await pickTerrain(page, 'Plain').click()
  await clickCell(page, 1, 3)
  await saved(page)
  await play(page)
})

test('an error from the player is shown readably with a way back', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'blank', 'PlayError')
  await page.route('**/player/embed.html*', (route) => route.fulfill({
    contentType: 'text/html',
    body: '<script>parent.postMessage({type:"ote-player-error", message:"Script error: boom at line 3"}, "*")</script>',
  }))
  await page.goto(editorUrl(id, 'rules'))
  await playButton(page).click()
  const err = page.getByTestId('play-error')
  await expect(err).toContainText('The game could not start')
  await expect(err).toContainText('Script error: boom at line 3')
  await err.getByRole('button', { name: 'Back to editing' }).click()
  await expect(panel(page)).toBeHidden()
  await expect(section(page, 'Rules')).toBeVisible()
})

test('while the player has not answered yet, a loading state shows', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'blank', 'PlayLoading')
  await page.route('**/player/embed.html*', (route) => route.fulfill({ contentType: 'text/html', body: '<p>quiet</p>' }))
  await page.goto(editorUrl(id, 'rules'))
  await playButton(page).click()
  await expect(page.getByTestId('play-loading')).toContainText('Starting the game')
})
