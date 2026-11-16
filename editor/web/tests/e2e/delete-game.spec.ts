import { existsSync } from 'node:fs'
import { diskPath, expect, test } from './fixtures'

test('a game is deleted for good only after its name is typed; the demos have no Delete', async ({ page, request }) => {
  const id = `delete-me-${Date.now().toString(36)}`
  const name = `Delete Me ${id.slice(-5)}`
  expect((await request.post('/api/projects', { data: { id, name, template: 'blank' } })).status()).toBe(201)
  expect(existsSync(diskPath(id, 'game.json'))).toBe(true)

  await page.goto('/')
  await expect(page.getByRole('link', { name: /Story Tactics Demo/ })).toBeVisible()
  // The demos that come with the editor cannot be deleted.
  for (const demo of ['Story Tactics Demo', 'Wars Demo', 'Hex Wars Demo'])
    await expect(page.getByRole('button', { name: `Delete ${demo}`, exact: true })).toHaveCount(0)

  // Cancel changes nothing.
  await page.getByRole('button', { name: `Delete ${name}`, exact: true }).click()
  const dialog = page.getByRole('dialog')
  await expect(dialog).toContainText('It cannot be undone.')
  await expect(dialog).toContainText(id)
  const go = dialog.getByRole('button', { name: 'Delete for good' })
  await expect(go).toBeDisabled()
  await dialog.getByRole('button', { name: 'Cancel' }).click()
  await expect(dialog).toHaveCount(0)
  expect(existsSync(diskPath(id, 'game.json'))).toBe(true)

  // A wrong name keeps the button off; the right one (capitals forgiven) deletes.
  await page.getByRole('button', { name: `Delete ${name}`, exact: true }).click()
  const box = dialog.getByLabel('Type the game’s name to confirm')
  await box.fill('Story Tactics Demo')
  await expect(go).toBeDisabled()
  await box.press('Enter')
  expect(existsSync(diskPath(id, 'game.json'))).toBe(true)
  await box.fill(name.toLowerCase())
  await expect(go).toBeEnabled()
  await go.click()
  await expect(dialog).toHaveCount(0)
  await expect(page.getByRole('link', { name: new RegExp(name) })).toHaveCount(0)
  await expect(page.getByRole('link', { name: /Story Tactics Demo/ })).toBeVisible()
  expect(existsSync(diskPath(id, '.'))).toBe(false)
  expect(existsSync(diskPath('demo-srpg', 'game.json'))).toBe(true)
})

test('on a phone the Delete button stays inside its card and the page does not scroll sideways', async ({ page, request }) => {
  const id = `delete-phone-${Date.now().toString(36)}`
  expect((await request.post('/api/projects', { data: { id, name: 'Phone Delete Game With A Rather Long Name', template: 'blank' } })).status()).toBe(201)
  await page.setViewportSize({ width: 360, height: 700 })
  await page.goto('/')
  const btn = page.getByRole('button', { name: /^Delete Phone Delete Game/ })
  await expect(btn).toBeVisible()
  const card = page.locator('li', { has: btn })
  const [b, c] = [await btn.boundingBox(), await card.boundingBox()]
  expect(b!.x >= c!.x && b!.x + b!.width <= c!.x + c!.width && b!.y + b!.height <= c!.y + c!.height).toBe(true)
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
  await request.delete(`/api/projects/${id}?confirm=${id}`)
})
