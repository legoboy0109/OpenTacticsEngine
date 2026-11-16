// E1 at 360 px (docs/EDITOR_UI.md): the Characters screen and the map's Characters tab never scroll sideways and every
// control stays reachable. Chromium only (Firefox has no mobile emulation).
import { newCharacter, switchOn } from './characterskit'
import { expect, expectNoHScroll, saved, test } from './fixtures'
import { clickCell, openMap, pickSide, tool } from './mapkit'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

test('Characters at 360 px: list, form and the map picker fit, with no horizontal scroll', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Phone characters')
  await switchOn(page, id)
  await expectNoHScroll(page)
  await expect(page.getByText('No characters yet').first()).toBeVisible()
  await newCharacter(page, 'Aldric the Bold with a Very Long Name Indeed')
  // On a phone the form opens on its own page with a way back.
  await expect(page.getByRole('button', { name: /^Characters$/ })).toBeVisible()
  await expectNoHScroll(page)
  const form = page.locator('.md-detail')
  await form.getByLabel('Strength personal bonus').fill('3')
  await expect(form.getByTestId('total-str').first()).toBeVisible()
  await expectNoHScroll(page)
  // The three-column table fits the width too.
  const table = await form.locator('table.three-col').first().boundingBox()
  expect(table!.x + table!.width).toBeLessThanOrEqual(360)
  await saved(page)
  await page.getByRole('button', { name: /^Characters$/ }).click()
  await expectNoHScroll(page)

  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await page.getByRole('tab', { name: 'Characters', exact: true }).click()
  await pickSide(page, 'Blue').click()
  await page.getByRole('radiogroup', { name: 'Characters' }).getByRole('radio').first().click()
  await expectNoHScroll(page)
  await clickCell(page, 3, 1)
  await expect(page.getByTestId('unit-character')).toBeVisible()
  await expectNoHScroll(page)
})
