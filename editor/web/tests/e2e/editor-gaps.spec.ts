// The editor-gap pass (docs/EDITOR_UI.md "Editor gaps"): class locks on a weapon or staff (`wieldedByTypes`), a character's own weapon groups
// (`wields`), `rankXp: 0`, starting ranks and "goes for" on a story "Add a unit" command (the same controls as the map's unit panel), and the
// preparation marker on the Campaign screen. The proof that matters: the exact JSON the forms wrote (compared with docs/SRPG.md), that the
// loader accepts it (/problems is empty), that the real engine loads it (Play), at desktop width and on a 360 px phone.
import { cpSync, readdirSync, rmSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import { diskPath, editorUrl, expect, expectNoHScroll, gotoSection, readDisk, requirePlayer, saved, test } from './fixtures'
import { openMap, tab } from './mapkit'
import { rankedGame, shot, shotEl } from './rankskit'
import { addCommand, eventsTab, newEvent, when } from './storykit'

test.use({ actionTimeout: 10_000 }) // a wrong label fails in seconds, not minutes
test.describe.configure({ timeout: 150_000 }) // one long path through five screens, and the machine may be busy

const weapon = (id: string, wid: string) => readDisk(id, 'data/weapons.json').weapons.find((w: any) => w.id === wid)
const character = (id: string, cid: string) => readDisk(id, 'data/characters.json').characters.find((c: any) => c.id === cid)
const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const playPanel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })

/** Goes to a screen by its link on a phone (the menu is opened first) and straight to the address on a desktop. */
async function open(page: Page, phone: boolean, id: string, section: string, item?: string, link?: string): Promise<void> {
  await page.goto(editorUrl(id, section, item))
  if (phone && link) { await gotoSection(page, link); if (item) await page.goto(editorUrl(id, section, item)) }
}

