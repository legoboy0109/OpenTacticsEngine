// Map weapons through the editor's own forms (docs/SRPG.md "Map weapons", docs/EDITOR_UI.md "Map weapons"): the "A weapon to use here" card on a structure kind,
// a placed structure's own uses, the computer unit's "Goes for: Weapons", and the checks. The proof that matters: the exact JSON the forms wrote (what
// docs/SRPG.md specifies), that the real loader accepts it (/problems is empty), and the 360 px phone layout. The game is the engine's fixture
// tests/fixtures/srpg-mapweapon.
import { cpSync, readdirSync, rmSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import { diskPath, editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test, writeDisk } from './fixtures'
import { clickCell, openMap, tab, tool } from './mapkit'
import { shot } from './storykit'

test.use({ actionTimeout: 10_000 }) // a wrong label fails in seconds, not minutes

const repoFixture = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-mapweapon')
const structures = (id: string) => readDisk(id, 'data/structures.json').structures as any[]
const kind = (id: string, k: string) => structures(id).find((s) => s.id === k)
const card = (page: Page) => page.getByTestId('weapon-card')

/** A new game whose files are replaced by the engine's map weapon fixture (two maps: a field with ballistas, a siege with a catapult). */
async function weaponGame(game: { create: (t: string, n?: string) => Promise<string> }): Promise<string> {
  const id = await game.create('blank', 'Siege')
  for (const dir of ['maps', 'data']) for (const f of readdirSync(diskPath(id, dir))) rmSync(diskPath(id, `${dir}/${f}`), { recursive: true, force: true })
  cpSync(repoFixture, diskPath(id, '.'), { recursive: true })
  rmSync(diskPath(id, 'maps/hexfield.json'), { force: true })  // (the hexagon map of the engine tests is not part of this game)
  rmSync(diskPath(id, 'README.md'), { force: true })
  return id
}

async function run(page: Page, game: { problems: (id: string) => Promise<Array<{ file: string; message: string }>> }, id: string, phone: boolean): Promise<void> {
  // ===== The card on a kind with no weapon: switch it on, fill it in =====
  await page.goto(editorUrl(id, 'structures', 'tower'))
  await expect(card(page)).toBeVisible()
  const on = card(page).getByRole('checkbox', { name: /A unit standing here can attack with a weapon/ })
  await expect(on).not.toBeChecked()
  await on.check()
  await saved(page)
  expect(kind(id, 'tower').weapon).toBe('sword')  // the first weapon item of the game
  await card(page).getByLabel('Weapon', { exact: true }).selectOption({ label: 'Bolt' })
  await card(page).getByLabel('Uses', { exact: true }).fill('5')
  await card(page).getByRole('checkbox', { name: 'Knight', exact: true }).check()
  await card(page).getByRole('checkbox', { name: 'archer', exact: true }).check()
  await card(page).getByRole('checkbox', { name: /Ignore the weapon’s rank and lock/ }).check()
  await card(page).getByLabel('When the weapon is spent', { exact: true }).selectOption({ label: 'It becomes Broken Ballista' })
  await saved(page)
  expect(kind(id, 'tower')).toMatchObject({ weapon: 'ballista_bolt', uses: 5, by: { types: ['knight'], tags: ['archer'] }, ignoreRank: true, becomes: 'ballista_wreck' })
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('weapon-who')).toContainText('2 unit types may use it')
  if (phone) await expectNoHScroll(page)
  await shot(page, `${phone ? 'phone' : 'desktop'}-weapon-card`)

  // An empty "Uses" box means no limit (the key goes); un-ticking the types and tags removes "by".
  await card(page).getByLabel('Uses', { exact: true }).fill('')
  await card(page).getByRole('checkbox', { name: 'Knight', exact: true }).uncheck()
  await card(page).getByRole('checkbox', { name: 'archer', exact: true }).uncheck()
  await saved(page)
  expect(kind(id, 'tower')).not.toHaveProperty('uses')
  expect(kind(id, 'tower')).not.toHaveProperty('by')

  // Switching the card off removes the weapon and everything that belongs with it.
  await on.uncheck()
  await saved(page)
  const tower = kind(id, 'tower')
  for (const k of ['weapon', 'uses', 'by', 'ignoreRank', 'becomes']) expect(tower).not.toHaveProperty(k)
  expect(await game.problems(id)).toEqual([])

  // ===== A kind that is wrong says so (what the loader would refuse), in plain words =====
  const list = structures(id)
  list.find((s) => s.id === 'ballista').uses = 0
  writeDisk(id, 'data/structures.json', JSON.stringify({ structures: list }, null, 1))
  await page.goto(editorUrl(id, 'structures', 'ballista'))
  await expect(card(page).getByTestId('weapon-checks')).toContainText('whole number from 1 to 9999')
  list.find((s) => s.id === 'ballista').uses = 3
  writeDisk(id, 'data/structures.json', JSON.stringify({ structures: list }, null, 1))

  // ===== The map: a placed ballista has uses of its own =====
  await openMap(page, id, 'field')
  await tab(page, 'Tool').click()
  await tool(page, 'Structures').click()
  await clickCell(page, 4, 1)  // the ballista b1 (the kind's 3 uses)
  const own = page.getByTestId('map-weapon-props')
  await expect(own).toBeVisible()
  await expect(own).toContainText('as the kind says (3 uses)')
  await own.getByLabel('Uses left of its weapon').fill('2')
  await saved(page)
  expect(readDisk(id, 'maps/field.json').structures.find((p: any) => p.id === 'b1').uses).toBe(2)
  await own.getByLabel('Uses left of its weapon').fill('')
  await saved(page)
  expect(readDisk(id, 'maps/field.json').structures.find((p: any) => p.id === 'b1')).not.toHaveProperty('uses')
  await clickCell(page, 6, 0)  // the tower has no weapon any more: nothing to set
  await expect(own).toHaveCount(0)
  if (phone) await expectNoHScroll(page)
  await clickCell(page, 4, 1)
  await shot(page, `${phone ? 'phone' : 'desktop'}-map-weapon-uses`)

  // ===== Goes for: Weapons on a computer unit =====
  await openMap(page, id, 'siege')
  await tab(page, 'Tool').click()
  await tool(page, 'Select').click()
  await clickCell(page, 8, 1)  // the red grunt
  const goes = page.getByTestId('goes-for')
  await expect(goes).toBeVisible()
  await goes.getByRole('checkbox', { name: /^Weapons/ }).check()
  await saved(page)
  expect(readDisk(id, 'maps/siege.json').units.find((u: any) => u.id === 'g1').goesFor).toEqual(['weapons'])
  expect(await game.problems(id)).toEqual([])
  await clickCell(page, 0, 1)  // the knight is on a side a person plays: no such control
  await expect(goes).toHaveCount(0)
  if (phone) await expectNoHScroll(page)
  await clickCell(page, 8, 1)
  await shot(page, `${phone ? 'phone' : 'desktop'}-goes-for-weapons`)
}

