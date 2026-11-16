// Staves in the editor (docs/SRPG.md section 8): the Staff kind on the Items screen with a formula reach and an area, the class that
// wields its group, the kind change that cleans up (one Undo), the checks, and the 360 px phone layout. The proof that matters: the exact
// JSON the editor wrote, that the loader accepts it (/problems is empty) and that the real engine loads it (Play).
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, requirePlayer, saved, test } from './fixtures'
import { rankedGame, shot, shotEl } from './rankskit'

const items = (id: string) => readDisk(id, 'data/items.json').items as any[]
const staff = (id: string, sid: string) => items(id).find((i) => i.id === sid)
const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const playPanel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })

async function createStaff(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New item' }).click()
  const dlg = page.getByRole('dialog', { name: 'New item' })
  await dlg.getByLabel('Name of the item').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
  await page.getByLabel('Kind').selectOption('staff')
  await expect(page.getByTestId('staff-use')).toBeVisible()
}

test('@smoke make a staff with a formula reach and an area, give a class its group, reload, press Play: the real engine loads it', async ({ page, game, request }) => {
  requirePlayer()
  const id = await game.create('strategy-rpg', 'Staves Play', { srpgContent: true })
  await rankedGame(request, id)

  await page.goto(editorUrl(id, 'items'))
  await createStaff(page, 'Mend Staff')
  // A new staff is valid straight away: a group, a short reach, a small heal.
  await saved(page)
  expect(staff(id, 'mend_staff')).toMatchObject({ kind: 'staff', group: 'staff', range: 2, effects: [{ heal: 10 }] })
  expect(staff(id, 'mend_staff')).not.toHaveProperty('heal')

  await page.getByLabel('How far it reaches').selectOption('formula')
  await page.getByLabel('Formula for the farthest square').fill('1 + a.str / 3')
  await page.getByLabel('What it affects').selectOption('radius')
  await page.getByLabel('Squares around the unit picked').fill('2')
  await page.getByLabel('Who it can be used on').selectOption('team')
  await page.getByLabel('Healing', { exact: true }).selectOption('percent')
  await page.getByLabel('Percent of maximum HP').fill('30')
  await page.getByLabel('Number of uses').fill('5')
  await saved(page)
  expect(staff(id, 'mend_staff')).toMatchObject({ range: '1 + a.str / 3', area: { radius: 2 }, target: 'team', effects: [{ heal: { percent: 30 } }], uses: 5 })
  await shot(page, 'desktop-staff-formula-radius-percent')

  // A class gets the group: the Units screen offers it although no weapon has it.
  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByRole('checkbox', { name: 'staff', exact: true }).check()
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord').wields).toContain('staff')

  // Reload: all still there, and the loader and the real engine are happy.
  await page.goto(editorUrl(id, 'items', 'mend_staff'))
  await page.reload()
  await expect(page.getByLabel('Formula for the farthest square')).toHaveValue('1 + a.str / 3')
  await expect(page.getByLabel('Squares around the unit picked')).toHaveValue('2')
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('staff-checks')).toHaveCount(0)
  await playButton(page).click()
  await expect(playPanel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})