async function run(page: Page, game: any, request: any, phone: boolean): Promise<void> {
  const tag = phone ? 'phone-' : 'desktop-'
  const id = await game.create('strategy-rpg', phone ? 'Gaps Phone' : 'Gaps Desktop', { srpgContent: true })
  await rankedGame(request, id) // carried weapons, groups sword / lance / axe / bow, a ladder, the lord wields sword + lance

  // A staff (an item) so the same card is seen on the Items screen: written through the API like a second author would.
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const items = [...doc.items, { id: 'mend_staff', name: 'Mend Staff', kind: 'staff', group: 'staff', range: 2, effects: [{ heal: 10 }], uses: 5 }]
  const units = doc.units.map((u: any) => (u.id === 'archer' ? { ...u, wields: [...u.wields, 'staff'] } : u))
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, items, units } })
  expect(put.ok(), await put.text()).toBeTruthy()

  // ===== 1. Weapons: classes and characters on one card =====
  await open(page, phone, id, 'weapons', 'iron_sword', 'Weapons')
  const card = page.getByTestId('weapon-lock')
  await expect(card).toBeVisible()
  await expect(card.getByRole('group', { name: 'Classes that can wield it' })).toBeVisible()
  await expect(page.getByTestId('weapon-lock-summary')).toContainText('No lock')
  await card.getByRole('checkbox', { name: 'Archer' }).check() // an archer cannot use a sword: worth a look
  await expect(page.getByTestId('weapon-lock-checks')).toContainText('Nobody on the list can ever wield Iron Sword: none of those classes can use its group')
  await card.getByRole('checkbox', { name: 'Lord' }).check()
  await expect(page.getByTestId('weapon-lock-checks')).toHaveCount(0)
  await card.getByRole('checkbox', { name: 'Archer' }).uncheck()
  await card.getByRole('checkbox', { name: 'Vex' }).check() // and one character: a unit passes by either list
  await saved(page)
  expect(weapon(id, 'iron_sword').wieldedByTypes).toEqual(['lord'])
  expect(weapon(id, 'iron_sword').wieldedBy).toEqual(['vex'])
  await expect(page.getByTestId('weapon-lock-summary')).toContainText('Locked to 1 character and to 1 class')
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shotEl(card, `${tag}weapon-lock-card`)

  // ===== 2. A staff: the same card on the Items screen =====
  await open(page, phone, id, 'items', 'mend_staff', 'Items')
  const staffCard = page.getByTestId('staff-lock')
  await staffCard.getByRole('checkbox', { name: 'Archer' }).check()
  await saved(page)
  expect(readDisk(id, 'data/items.json').items.find((i: any) => i.id === 'mend_staff').wieldedByTypes).toEqual(['archer'])
  expect(await game.problems(id)).toEqual([])

  // ===== 3. rankXp 0 on a weapon =====
  await open(page, phone, id, 'weapons', 'iron_lance', 'Weapons')
  await expect(page.getByLabel('Experience per use')).toHaveValue('2')
  await page.getByLabel('Experience per use').fill('0')
  await saved(page)
  expect(weapon(id, 'iron_lance').rankXp).toBe(0)
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('weapon-rank-checks')).toHaveCount(0)
  if (phone) await expectNoHScroll(page)
  await shotEl(page.getByTestId('weapon-rank'), `${tag}weapon-rank-xp-0`)

  // ===== 4. A character's own weapon groups, and a starting rank in one =====
  await open(page, phone, id, 'characters', 'eirik', 'Characters')
  const wields = page.getByTestId('character-wields')
  await expect(wields).toBeVisible()
  await expect(wields.getByRole('checkbox', { name: /^sword/ })).toBeDisabled() // the class already wields it
  await wields.getByRole('checkbox', { name: 'axe' }).check()
  await saved(page)
  expect(character(id, 'eirik').wields).toEqual(['axe'])
  await page.getByTestId('character-ranks-table').getByLabel('Starting rank in axe').selectOption('d')
  await saved(page)
  expect(character(id, 'eirik').ranks).toEqual({ sword: 'c', axe: 'd' })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shotEl(wields, `${tag}character-wields`)
  await shotEl(page.getByTestId('character-ranks'), `${tag}character-ranks-axe`)

  // ===== 5. Add a unit command: starting ranks and goes-for =====
  const ford = readDisk(id, 'maps/ford.json')
  const red = ford.sides.find((s: any) => s.controller === 'ai')
  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'reinforce')
  await when(page, 'A particular turn')
  await addCommand(page, 'Then', 'Add a unit')
  const f = page.getByTestId('add-unit-form')
  await expect(f).toBeVisible()
  await f.getByLabel('Unit type', { exact: true }).selectOption({ label: 'Lord' })
  await f.getByLabel('Side', { exact: true }).selectOption({ label: red.name ?? red.id })
  await expect(page.getByTestId('goes-for')).toBeVisible()
  await f.getByTestId('add-unit-ranks').getByLabel('Starting rank in sword').selectOption('a')
  await f.getByTestId('goes-for').getByRole('checkbox', { name: /^Chests/ }).check()
  await f.getByTestId('goes-for').getByRole('checkbox', { name: /^Steal/ }).check()
  await saved(page)
  const cmd = readDisk(id, 'maps/ford.json').events.find((e: any) => e.id === 'reinforce').do[0].addUnit
  expect(cmd).toMatchObject({ type: 'lord', side: red.id, ranks: { sword: 'a' }, goesFor: ['chests', 'steal'] })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}add-unit-command`)
  // Clearing them removes the keys; a side a person plays has no goes-for.
  await f.getByTestId('add-unit-ranks').getByLabel('Starting rank in sword').selectOption('')
  await f.getByTestId('goes-for').getByRole('checkbox', { name: /^Chests/ }).uncheck()
  await f.getByTestId('goes-for').getByRole('checkbox', { name: /^Steal/ }).uncheck()
  await saved(page)
  const cleared = readDisk(id, 'maps/ford.json').events.find((e: any) => e.id === 'reinforce').do[0].addUnit
  expect(cleared).not.toHaveProperty('ranks')
  expect(cleared).not.toHaveProperty('goesFor')
  await f.getByTestId('add-unit-ranks').getByLabel('Starting rank in sword').selectOption('b')
  await f.getByTestId('goes-for').getByRole('checkbox', { name: /^Doors/ }).check()
  await saved(page)

  // ===== The real engine loads all of it =====
  expect(await game.problems(id)).toEqual([])
  if (!phone) {
    requirePlayer()
    await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
    await playButton(page).click()
    await expect(playPanel(page)).toBeVisible()
    await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
    await expect(page.getByTestId('play-error')).toHaveCount(0)
  }
}

test('@smoke class locks, a character\'s own groups, rankXp 0 and the Add a unit command\'s ranks and goes-for (desktop)', async ({ page, game, request }) => {
  await run(page, game, request, false)
})

test('the same on a 360 px phone', async ({ page, game, request }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  await run(page, game, request, true)
})

// --- The campaign marks a chapter whose map has a preparation screen ---------------------------------------------------------------

const repoFixture = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-prep')

async function prepGame(game: { create: (t: string, n?: string) => Promise<string> }): Promise<string> {
  const id = await game.create('blank', 'Marked')
  for (const dir of ['maps', 'data']) for (const f of readdirSync(diskPath(id, dir))) rmSync(diskPath(id, `${dir}/${f}`), { recursive: true, force: true })
  cpSync(repoFixture, diskPath(id, '.'), { recursive: true })
  rmSync(diskPath(id, 'README.md'), { force: true })
  return id
}

test('the Campaign list and timeline mark the chapters whose map has a preparation screen', async ({ page, game }) => {
  const id = await prepGame(game)
  await page.goto(`/p/${id}/campaign`)
  await page.getByRole('tab', { name: 'List' }).click()
  const orchard = page.locator('.md-item').filter({ hasText: 'The Orchard' })
  await expect(orchard).toContainText('preparation screen')
  await expect(page.locator('.md-item').filter({ hasText: 'The Meadow' })).not.toContainText('preparation screen')
  await page.getByRole('tab', { name: 'Timeline' }).click()
  await expect(page.getByTestId('timeline')).toBeVisible()
  await expect(page.getByTestId('prep-ch2:level')).toBeVisible()
  await expect(page.getByTestId('prep-ch1:level')).toHaveCount(0)
  await expect(page.getByTestId('box-ch2:level').getByRole('button').first()).toHaveAttribute('aria-label', /preparation screen before the battle/)
  await shot(page, 'timeline-prep-marker')
  // Switching the map's preparation off in the map editor takes the marker away.
  await openMap(page, id, 'orchard')
  await tab(page, 'Preparation').click()
  await page.getByRole('switch', { name: /^This map has a preparation screen/ }).uncheck()
  await saved(page)
  await page.goto(`/p/${id}/campaign`)
  await page.getByRole('tab', { name: 'List' }).click()
  await expect(page.locator('.md-item').filter({ hasText: 'The Orchard' })).not.toContainText('preparation screen')
})
