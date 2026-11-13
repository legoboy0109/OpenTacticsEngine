import path from 'node:path'
import { tmpdir } from 'node:os'
import { expect, test } from './fixtures'
import { startHelper, type Helper } from './helper'

// A second helper started WITHOUT a web player folder (the player build is optional for the editor itself).
let helper: Helper
test.beforeAll(async () => { helper = await startHelper(path.join(tmpdir(), 'ote-no-such-player-folder')) })
test.afterAll(async () => { await helper?.stop() })

test('@smoke without a web player build, Play explains how to build it and starts nothing', async ({ page, playwright }) => {
  const api = await playwright.request.newContext({ baseURL: helper.url })
  const res = await api.post('/api/projects', { data: { id: 'noplayer', name: 'No Player', template: 'blank' } })
  expect(res.status()).toBe(201)
  expect((await api.head('/player/ote-player.wasm')).status()).toBe(404)

  await page.goto(`${helper.url}/p/noplayer/maps/start`)
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const dlg = page.getByRole('dialog', { name: 'The web player has not been built' })
  await expect(dlg).toBeVisible()
  await expect(dlg.getByTestId('player-build-command')).toContainText('emcmake cmake -S . -B build-web')
  await expect(dlg.getByTestId('player-build-command')).toContainText('cmake --build build-web')
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toHaveCount(0)
  await dlg.getByRole('button', { name: 'OK' }).click()
  await expect(dlg).toBeHidden()
  await expect(page.getByRole('button', { name: 'Play', exact: true })).toBeFocused()
})
