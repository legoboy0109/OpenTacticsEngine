// Renaming a character's id on the Characters screen (Advanced > Internal name > Rename; docs/EDITOR_UI.md "Renaming a character"):
// the refusals in plain words, the list of where the new name goes, every file following in one step, one Undo, the real loader accepting
// the result (/problems) and the real engine loading it (Play); then the same dialog at 360 px with no sideways scroll.
import type { Page } from '@playwright/test'
import { readdirSync, readFileSync, statSync } from 'node:fs'
import path from 'node:path'
import { diskPath, editorUrl, expect, expectNoHScroll, gamesDir, gotoSection, readDisk, requirePlayer, saved, test } from './fixtures'
import { shot } from './rankskit'

test.use({ actionTimeout: 10_000 })

/** Every game file that holds the quoted id (JSON files only: the art images are not text). */
function filesHolding(id: string, quoted: string): string[] {
  const root = path.join(gamesDir(), id)
  const found: string[] = []
  const walk = (dir: string): void => {
    for (const name of readdirSync(dir)) {
      const full = path.join(dir, name)
      if (statSync(full).isDirectory()) walk(full)
      else if (name.endsWith('.json') && readFileSync(full, 'utf8').includes(quoted)) found.push(path.relative(root, full))
    }
  }
  walk(root)
  return found.sort()
}

const openRename = async (page: Page) => {
  const rename = page.getByRole('button', { name: 'Rename…' })
  if (!(await rename.isVisible())) await page.getByText('Advanced', { exact: true }).click()   // (the area stays open once it is opened)
  await rename.click()
  const dlg = page.getByRole('dialog', { name: /Rename the internal name of/ })
  await expect(dlg).toBeVisible()
  return dlg
}

test('@smoke rename a character: refusals in plain words, every file follows in one step, Undo brings it back, the loader and the engine accept it', async ({ page, game }) => {
  test.setTimeout(120_000)
  requirePlayer()
  const id = await game.create('strategy-rpg', 'Rename', { srpgContent: true })
  // Before: Eirik is named in the character list, the map, the supports and the portraits.
  const before = filesHolding(id, '"eirik"')
  expect(before).toEqual(expect.arrayContaining(['data/characters.json', 'data/graphics.json', 'data/supports.json', 'maps/ford.json']))

  await page.goto(editorUrl(id, 'characters', 'eirik'))
  const dlg = await openRename(page)
  const field = dlg.getByLabel('New internal name')
  const confirm = dlg.getByRole('button', { name: 'Rename', exact: true })
  await expect(field).toHaveValue('eirik')
  await expect(confirm).toBeDisabled()
  // The list of where the new name goes, before anything is changed.
  const follows = dlg.getByTestId('rename-follows')
  await expect(follows).toContainText('The map “The Ford” places it')
  await expect(follows).toContainText('Supports')
  await expect(follows).toContainText('portrait')
  await expect(dlg).toContainText('Saves of this game made before the rename will no longer load')

  // Refusals.
  await field.fill('kell')
  await expect(dlg).toContainText('The character “Kell” already has this internal name.')
  await expect(confirm).toBeDisabled()
  await field.fill('bad!')
  await expect(dlg).toContainText('Use letters, digits, _ and - only')
  await expect(confirm).toBeDisabled()
  await field.fill('')
  await expect(dlg).toContainText('Type the new internal name.')
  await field.fill('my captain')   // a space turns into an underscore as it is typed
  await expect(field).toHaveValue('my_captain')
  await expect(confirm).toBeEnabled()
  await dlg.getByRole('button', { name: 'Make it from the name' }).click()
  await expect(field).toHaveValue('eirik')
  await field.fill('captain')
  await expect(confirm).toBeEnabled()
  await shot(page, 'desktop-rename-dialog')
  // Cancel changes nothing.
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  await expect(dlg).toBeHidden()
  expect(filesHolding(id, '"eirik"')).toEqual(before)

  // The rename.
  const dlg2 = await openRename(page)
  await dlg2.getByLabel('New internal name').fill('captain')
  await dlg2.getByRole('button', { name: 'Rename', exact: true }).click()
  await expect(page).toHaveURL(/\/characters\/captain$/)
  await expect(page.getByRole('heading', { name: 'Eirik', level: 2 })).toBeVisible()   // the person is still Eirik: only the internal name changed
  await saved(page)
  expect(filesHolding(id, '"eirik"')).toEqual([])
  expect(readDisk(id, 'data/characters.json').characters.map((c: any) => c.id)).toEqual(['captain', 'kell', 'bram', 'gorm', 'vex'])
  expect(readDisk(id, 'maps/ford.json').units.find((u: any) => u.character === 'captain')).toBeTruthy()
  expect(readDisk(id, 'data/supports.json').pairs[0]).toMatchObject({ a: 'captain', b: 'kell' })
  expect(Object.keys(readDisk(id, 'data/graphics.json').portraits)).toContain('captain')
  expect(await game.problems(id)).toEqual([])
  // The new id is what the rename dialog shows next time.
  await expect(page.getByLabel('Internal name')).toHaveValue('captain')

  // One Undo puts every file back.
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(filesHolding(id, '"captain"')).toEqual([])
  expect(filesHolding(id, '"eirik"')).toEqual(before)
  expect(await game.problems(id)).toEqual([])

  // Rename again and let the real engine load the result.
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  const dlg3 = await openRename(page)
  await dlg3.getByLabel('New internal name').fill('captain')
  await dlg3.getByRole('button', { name: 'Rename', exact: true }).click()
  await saved(page)
  await page.goto(editorUrl(id, 'maps', 'ford'))
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  expect(diskPath(id, 'data/characters.json')).toBeTruthy()
})

test('@smoke a formula that spells the old id in quotes is listed as not changed', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Rename formula', { srpgContent: true })
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.game.formulas.damage = `${doc.game.formulas.damage} + (a.character == "eirik" and 1 or 0)`
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game } })
  expect(res.ok(), await res.text()).toBeTruthy()
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  const dlg = await openRename(page)
  await expect(dlg).toContainText('These are not changed')
  await expect(dlg).toContainText('The rules’ formulas spell “eirik” in quotes')
})

test.describe('at 360 px', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
  test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

  test('@smoke the rename dialog fits, and the rename works from the phone layout', async ({ page, game }) => {
    const id = await game.create('strategy-rpg', 'Rename phone', { srpgContent: true })
    await page.goto(editorUrl(id, 'game'))
    await gotoSection(page, 'Characters')
    await page.getByRole('button', { name: /Eirik/ }).first().click()
    const dlg = await openRename(page)
    await expectNoHScroll(page)
    const box = await dlg.boundingBox()
    expect(box!.x).toBeGreaterThanOrEqual(0)
    expect(box!.x + box!.width).toBeLessThanOrEqual(360)
    await dlg.getByLabel('New internal name').fill('kell')
    await expect(dlg).toContainText('already has this internal name')
    await shot(page, 'phone-rename-dialog')
    await dlg.getByLabel('New internal name').fill('captain')
    await dlg.getByRole('button', { name: 'Rename', exact: true }).click()
    await expect(page).toHaveURL(/\/characters\/captain$/)
    await saved(page)
    expect(filesHolding(id, '"eirik"')).toEqual([])
    await expectNoHScroll(page)
  })
})
