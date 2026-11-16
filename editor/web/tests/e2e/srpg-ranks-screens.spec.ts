// Weapon ranks and stat caps (docs/SRPG.md section 7, editor step), the screens in detail: the ladder's rules, what a rank id change, a rank
// removal, a group rename and switching a feature off do to everything that names them, a character's and a placed unit's pickers, the
// checks, and the 360 px phone layout. Starts from a game made through the API (rankskit.ts).
import type { Page } from '@playwright/test'
import { existsSync } from 'node:fs'
import { diskPath, editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { clickCell, tool } from './mapkit'
import { rankedGame, shot, shotEl } from './rankskit'

const sw = (page: Page, label: string) => page.getByTestId('srpg-switches').getByRole('switch', { name: label })
const rankRows = (page: Page) => page.getByTestId('rank-ladder').getByRole('listitem')
const weapon = (id: string, wid: string) => readDisk(id, 'data/weapons.json').weapons.find((w: any) => w.id === wid)
const unit = (id: string, uid: string) => readDisk(id, 'data/units.json').units.find((u: any) => u.id === uid)

test('@smoke the ladder: rising experience is enforced with a message, an id change and a removal follow through, and Undo brings it back', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Ranks Ladder', { srpgContent: true })
  await rankedGame(request, id)
  expect(await game.problems(id)).toEqual([])
  await page.goto(editorUrl(id, 'rules'))
  await expect(rankRows(page)).toHaveCount(6)

  // Each rank needs more than the one before: a lower number is refused with a message, nothing is written.
  const b = page.getByLabel('Experience needed for B')
  await b.fill('50')
  await expect(page.getByRole('alert').filter({ hasText: 'At least 71.' })).toBeVisible()
  await b.fill('125')
  // A number that reaches the ones above moves them up to make room: D to 130 pushes C and B.
  await page.getByLabel('Experience needed for D').fill('130')
  await saved(page)
  expect(readDisk(id, 'data/ranks.json').ranks.map((r: any) => r.xp)).toEqual([0, 130, 131, 132, 180, 250])
  await page.getByRole('button', { name: 'Undo' }).click()
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, 'data/ranks.json').ranks.map((r: any) => r.xp)).toEqual([0, 30, 70, 120, 180, 250])

  // A new id follows into weapons, classes and characters.
  await page.getByLabel('Id of D').fill('second')
  await saved(page)
  expect(weapon(id, 'iron_axe').rank).toBe('second')
  expect(readDisk(id, 'data/ranks.json').ranks[1].id).toBe('second')
  expect(unit(id, 'lord').ranks).toEqual({ sword: 'second' })
  await page.getByLabel('Id of D').fill('d') // the box is still named after the rank's name
  await saved(page)
  expect(weapon(id, 'iron_axe').rank).toBe('d')

  // Removing C: the question names what needs it; confirming takes it out of all of them.
  await page.getByRole('button', { name: 'Remove C' }).click()
  const dialog = page.getByRole('dialog')
  await expect(dialog).toContainText('Weapon “Iron Lance” needs this rank')
  await expect(dialog).toContainText('Character “Eirik” starts in this rank')
  await dialog.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(weapon(id, 'iron_lance')).not.toHaveProperty('rank')
  expect(readDisk(id, 'data/characters.json').characters[0]).not.toHaveProperty('ranks')
  expect(unit(id, 'lord').ranks).toEqual({ sword: 'd' })
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(weapon(id, 'iron_lance').rank).toBe('c')
  expect(readDisk(id, 'data/characters.json').characters[0].ranks).toEqual({ sword: 'c' })
  expect(readDisk(id, 'data/ranks.json').ranks).toHaveLength(6)
  await shotEl(page.locator('#weapon-ranks'), 'rules-ranks-ladder')
  await shotEl(page.locator('#stat-caps'), 'rules-caps')
})

test('a weapon group renamed on its only weapon takes the class that wields it and its ranks along; switching ranks off cleans everything', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Ranks Groups', { srpgContent: true })
  await rankedGame(request, id)
  // The archer wields "bow" (only the bow is in that group); give the class a rank in it.
  await page.goto(editorUrl(id, 'units', 'archer'))
  await page.getByTestId('unit-ranks').getByLabel('Starting rank in bow').selectOption('d')
  await saved(page)
  await page.goto(editorUrl(id, 'weapons', 'iron_bow'))
  await page.getByLabel('Group', { exact: true }).fill('archery')
  await saved(page)
  expect(unit(id, 'archer')).toMatchObject({ wields: ['archery'], ranks: { archery: 'd' } })
  expect(await game.problems(id)).toEqual([])

  // Switching ranks off: asked first; weapons, classes and characters lose their ranks; caps stay.
  await page.goto(editorUrl(id, 'rules'))
  await sw(page, 'This game has weapon ranks').click() // a question follows, so the box does not flip yet
  const dialog = page.getByRole('dialog')
  await expect(dialog).toContainText('Remove weapon ranks?')
  await expect(dialog).toContainText('including the 6 ranks you made')
  await dialog.getByRole('button', { name: 'Remove' }).click()
  await saved(page)
  expect(existsSync(diskPath(id, 'data/ranks.json'))).toBe(false)
  expect(JSON.stringify(readDisk(id, 'data/weapons.json'))).not.toMatch(/"rank/)
  expect(JSON.stringify(readDisk(id, 'data/units.json'))).not.toMatch(/"ranks"/)
  expect(JSON.stringify(readDisk(id, 'data/characters.json'))).not.toMatch(/"ranks"/)
  expect(readDisk(id, 'data/caps.json').caps).toEqual({ str: 30, spd: 30, hp: 60 })
  expect(await game.problems(id)).toEqual([])
  // Caps off: the class cap and the character's adjustment go with the game-wide file.
  await sw(page, 'This game has stat caps').click()
  await page.getByRole('dialog').getByRole('button', { name: 'Remove' }).click()
  await saved(page)
  expect(existsSync(diskPath(id, 'data/caps.json'))).toBe(false)
  expect(JSON.stringify(readDisk(id, 'data/units.json'))).not.toMatch(/"caps"/)
  expect(JSON.stringify(readDisk(id, 'data/characters.json'))).not.toMatch(/capAdjust/)
  expect(await game.problems(id)).toEqual([])
})

