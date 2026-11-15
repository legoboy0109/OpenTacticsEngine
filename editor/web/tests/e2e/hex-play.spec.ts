// Play on hexagon games: the embedded player starts and reports ready (the player's own hexagon drawing is checked with the
// player), and the Sound checkbox of the Play panel (on by default, remembered, adds noaudio=1 when off).
import type { Page } from '@playwright/test'
import { expect, requirePlayer, test } from './fixtures'
import { hexGame, type Layout } from './hexkit'
import { openMap } from './mapkit'

test.beforeEach(() => requirePlayer())

const panel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })
async function play(page: Page): Promise<void> {
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(panel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
}

for (const layout of ['pointy', 'flat'] as Layout[]) {
  test(`@smoke ${layout}: Play starts a hexagon game and reports ready`, async ({ page, game }) => {
    const id = hexGame(layout)
    await page.addInitScript(() => {
      ;(window as unknown as { __msgs: string[] }).__msgs = []
      window.addEventListener('message', (e) => (window as unknown as { __msgs: string[] }).__msgs.push(String(e.data?.type)))
    })
    await openMap(page, id, 'skirmish')
    await play(page)
    expect(await panel(page).locator('iframe').getAttribute('src')).toBe(`/player/embed.html?project=${id}&map=skirmish`)
    expect(await page.evaluate(() => (window as unknown as { __msgs: string[] }).__msgs)).toContain('ote-player-ready')
    expect(await game.problems(id)).toEqual([])
  })
}

test('@smoke the Sound checkbox: on by default, off adds noaudio=1, and it is remembered', async ({ page }) => {
  const id = hexGame('pointy')
  await openMap(page, id, 'skirmish')
  await play(page)
  const sound = panel(page).getByRole('checkbox', { name: 'Sound' })
  await expect(sound).toBeChecked()
  await sound.uncheck()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(panel(page).locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=skirmish&noaudio=1`)
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  // closed and opened again, even after a reload: still off
  await page.reload()
  await play(page)
  await expect(panel(page).getByRole('checkbox', { name: 'Sound' })).not.toBeChecked()
  await expect(panel(page).locator('iframe')).toHaveAttribute('src', /noaudio=1$/)
  await panel(page).getByRole('checkbox', { name: 'Sound' }).check()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(panel(page).locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=skirmish`)
})
