// The Sound screen on a phone (360 x 740, touch): all three parts, the empty state, the files and the pickers fit, with no
// sideways scrolling and nothing cut off.
import type { Page } from '@playwright/test'
import { expect, expectNoHScroll, editorUrl, test } from './fixtures'
import { shot } from './storykit'
import { openSound, silentGame, soundGame } from './soundkit'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

/** Nothing sticks out of the screen: no element's right edge is past the window (a clipped row would hide a control). */
async function nothingWider(page: Page): Promise<void> {
  const worst = await page.evaluate(() => {
    let max = 0
    let who = ''
    for (const el of document.querySelectorAll<HTMLElement>('main *')) {
      if (el.closest('[hidden]') || getComputedStyle(el).position === 'fixed') continue
      const r = el.getBoundingClientRect()
      if (r.width > 0 && r.right > max) { max = r.right; who = `${el.tagName}.${el.className}` }
    }
    return { max, who }
  })
  expect(worst.max, `${worst.who} sticks out`).toBeLessThanOrEqual(361)
}

test('the empty Sound screen at 360 px', async ({ page, request, game }) => {
  const id = await silentGame(request, game, 'strategy-rpg', 'Phone Empty Sound')
  await openSound(page, id)
  await expectNoHScroll(page)
  await expect(page.getByRole('button', { name: 'Add music' })).toBeVisible()
  await expect(page.getByRole('button', { name: 'Add a sound' })).toBeVisible()
  await nothingWider(page)
  await shot(page, 'sound-phone-empty')
})

test('music, sounds and "when things happen" at 360 px: every part fits, with controls you can reach', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Phone Sound')
  await openSound(page, id)
  await expectNoHScroll(page)
  for (const part of ['music-card', 'sounds-card', 'events-card', 'sound-files']) {
    const el = page.getByTestId(part)
    await el.scrollIntoViewIfNeeded()
    await expect(el).toBeVisible()
    const box = (await el.boundingBox())!
    expect(box.x).toBeGreaterThanOrEqual(0)
    expect(box.x + box.width, part).toBeLessThanOrEqual(361)
    await expectNoHScroll(page)
  }
  // play buttons are big enough to tap, and the picker of a row is not squeezed to nothing
  const play = (await page.getByTestId('play-music:battle').boundingBox())!
  expect(play.width).toBeGreaterThanOrEqual(32)
  expect(play.height).toBeGreaterThanOrEqual(32)
  const pick = (await page.getByTestId('slot-titleMusic').getByRole('combobox').boundingBox())!
  expect(pick.width).toBeGreaterThan(200)
  await nothingWider(page)
  await shot(page, 'sound-phone-top')
  await page.getByTestId('events-card').scrollIntoViewIfNeeded()
  await shot(page, 'sound-phone-events')
  // tapping play on a phone plays
  await page.getByTestId('play-sound:hit').tap()
  await expect.poll(() => page.evaluate(() => (document.querySelector('audio[data-testid="sound-preview"]') as HTMLAudioElement | null)?.paused), { timeout: 8000 }).toBe(false)
})

test('the pickers on other screens and the story command forms at 360 px', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Phone Pickers')
  await page.goto(editorUrl(id, 'weapons'))
  await page.getByRole('list', { name: 'Weapons' }).getByRole('button').first().click() // on a phone the list comes first
  await expect(page.getByTestId('sounds-weapons')).toBeVisible()
  await expectNoHScroll(page)
  await nothingWider(page)
  await shot(page, 'sound-phone-weapon')
  await page.goto(editorUrl(id, 'campaign'))
  await page.getByRole('list', { name: 'Chapters' }).getByRole('button', { name: /^One/ }).click()
  await expect(page.getByLabel('Before the battle: Music for this scene')).toBeVisible()
  await expectNoHScroll(page)
  await nothingWider(page)
  await shot(page, 'sound-phone-scene')
})
