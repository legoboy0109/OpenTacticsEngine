// The Staff form, stage two (docs/SRPG.md section 8): moving a unit (warp / pull / swap), repairing an item, a light staff, an item's
// "can be repaired" switch. The proof that matters: the exact JSON the form wrote, that the loader accepts it (/problems is empty), that
// the real engine loads it (Play), that the choices the loader would refuse cannot be made, and the 360 px phone layout.
import type { APIRequestContext, Page } from '@playwright/test'
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
  await page.getByLabel('Kind', { exact: true }).selectOption('staff')
  await expect(page.getByTestId('staff-use')).toBeVisible()
}

/** A game with weapons carried, a ladder, and one item that wears out (so a repair staff has something to mend). */
async function staffGame(request: APIRequestContext, id: string): Promise<void> {
  await rankedGame(request, id)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const list = [...(doc.items ?? []), { id: 'old_herb', name: 'Old Herb', kind: 'usable', heal: 5, uses: 3 }]
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, items: list } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

const check = (page: Page, name: string) => page.getByRole('checkbox', { name, exact: false })
const optionDisabled = async (page: Page, label: string, value: string) => page.getByLabel(label, { exact: true }).locator(`option[value="${value}"]`).isDisabled()

test('@smoke build a warp staff, a heal + pull staff, a repair staff and a light staff; the loader and the real engine accept the game', async ({ page, game, request }) => {
  requirePlayer()
  const id = await game.create('strategy-rpg', 'Staves Two', { srpgContent: true })
  await staffGame(request, id)
  await page.goto(editorUrl(id, 'items'))

  // A warp staff: moving a unit by warp, healing off, a reach of 2 to 4 squares from the user.
  await createStaff(page, 'Warp Staff')
  await page.getByLabel('Moving a unit').selectOption('warp')
  await check(page, 'It heals').uncheck()
  await page.getByLabel('How far the unit can be sent').selectOption('between')
  await page.getByLabel('Farthest square it can land on').fill('4')
  await page.getByLabel('Nearest square it can land on').fill('2')
  await saved(page)
  expect(staff(id, 'warp_staff')).toMatchObject({ kind: 'staff', group: 'staff', range: 2, effects: [{ warp: { range: [2, 4] } }] })
  await shot(page, 'desktop-warp-staff')
  // The only effect cannot be switched off.
  await expect(page.getByLabel('Moving a unit').locator('option[value="none"]')).toBeDisabled()

  // A heal + pull staff on a wide area: choosing "pull" brings the area back to one unit in the same step.
  await createStaff(page, 'Recall Staff')
  await page.getByLabel('What it affects').selectOption('radius')
  await saved(page)
  expect(staff(id, 'recall_staff').area).toEqual({ radius: 1 })
  await page.getByLabel('Moving a unit').selectOption('pull')
  await saved(page)
  expect(staff(id, 'recall_staff')).toMatchObject({ effects: [{ heal: 10 }, { pull: true }] })
  expect(staff(id, 'recall_staff')).not.toHaveProperty('area')
  expect(await optionDisabled(page, 'What it affects', 'radius')).toBe(true)
  expect(await optionDisabled(page, 'Who it can be used on', 'self')).toBe(true)
  await shot(page, 'desktop-heal-pull-staff')

  // A repair staff (all uses is the default; then a number of uses), with the heal off.
  await createStaff(page, 'Fix Staff')
  await check(page, 'It repairs an item').check()
  await page.getByLabel('Repairing', { exact: true }).selectOption('uses')
  await page.getByLabel('Uses restored').fill('2')
  await saved(page)
  expect(staff(id, 'fix_staff')).toMatchObject({ effects: [{ heal: 10 }, { repair: 2 }] })
  await check(page, 'It heals').uncheck()
  await saved(page)
  expect(staff(id, 'fix_staff').effects).toEqual([{ repair: 2 }])
  expect(await optionDisabled(page, 'Moving a unit', 'warp')).toBe(true)
  await shot(page, 'desktop-repair-staff')

  // A light staff: switching it on takes the chance of missing and the conditions away with the other effects.
  await createStaff(page, 'Lantern Staff')
  await page.getByRole('checkbox', { name: 'It can miss' }).check()
  await page.getByLabel('Who it can be used on').selectOption('team')
  await saved(page)
  expect(staff(id, 'lantern_staff')).toMatchObject({ hit: 80, target: 'team' })
  await check(page, 'It lights up an area').check()
  await page.getByLabel('Radius of the light (squares)').fill('3')
  await page.getByLabel('Turns it lasts').fill('2')
  await saved(page)
  const lantern = staff(id, 'lantern_staff')
  expect(lantern).toMatchObject({ range: 2, effects: [{ light: { radius: 3, turns: 2 } }] })
  for (const k of ['target', 'area', 'hit', 'if']) expect(lantern).not.toHaveProperty(k)
  await expect(page.getByLabel('Who it can be used on')).toHaveCount(0)
  await expect(page.getByTestId('staff-conditions')).toHaveCount(0)
  // No map of this game has fog: worth a look (it loads fine).
  await expect(page.getByTestId('staff-checks')).toContainText('no map of this game has fog')
  await shot(page, 'desktop-light-staff')

  // An item that wears out gets the "can be repaired" switch (a repair staff exists now).
  await page.goto(editorUrl(id, 'items', 'old_herb'))
  await page.getByRole('checkbox', { name: 'A repair staff can mend it' }).uncheck()
  await saved(page)
  expect(staff(id, 'old_herb')).toMatchObject({ uses: 3, repairable: false })
  await shotEl(page.getByRole('checkbox', { name: 'A repair staff can mend it' }), 'desktop-repairable-switch')
  await page.getByRole('checkbox', { name: 'A repair staff can mend it' }).check()
  await saved(page)
  expect(staff(id, 'old_herb')).not.toHaveProperty('repairable')

  // Reload: everything is still there, the loader is happy, and the real engine loads it.
  await page.goto(editorUrl(id, 'items', 'recall_staff'))
  await page.reload()
  await expect(page.getByLabel('Moving a unit')).toHaveValue('pull')
  expect(await game.problems(id)).toEqual([])
  await playButton(page).click()
  await expect(playPanel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})

test('choices the loader would refuse cannot be made, and what the form does not know is kept', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Staves Two Rules', { srpgContent: true })
  await staffGame(request, id)
  await page.goto(editorUrl(id, 'items'))
  await createStaff(page, 'Rule Staff')

  // The staff heals: an enemy is not offered. Healing + repair is allowed; repair with a move is not (greyed, with the reason).
  await expect(page.getByLabel('Who it can be used on').locator('option[value="foe"]')).toHaveCount(0)
  await check(page, 'It repairs an item').check()
  expect(await optionDisabled(page, 'Moving a unit', 'warp')).toBe(true)
  await expect(page.getByTestId('staff-effect')).toContainText('cannot be put together with repairing')
  await check(page, 'It repairs an item').uncheck()
  await page.getByLabel('Moving a unit').selectOption('swap')
  await expect(check(page, 'It repairs an item')).toBeDisabled()
  // Only the user cannot be swapped with; warping the user is fine, and an enemy can be moved once nothing heals.
  expect(await optionDisabled(page, 'Who it can be used on', 'self')).toBe(true)
  await page.getByLabel('Moving a unit').selectOption('warp')
  await page.getByLabel('Who it can be used on').selectOption('self')
  await expect(page.getByTestId('staff-self-note')).toBeVisible()
  expect(await optionDisabled(page, 'Moving a unit', 'pull')).toBe(true)
  await page.getByLabel('Who it can be used on').selectOption('ally')
  await check(page, 'It heals').uncheck()
  await page.getByLabel('Who it can be used on').selectOption('foe')
  await expect(check(page, 'It heals')).toBeDisabled()
  await saved(page)
  expect(staff(id, 'rule_staff')).toMatchObject({ target: 'foe', effects: [{ warp: { range: 3 } }] })
  expect(await game.problems(id)).toEqual([])

  // The conditions box of a staff says staff.
  await page.getByLabel('Add a condition…').selectOption('hp')
  await expect(page.getByTestId('staff-conditions')).toContainText('The unit using the staff')
  await expect(page.getByTestId('staff-conditions')).not.toContainText('ability')

  // A file with an effect the form does not know (and the loader does not either): it is kept, said so, and light cannot be switched on.
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const odd = doc.items.map((i: any) => (i.id === 'rule_staff' ? { ...i, effects: [{ heal: 4 }, { unlock: 'door' }, { pull: true }] } : i))
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, items: odd } })
  expect(res.ok(), await res.text()).toBeTruthy()
  await page.reload()
  await expect(page.getByTestId('staff-more-effects')).toContainText('One more effect')
  await expect(check(page, 'It lights up an area')).toBeDisabled()
  await expect(page.getByTestId('item-checks')).toContainText('“unlock”, which a staff cannot do')
  await page.getByLabel('Experience for the user').fill('12')
  await page.getByLabel('Healing', { exact: true }).selectOption('full')
  await saved(page)
  expect(staff(id, 'rule_staff').effects).toEqual([{ heal: 'full' }, { unlock: 'door' }, { pull: true }])
  expect(staff(id, 'rule_staff').xp).toBe(12)

  // A hand-edited file the form cannot make: two moves. The Problems list says so.
  const doc2 = await (await request.get(`/api/projects/${id}`)).json()
  const two = doc2.items.map((i: any) => (i.id === 'rule_staff' ? { ...i, effects: [{ warp: { range: 3 } }, { swap: true }] } : i))
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc2.revision, items: two } })).ok()).toBeTruthy()
  await page.reload()
  await expect(page.getByTestId('item-checks')).toContainText('only one of warp, pull and swap')
  await shotEl(page.getByTestId('item-checks'), 'checks-two-moves')
})

