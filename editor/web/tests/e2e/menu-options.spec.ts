// The Menu options card on the Rules screen (docs/SRPG.md "Menu options", docs/EDITOR_UI.md): what an action with nothing to act on does in
// the menu, and whether Attack asks for the weapon first. The defaults are written as no key; the saved file loads in the real game loader (the
// helper validates with it); no sideways scrolling. A desktop window and 360 px.
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`the two menu options are choices on the Rules screen, defaults are no key (${label})`, async ({ page, game }) => {
      const id = await game.create('strategy-rpg', `Menu Options ${label}`)
      await page.goto(editorUrl(id, 'rules'))
      const card = page.getByTestId('menu-options')
      await card.scrollIntoViewIfNeeded()
      const unavailable = card.getByRole('radiogroup', { name: 'Actions with nothing to act on' })
      const attack = card.getByRole('radiogroup', { name: 'Choosing Attack' })
      // today's behaviour is selected on a game without the keys
      await expect(unavailable.getByRole('radio', { name: 'Greyed out' })).toHaveAttribute('aria-checked', 'true')
      await expect(attack.getByRole('radio', { name: 'Go straight to the targets' })).toHaveAttribute('aria-checked', 'true')
      expect('menuUnavailable' in readDisk(id, 'game.json')).toBe(false)
      // left out
      await unavailable.getByRole('radio', { name: 'Left out' }).click()
      await saved(page)
      expect(readDisk(id, 'game.json').menuUnavailable).toBe('hide')
      await expect(card).toContainText('Wait is always there')
      // ask for the weapon first
      await attack.getByRole('radio', { name: 'Ask for the weapon first' }).click()
      await saved(page)
      expect(readDisk(id, 'game.json').attackWeaponChoice).toBe('pick')
      await expect(card).toContainText('puts it in hand, then attacks')
      expect(await game.problems(id)).toEqual([])
      await expectNoHScroll(page)
      // a reload keeps both
      await page.reload()
      await expect(page.getByTestId('menu-options').getByRole('radio', { name: 'Left out' })).toHaveAttribute('aria-checked', 'true')
      await expect(page.getByTestId('menu-options').getByRole('radio', { name: 'Ask for the weapon first' })).toHaveAttribute('aria-checked', 'true')
      // back to the defaults: no keys at all
      await page.getByTestId('menu-options').getByRole('radio', { name: 'Greyed out' }).click()
      await page.getByTestId('menu-options').getByRole('radio', { name: 'Go straight to the targets' }).click()
      await saved(page)
      const gj = readDisk(id, 'game.json')
      expect('menuUnavailable' in gj).toBe(false)
      expect('attackWeaponChoice' in gj).toBe(false)
      expect(await game.problems(id)).toEqual([])
    })

    test(`choosing the weapon with fixed weapons carries a note (${label})`, async ({ page, game }) => {
      const id = await game.create('strategy-rpg', `Menu Options Bad ${label}`)
      await page.goto(editorUrl(id, 'rules'))
      await page.getByTestId('menu-options').getByRole('radio', { name: 'Ask for the weapon first' }).click()
      await saved(page)
      // the template's weapons are fixed: the card notes that choosing matters most with carried weapons
      const gj = readDisk(id, 'game.json')
      if (gj.rules?.items?.weapons !== 'carried') await expect(page.getByTestId('menu-options').getByRole('alert')).toContainText('do not carry weapons')
      await expectNoHScroll(page)
    })
  })
}
