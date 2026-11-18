// The Play panel's "Start" (this map alone / from this chapter of the campaign) and "Test level" (off / automatic / a number): what the
// panel asks the embedded player for (the iframe's address carries the arguments), the greyed choices and their reasons, that the choices
// are remembered (per project; the level per map) across closing the panel and reloading the editor, and the 360 px phone layout.
// The prebuilt web player in OTE_PLAYER_DIR may be older than these options and then ignores them: this spec judges the ADDRESS only,
// never what the player does with it (that is tests/testplay_player_test.py).
import type { Page } from '@playwright/test'
import { copyDemo } from './campaignkit'
import { openMap } from './mapkit'
import { addSecondMap, editorUrl, expect, expectNoHScroll, requirePlayer, test } from './fixtures'

test.beforeEach(() => requirePlayer())

const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const panel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })
const src = (page: Page) => panel(page).locator('iframe').getAttribute('src')
const startBox = (page: Page) => panel(page).getByLabel('Start', { exact: true })
const levelBox = (page: Page) => panel(page).getByLabel('Test level', { exact: true })
const levelNumber = (page: Page) => panel(page).getByLabel('Test level number', { exact: true })

async function openPlay(page: Page, id: string, map: string): Promise<void> {
  await openMap(page, id, map)
  await playButton(page).click()
  await expect(panel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
}

test('@smoke Play: this map alone / from this chapter and the test level go into the player address, are remembered, and are greyed with a reason when they cannot apply', async ({ page, request }) => {
  test.setTimeout(120_000)
  const id = copyDemo()
  const base = `/player/embed.html?project=${id}`
  const second = await addSecondMap(request, id)

  // --- On a chapter's map: the plain address first, both starts offered, the level off.
  await openPlay(page, id, 'north_road')
  expect(await src(page)).toBe(`${base}&map=north_road`)
  await expect(startBox(page)).toHaveValue('map')
  await expect(startBox(page).locator('option', { hasText: /^From this chapter of the campaign \(The North Road\)$/ })).toBeEnabled()
  await expect(levelBox(page)).toHaveValue('off')
  await expect(levelNumber(page)).toHaveCount(0)

  // --- From this chapter: the story, started at the chapter whose map this is.
  await startBox(page).selectOption('chapter')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=pass`)

  // --- Test level: automatic, then a number (the box appears; it applies on Enter), then off again.
  await levelBox(page).selectOption('auto')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=pass&testlevel=auto`)
  await levelBox(page).selectOption('number')
  await expect(levelNumber(page)).toHaveValue('5')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=pass&testlevel=5`)
  await levelNumber(page).fill('12')
  await levelNumber(page).press('Enter')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=pass&testlevel=12`)
  await levelNumber(page).fill('150')  // out of range: flagged, nothing changes
  await levelNumber(page).press('Enter')
  await expect(levelNumber(page)).toHaveAttribute('aria-invalid', 'false')  // the box went back to the kept value
  expect(await src(page)).toContain('testlevel=12')

  // --- Remembered: close and open again, then reload the whole editor.
  await panel(page).getByRole('button', { name: 'Close' }).click()
  await expect(panel(page)).toBeHidden()
  await playButton(page).click()
  await expect(panel(page)).toBeVisible()
  expect(await src(page)).toBe(`${base}&campaign=1&chapter=pass&testlevel=12`)
  await page.reload()
  await playButton(page).click()
  await expect(startBox(page)).toHaveValue('chapter')
  await expect(levelBox(page)).toHaveValue('number')
  await expect(levelNumber(page)).toHaveValue('12')
  expect(await src(page)).toBe(`${base}&campaign=1&chapter=pass&testlevel=12`)

  // --- The level is per map; the start choice is per project: the Ford is a chapter's map too, and starts at its chapter with the level off.
  await panel(page).getByLabel('Map', { exact: true }).selectOption('bridge')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=ford`)
  await expect(levelBox(page)).toHaveValue('off')
  await levelBox(page).selectOption('auto')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=ford&testlevel=auto`)
  await panel(page).getByLabel('Map', { exact: true }).selectOption('north_road')
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1&chapter=pass&testlevel=12`)

  // --- A map that is no chapter's: "from this chapter" is greyed with the reason and the map alone is used (the level stays per map).
  await panel(page).getByLabel('Map', { exact: true }).selectOption(second)
  await expect.poll(() => src(page)).toBe(`${base}&map=${second}`)
  await expect(startBox(page)).toHaveValue('map')
  const off = startBox(page).locator('option', { hasText: /^From this chapter of the campaign/ })
  await expect(off).toBeDisabled()
  await expect(off).toContainText('this map is not a chapter')

  // --- The story picker already starts at chapter 1: the same choice is greyed there, with that reason.
  await panel(page).getByLabel('Map', { exact: true }).selectOption({ label: 'The story, from chapter 1' })
  await expect.poll(() => src(page)).toBe(`${base}&campaign=1`)
  await expect(startBox(page).locator('option', { hasText: /^From this chapter of the campaign/ })).toContainText('already starts at chapter 1')

  // --- Off removes the argument again.
  await panel(page).getByLabel('Map', { exact: true }).selectOption('north_road')
  await levelBox(page).selectOption('off')
  await startBox(page).selectOption('map')
  await expect.poll(() => src(page)).toBe(`${base}&map=north_road`)
})

test('Play: a game without a campaign greys "from this chapter" with the reason', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'NoCampaign')
  await page.goto(editorUrl(id, 'game'))
  await playButton(page).click()
  await expect(panel(page)).toBeVisible()
  const opt = startBox(page).locator('option', { hasText: /^From this chapter of the campaign/ })
  await expect(opt).toBeDisabled()
  await expect(opt).toContainText('this game has no campaign')
  await expect(page.getByRole('heading', { level: 1 }).first()).toBeVisible()
})

test.describe('at 360 px', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the Play panel with both choices and the number box fits the phone: nothing sideways, every control reachable', async ({ page }) => {
    test.setTimeout(90_000)
    const id = copyDemo()
    await openPlay(page, id, 'north_road')
    await startBox(page).selectOption('chapter')
    await levelBox(page).selectOption('number')
    await levelNumber(page).fill('9')
    await levelNumber(page).press('Enter')
    await expect.poll(() => src(page)).toBe(`/player/embed.html?project=${id}&campaign=1&chapter=pass&testlevel=9`)
    await expectNoHScroll(page)
    for (const box of [startBox(page), levelBox(page), levelNumber(page), panel(page).getByRole('button', { name: 'Close' })]) {
      await expect(box).toBeVisible()
      const b = (await box.boundingBox())!
      expect(b.x, 'control starts inside the screen').toBeGreaterThanOrEqual(0)
      expect(b.x + b.width, 'control ends inside the screen').toBeLessThanOrEqual(361)
    }
    const dlg = (await panel(page).boundingBox())!
    expect(dlg.width).toBeLessThanOrEqual(361)
    // The player area keeps room: the panel's header must not eat the screen.
    const stage = (await panel(page).locator('.play-stage').boundingBox())!
    expect(stage.height, 'the game area keeps most of the screen').toBeGreaterThan(300)
  })
})
