import { readFileSync, writeFileSync } from 'node:fs'
import { gotoSection, diskPath, editorUrl, expect, expectNoHScroll, readDisk, saved, test, writeDisk } from './fixtures'

test('@smoke a change on disk shows the conflict banner; Reload shows the disk state', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Conflict')
  await page.goto(editorUrl(id, 'game'))
  await expect(page.getByLabel('Name', { exact: true })).toHaveValue('Conflict')
  const g = readDisk(id, 'game.json')
  g.description = 'Edited elsewhere'
  writeDisk(id, 'game.json', JSON.stringify(g, null, 2))

  await page.getByLabel('Author').fill('Me')
  const banner = page.getByRole('alert').filter({ hasText: 'changed on disk' })
  await expect(banner).toBeVisible()
  await expect(page.getByText('Not saved: changed elsewhere')).toBeVisible()
  await banner.getByRole('button', { name: 'Reload' }).click()
  await expect(banner).toHaveCount(0)
  await expect(page.getByLabel('Description')).toHaveValue('Edited elsewhere')
  await expect(page.getByLabel('Author')).toHaveValue('')
  // Saving works again afterwards.
  await page.getByLabel('Author').fill('Again')
  await saved(page)
  expect(readDisk(id, 'game.json').author).toBe('Again')
})

test('Keep mine overwrites what is on disk', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'KeepMine')
  await page.goto(editorUrl(id, 'game'))
  await expect(page.getByLabel('Name', { exact: true })).toHaveValue('KeepMine')
  const g = readDisk(id, 'game.json')
  g.description = 'Edited elsewhere'
  writeDisk(id, 'game.json', JSON.stringify(g, null, 2))
  await page.getByLabel('Author').fill('Mine')
  await page.getByRole('button', { name: 'Keep mine' }).click()
  await saved(page)
  const now = readDisk(id, 'game.json')
  expect(now.author).toBe('Mine')
  expect(now.description).not.toBe('Edited elsewhere')
})

test('@smoke a broken file blocks only its section and is never written back', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Broken')
  const corrupt = '{ "units": [ this is not json'
  writeFileSync(diskPath(id, 'data/units.json'), corrupt)
  await page.goto(editorUrl(id, 'units'))
  await expect(page.getByText("This section can't be opened: data/units.json could not be read.")).toBeVisible()
  await expect(page.getByRole('button', { name: 'New unit' })).toHaveCount(0)
  await expect(page.getByRole('button', { name: /problem/ })).toBeVisible()

  await gotoSection(page, 'Terrain')
  await page.getByRole('button', { name: /^Plain/ }).click()
  await page.getByLabel('Name', { exact: true }).fill('Meadow')
  await saved(page)
  expect(readDisk(id, 'data/terrain.json').terrain[0].name).toBe('Meadow')
  expect(readFileSync(diskPath(id, 'data/units.json'), 'utf8')).toBe(corrupt)
  await gotoSection(page, 'Rules')
  await expect(page.getByLabel('Name of stat 1')).toBeVisible()
  await page.getByLabel('Add a resource').fill('Gold')
  await page.getByLabel('Add a resource').press('Enter')
  await saved(page)
  expect(readFileSync(diskPath(id, 'data/units.json'), 'utf8')).toBe(corrupt)
  // The problems panel names the broken file and clicking it goes to its section.
  await page.getByRole('button', { name: /problem/ }).click()
  await page.getByRole('button', { name: /data\/units\.json/ }).first().click()
  await expect(page).toHaveURL(/\/units$/)
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 } })

  test('every screen fits 360 px; navigate sections and edit a unit', async ({ page, game }) => {
    const id = await game.create('strategy-rpg', 'Phone')
    await page.goto('/')
    await expectNoHScroll(page)
    await page.goto(editorUrl(id, 'game'))
    for (const section of ['Game', 'Rules', 'Units', 'Weapons', 'Terrain', 'Maps']) {
      await gotoSection(page, section)
      await expect(page).toHaveURL(new RegExp(`/${section.toLowerCase()}$`))
      await expectNoHScroll(page)
      if (['Units', 'Weapons', 'Terrain', 'Maps'].includes(section)) {
        // list first, then the item opens as its own view
        const first = page.locator('.md-items button').first()
        await expect(first).toBeVisible()
        await first.click()
        await expect(page.locator('.md-detail-body')).toBeVisible()
        await expectNoHScroll(page)
        await page.getByRole('button', { name: section, exact: true }).click()
        await expect(page.locator('.md-items')).toBeVisible()
      }
    }
    await gotoSection(page, 'Units')
    await page.getByRole('button', { name: /^Archer/ }).click()
    await page.getByLabel('Name', { exact: true }).fill('Longbow Archer')
    await page.getByLabel('Strength Value').fill('11')
    await saved(page)
    expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'archer')).toMatchObject({ name: 'Longbow Archer' })
    await page.goBack() // browser back leaves the unit view
    await expect(page.locator('.md-items')).toBeVisible()
    await expectNoHScroll(page)
  })
})