test('every reach, healing and miss mode writes what it says; checks name what the loader would refuse', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Staves Modes', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'items'))
  await createStaff(page, 'Odd Staff')

  await page.getByLabel('How far it reaches').selectOption('between')
  await page.getByLabel('To (squares)').fill('4')
  await page.getByLabel('From (squares)').fill('2')
  await saved(page)
  expect(staff(id, 'odd_staff').range).toEqual([2, 4])
  await shot(page, 'desktop-staff-between')

  await page.getByLabel('Healing', { exact: true }).selectOption('full')
  await saved(page)
  expect(staff(id, 'odd_staff').effects).toEqual([{ heal: 'full' }])
  await page.getByLabel('Healing', { exact: true }).selectOption('formula')
  await expect(page.getByLabel('Formula for the HP restored')).toHaveValue(/w\.power/)
  await saved(page)
  expect(typeof staff(id, 'odd_staff').effects[0].heal).toBe('string')

  await page.getByRole('checkbox', { name: 'It can miss' }).check()
  await page.getByLabel('Chance of working, in percent').fill('70 + a.str - b.def')
  await page.getByLabel('Experience for the user').fill('15')
  await saved(page)
  expect(staff(id, 'odd_staff')).toMatchObject({ hit: '70 + a.str - b.def', xp: 15 })
  await shot(page, 'desktop-staff-formula-heal-miss')
  await page.getByRole('checkbox', { name: 'It can miss' }).uncheck()
  await saved(page)
  expect(staff(id, 'odd_staff')).not.toHaveProperty('hit')

  // "Only the user" takes the reach and the area away.
  await page.getByLabel('Who it can be used on').selectOption('self')
  await expect(page.getByTestId('staff-self-note')).toBeVisible()
  await saved(page)
  expect(staff(id, 'odd_staff')).toMatchObject({ target: 'self' })
  expect(staff(id, 'odd_staff')).not.toHaveProperty('range')
  await page.getByLabel('Who it can be used on').selectOption('ally')

  // Nobody can use it yet (the class has no such group): worth a look, not a refusal.
  await expect(page.getByTestId('staff-checks')).toContainText('no class can wield its group')
  expect(await game.problems(id)).toEqual([])
  await shotEl(page.getByTestId('staff-checks'), 'checks-nobody-can-use')
  // Reach above the limit: the loader would refuse it, the screen says so.
  await page.getByLabel('How far it reaches').selectOption('upto')
  await page.getByLabel('Squares', { exact: true }).fill('20')
  await saved(page)
  await expect(page.getByTestId('item-checks')).toHaveCount(0)
})

test('a rank and a lock on a staff use the weapons’ cards; Staff to Usable removes the staff keys in one Undo step', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Staves Kind', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'items'))
  await createStaff(page, 'Rank Staff')
  await expect(page.getByTestId('staff-rank')).toBeVisible()
  await page.getByTestId('staff-rank').getByLabel('Rank needed to use it').selectOption('c')
  await page.getByTestId('staff-lock').getByRole('checkbox', { name: 'Eirik' }).check()
  await saved(page)
  expect(staff(id, 'rank_staff')).toMatchObject({ rank: 'c', wieldedBy: ['eirik'] })
  expect(await game.problems(id)).toEqual([])
  await shotEl(page.locator('#staff-rank'), 'staff-rank-card')
  await shotEl(page.locator('#staff-lock'), 'staff-lock-card')

  // The character's screen lists the staff.
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  await expect(page.getByTestId('character-locked-weapons')).toContainText('Rank Staff')
  await page.goto(editorUrl(id, 'items', 'rank_staff'))

  await page.getByLabel('Kind').selectOption('usable')
  await saved(page)
  const u = staff(id, 'rank_staff')
  expect(u).toMatchObject({ kind: 'usable', heal: 10 })
  for (const k of ['group', 'rank', 'wieldedBy', 'range', 'effects']) expect(u).not.toHaveProperty(k)
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(staff(id, 'rank_staff')).toMatchObject({ kind: 'staff', group: 'staff', rank: 'c', wieldedBy: ['eirik'], range: 2 })
})

test('@smoke on a 360 px phone the staff form fits without sideways scrolling in every mode', async ({ page, game, request }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Staves Phone', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'items'))
  // The items list is a master list on a phone: the new item opens its form.
  await createStaff(page, 'Phone Staff')
  await expectNoHScroll(page)
  await shot(page, 'phone-staff-default')
  await page.getByLabel('How far it reaches').selectOption('formula')
  await page.getByLabel('What it affects').selectOption('radius')
  await page.getByLabel('Healing', { exact: true }).selectOption('formula')
  await page.getByRole('checkbox', { name: 'It can miss' }).check()
  await page.getByLabel('Chance of working, in percent').fill('70 + a.str - b.def')
  await expectNoHScroll(page)
  await shot(page, 'phone-staff-formulas')
  await page.getByLabel('How far it reaches').selectOption('between')
  await page.getByLabel('Healing', { exact: true }).selectOption('percent')
  await expectNoHScroll(page)
  await shot(page, 'phone-staff-between-percent')
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})