test('@smoke a character: personal ranks and cap adjustments with the resulting cap; a placed unit: its own rank', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Ranks Character', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  const ranks = page.getByTestId('character-ranks')
  await expect(ranks.getByLabel('Starting rank in sword')).toHaveValue('c')
  await ranks.getByLabel('Starting rank in lance').selectOption('b')
  // Class cap 24 (Lord) + 3 = 27; the game's 30 - 2 = 28; no cap anywhere stays "no cap".
  const caps = page.getByTestId('character-caps')
  await expect(page.getByTestId('cap-result-str')).toHaveText('27')
  await expect(page.getByTestId('cap-base-str')).toHaveText('24')
  await expect(page.getByTestId('cap-result-spd')).toHaveText('28')
  await expect(page.getByTestId('cap-result-def')).toHaveText('no cap')
  await caps.getByLabel('Strength cap adjustment').fill('5')
  await caps.getByLabel('Defense cap adjustment').fill('4')
  await expect(page.getByTestId('cap-result-str')).toHaveText('29')
  await expect(page.getByTestId('cap-result-def')).toHaveText('no cap')
  await saved(page)
  expect(readDisk(id, 'data/characters.json').characters[0]).toMatchObject({ ranks: { sword: 'c', lance: 'b' }, capAdjust: { str: 5, spd: -2, def: 4 } })
  await shot(page, 'character-ranks')

  // A placed unit's own rank, where its other overrides are.
  await page.goto(editorUrl(id, 'maps', 'ford'))
  await tool(page, 'Select').click()
  await clickCell(page, 1, 3)
  const override = page.getByTestId('unit-rank-override')
  await override.getByLabel('Starting rank in sword').selectOption('a')
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').units[0]).toMatchObject({ character: 'eirik', ranks: { sword: 'a' } })
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'map-unit-rank')
  await override.getByLabel('Starting rank in sword').selectOption('')
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').units[0]).not.toHaveProperty('ranks')
})

test('a mistake shows where it is made and in the Problems list in the loader\'s words (a rank on a weapon without a group)', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Ranks Problem', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'weapons', 'iron_lance'))
  await page.getByLabel('Group', { exact: true }).fill('')
  await saved(page)
  await expect(page.getByTestId('weapon-rank-checks')).toContainText('Iron Lance needs a rank, but it has no group')
  await expect(page.getByTestId('weapon-rank-checks')).toContainText('Iron Lance earns weapon experience, but it has no group')
  await page.getByRole('button', { name: /^\d+ problems?$/ }).click()
  await expect(page.getByRole('region', { name: 'Problems' })).toContainText('needs a "group"')
  await shot(page, 'problems-rank')
  // Giving it its group back (the class entries were never touched) fixes everything.
  await page.getByRole('button', { name: 'Close problems' }).click()
  await page.getByLabel('Group', { exact: true }).fill('lance')
  await saved(page)
  await expect(page.getByTestId('weapon-rank-checks')).toHaveCount(0)
  expect(await game.problems(id)).toEqual([])
})

test('a ladder that no weapon asks for is worth a look on the Rules screen', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Ranks Idle', { srpgContent: true })
  await rankedGame(request, id)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const weapons = doc.weapons.map(({ rank: _r, ...w }: any) => w)
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, weapons } })).ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByTestId('rank-checks')).toContainText('No weapon needs a rank yet')
})

test('@smoke on a 360 px phone the Rules cards, a class, a character and a weapon fit without sideways scrolling', async ({ page, game, request }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Ranks Phone', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByTestId('rank-ladder')).toBeVisible()
  await expectNoHScroll(page)
  await shotEl(page.locator('#weapon-ranks'), 'phone-rules-ranks')
  await page.getByTestId('caps-card').scrollIntoViewIfNeeded()
  await shotEl(page.locator('#stat-caps'), 'phone-rules-caps')
  await expectNoHScroll(page)
  await page.goto(editorUrl(id, 'units', 'lord'))
  await expect(page.getByTestId('unit-ranks')).toBeVisible()
  await expectNoHScroll(page)
  await shotEl(page.locator('#unit-ranks'), 'phone-unit-ranks')
  await shotEl(page.locator('#unit-caps'), 'phone-unit-caps')
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  await expect(page.getByTestId('character-caps')).toBeVisible()
  await expectNoHScroll(page)
  await shotEl(page.locator('#character-ranks'), 'phone-character-ranks')
  await shotEl(page.locator('#character-caps'), 'phone-character-caps')
  await page.goto(editorUrl(id, 'weapons', 'iron_lance'))
  await expect(page.getByTestId('weapon-rank')).toBeVisible()
  await expectNoHScroll(page)
  await shotEl(page.locator('#weapon-rank'), 'phone-weapon-rank')
})
