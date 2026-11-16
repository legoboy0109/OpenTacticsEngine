// Weapon ranks and stat caps (docs/SRPG.md section 7, editor step): the Rules switches and cards, the ladder, the pickers on weapons, classes,
// characters and placed units, the sticky switches and the clean-up, through the real editor. The proof that matters: the exact JSON the editor
// wrote, that the game loader accepts it (/problems is empty) and that the real engine loads it (Play).
import type { Page } from '@playwright/test'
import { bareGame } from './artkit'
import { editorUrl, expect, readDisk, requirePlayer, saved, test, diskPath } from './fixtures'
import { existsSync } from 'node:fs'
import { LADDER, shot } from './rankskit'

const sw = (page: Page, label: string) => page.getByTestId('srpg-switches').getByRole('switch', { name: label })
const RANKS = 'This game has weapon ranks'
const CAPS = 'This game has stat caps'
const rankRows = (page: Page) => page.getByTestId('rank-ladder').getByRole('listitem')
const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const playPanel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })

test('@smoke build a ladder, give a weapon and a class ranks, press Play: the files are exactly that and the real engine loads the game', async ({ page, game, request }) => {
  requirePlayer()
  const id = await bareGame(request, game, 'strategy-rpg', 'Ranks Play')

  // Weapons must be carried before a ladder can exist: the switch says so until they are.
  await page.goto(editorUrl(id, 'rules'))
  await expect(sw(page, RANKS)).toBeDisabled()
  await expect(page.getByTestId('srpg-switches')).toContainText('Weapon ranks need weapons that units carry')
  const items = page.locator('section.card').filter({ has: page.getByRole('heading', { name: 'Items', exact: true }) })
  await items.getByLabel('Weapons', { exact: true }).selectOption('carried')
  await sw(page, RANKS).check()
  await expect(page.getByTestId('ladder-empty')).toBeVisible()
  await saved(page)
  expect(readDisk(id, 'data/ranks.json')).toEqual({ growth: 'fixed', ranks: [] }) // the empty file is how the switch is remembered
  expect(await game.problems(id)).toEqual([])

  // The default ladder, then "grow with use".
  await page.getByRole('button', { name: 'Use the default ranks (E, D, C, B, A, S)' }).click()
  await expect(rankRows(page)).toHaveCount(6)
  await page.getByLabel('How ranks grow').selectOption('use')
  await saved(page)
  expect(readDisk(id, 'data/ranks.json')).toEqual(LADDER)
  await shot(page, 'rules-ranks')

  // Stat caps: one optional number per stat.
  await sw(page, CAPS).check()
  await page.getByLabel('Strength cap').fill('30')
  await page.getByLabel('HP cap').fill('60')
  await saved(page)
  expect(readDisk(id, 'data/caps.json')).toEqual({ caps: { str: 30, hp: 60 } })

  // Weapons: groups, a required rank, and (growth is "use") the experience per use.
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await expect(page.getByTestId('weapon-rank-needs-group')).toBeVisible()
  await page.getByLabel('Group', { exact: true }).fill('sword')
  await page.goto(editorUrl(id, 'weapons', 'iron_lance'))
  await page.getByLabel('Group', { exact: true }).fill('lance')
  const card = page.getByTestId('weapon-rank')
  await card.getByLabel('Rank needed to use it').selectOption('c')
  await card.getByLabel('Experience per use').fill('2')
  await saved(page)
  const lance = readDisk(id, 'data/weapons.json').weapons.find((w: any) => w.id === 'iron_lance')
  expect(lance).toMatchObject({ group: 'lance', rank: 'c', rankXp: 2 })
  await shot(page, 'weapon-rank')

  // The Lord class: groups it can wield, and its starting rank in one of them.
  await page.goto(editorUrl(id, 'units', 'lord'))
  const wield = page.getByRole('group', { name: 'Can wield groups' })
  await wield.getByRole('checkbox', { name: 'sword' }).check()
  await wield.getByRole('checkbox', { name: 'lance' }).check()
  await page.getByTestId('unit-ranks').getByLabel('Starting rank in sword').selectOption('d')
  await page.getByTestId('unit-caps').getByLabel('Strength cap').fill('24')
  await saved(page)
  const lord = readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord')
  expect(lord).toMatchObject({ wields: ['lance', 'sword'], ranks: { sword: 'd' }, caps: { str: 24 } })
  await expect(page.getByTestId('unit-caps').getByLabel('Speed cap')).toHaveAttribute('placeholder', 'no cap')
  await expect(page.getByTestId('unit-caps').getByLabel('Strength cap')).toHaveValue('24')
  await expect(page.getByTestId('unit-caps').getByLabel('HP cap')).toHaveAttribute('placeholder', '60')
  await shot(page, 'unit-ranks')
  expect(await game.problems(id)).toEqual([])

  // The real engine loads it.
  await playButton(page).click()
  await expect(playPanel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})

test('@smoke the switches stick: each feature on with nothing in it stays on after a reload, and off removes its file', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Ranks Sticky')
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('Weapons', { exact: true }).selectOption('carried')
  await sw(page, CAPS).check()
  await saved(page)
  expect(readDisk(id, 'data/caps.json')).toEqual({ caps: {} })
  await page.reload()
  await expect(sw(page, CAPS)).toBeChecked()
  await expect(page.getByTestId('caps-card')).toBeVisible()

  await sw(page, RANKS).check()
  await saved(page)
  await page.reload()
  await expect(sw(page, RANKS)).toBeChecked()
  await expect(sw(page, CAPS)).toBeChecked()
  await expect(page.getByTestId('ranks-card')).toBeVisible()
  expect(readDisk(id, 'data/ranks.json')).toEqual({ growth: 'fixed', ranks: [] })
  expect(await game.problems(id)).toEqual([]) // an empty ladder and empty caps are the same game as none

  // Nothing is in them, so switching off asks nothing and deletes the files.
  await sw(page, RANKS).uncheck()
  await sw(page, CAPS).uncheck()
  await saved(page)
  expect(existsSync(diskPath(id, 'data/ranks.json'))).toBe(false)
  expect(existsSync(diskPath(id, 'data/caps.json'))).toBe(false)
  await page.reload()
  await expect(sw(page, RANKS)).not.toBeChecked()
  await expect(sw(page, CAPS)).not.toBeChecked()
})
