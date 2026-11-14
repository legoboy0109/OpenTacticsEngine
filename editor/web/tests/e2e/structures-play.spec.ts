// The real engine on what the editor wrote: Play (the web player) draws the structures, and the native simulator (ote-sim)
// plays the saved game to the end with captures, income and building going through structures.
import { execFileSync } from 'node:child_process'
import { existsSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import { bareGame } from './artkit'
import { diskPath, expect, readDisk, requirePlayer, saved, test } from './fixtures'
import { clickCell, openMap, pickSide, pickStructure, tool } from './mapkit'
import { countColor, decodePng, diffCount } from './png'

const panel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })
const frameShot = async (page: Page) => decodePng(await panel(page).locator('.play-frame').screenshot())

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

test('Play draws the structures the editor placed, in their colour', async ({ page, request, game }) => {
  requirePlayer()
  const id = await bareGame(request, game, 'economy-tactics', 'Play Structures')
  // Give the city a colour nothing else in the game has, so its blocks can be counted in the player's picture.
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, structures: doc.structures.map((s: any) => (s.id === 'city' ? { ...s, color: '#ff00ff' } : s)) } })
  expect(put.ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickSide(page, 'Red Army').click()
  await pickStructure(page, 'City').click()
  await clickCell(page, 4, 6)
  await saved(page)
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(panel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  await settle(page)
  expect(countColor(await frameShot(page), [255, 0, 255], 8), 'magenta city blocks in the picture').toBeGreaterThan(300)
})

const sim = (): string => path.resolve(import.meta.dirname, '../../', process.env.OTE_SIM_BIN ?? '../../build/ote-sim')

test('the saved game plays to the end in the real engine: structures are captured and units are built on them', async ({ page, request, game }) => {
  if (!existsSync(sim())) throw new Error(`ote-sim not found at ${sim()}. Build it (cmake --build build) or set OTE_SIM_BIN.`)
  const id = await bareGame(request, game, 'economy-tactics', 'Sim Structures')
  await openMap(page, id, 'crossroads')
  // A neutral 2 x 2 headquarters-sized structure in the middle, placed by hand.
  await tool(page, 'Structures').click()
  await pickSide(page, 'Nobody').click()
  await pickStructure(page, 'Headquarters').click()
  await clickCell(page, 5, 4)
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').structures.at(-1)).toEqual({ type: 'hq', x: 5, y: 4 })
  expect(await game.problems(id)).toEqual([])
  const out = execFileSync(sim(), [diskPath(id, ''), 'crossroads', '--runs', '3'], { encoding: 'utf8' })
  const summary = out.split('\n').find((l) => l.startsWith('summary:'))!
  expect(summary, out).toBeTruthy()
  expect(Number(/avg captures ([\d.]+)/.exec(summary)![1]), summary).toBeGreaterThan(0)
  expect(Number(/avg builds ([\d.]+)/.exec(summary)![1]), summary).toBeGreaterThan(0)
})