test('@smoke map weapons on the Structures screen and the map editor: the files are what docs/SRPG.md says and the game loads', async ({ page, game }) => {
  test.setTimeout(180_000)
  const id = await weaponGame(game)
  expect(await game.problems(id)).toEqual([])
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Structures')
  await run(page, game, id, false)
})

test('@smoke on a 360 px phone the map weapon forms fit without sideways scrolling', async ({ page, game }) => {
  test.setTimeout(180_000)
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await weaponGame(game)
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Structures')
  await run(page, game, id, true)
})

test('deleting a weapon item takes it off the structures that fire it', async ({ page, game }) => {
  const id = await weaponGame(game)
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Items')
  await page.getByRole('button', { name: /Bolt/ }).first().click()
  await page.getByRole('button', { name: /^Delete/ }).first().click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('fires it as its weapon')
  await dlg.getByRole('button', { name: /^Delete/ }).click()
  await saved(page)
  for (const k of ['ballista', 'catapult']) expect(kind(id, k)).not.toHaveProperty('weapon')
  expect(kind(id, 'ballista')).not.toHaveProperty('by')
  expect(readDisk(id, 'maps/field.json').structures.find((p: any) => p.id === 'b2')).not.toHaveProperty('uses')
  expect(await game.problems(id)).toEqual([])
})