test('a staff in a game with no repairable item says so; the switch hides until a repair staff exists', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Staves Two Notes', { srpgContent: true })
  await staffGame(request, id)
  await page.goto(editorUrl(id, 'items', 'old_herb'))
  await expect(page.getByRole('checkbox', { name: 'A repair staff can mend it' })).toHaveCount(0)
  await page.goto(editorUrl(id, 'items'))
  await createStaff(page, 'Note Staff')
  await check(page, 'It repairs an item').check()
  await saved(page)
  await page.goto(editorUrl(id, 'items', 'old_herb'))
  await page.getByRole('checkbox', { name: 'A repair staff can mend it' }).uncheck()
  await saved(page)
  await page.goto(editorUrl(id, 'items', 'note_staff'))
  await expect(page.getByTestId('staff-checks')).toContainText('the game has no item that can be repaired')
  expect(await game.problems(id)).toEqual([])
  await shotEl(page.getByTestId('staff-checks'), 'checks-nothing-to-repair')
})

test('@smoke on a 360 px phone every part of the form fits without sideways scrolling', async ({ page, game, request }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Staves Two Phone', { srpgContent: true })
  await staffGame(request, id)
  await page.goto(editorUrl(id, 'items'))
  await createStaff(page, 'Phone Staff')
  await expectNoHScroll(page)
  await page.getByLabel('Moving a unit').selectOption('warp')
  await page.getByLabel('How far the unit can be sent').selectOption('formula')
  await expectNoHScroll(page)
  await shot(page, 'phone-warp-formula')
  await page.getByLabel('Moving a unit').selectOption('none')
  await check(page, 'It repairs an item').check()
  await page.getByLabel('Repairing', { exact: true }).selectOption('uses')
  await expectNoHScroll(page)
  await shot(page, 'phone-heal-repair')
  await check(page, 'It lights up an area').check()
  await expectNoHScroll(page)
  await shot(page, 'phone-light')
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})
