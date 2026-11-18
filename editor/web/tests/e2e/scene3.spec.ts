// The third batch of battle scene editor parts (docs/BATTLE_SCENE.md; docs/EDITOR_UI.md "Battle animations"): the Rules screen's "Show staves in the battle
// scene", an effect's blending, position among the layers, flight speed, hiding windows and fades and a staff item as its weapon (Art > Battle effects), and
// the Stage card's layouts by distance with the one-fighter-at-a-time camera. What the editor writes must load in the real game loader.
// Desktop window and 360 px.
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test, writeDisk } from './fixtures'
import { openArt, seedArt, sheetPng } from './artkit'

const SHOTS = process.env.BATTLE_SHOTS

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`staves in the scene, effect options and range bands (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Scene Three ${label}`)
      // a staff item for the effect's weapon list (the template has none)
      writeDisk(id, 'data/items.json', JSON.stringify({ items: [{ id: 'mend_staff', name: 'Mend Staff', kind: 'staff', group: 'staff', uses: 3, range: 2, target: 'ally', effects: [{ heal: 5 }] }] }, null, 1))

      // ===== Rules: the switch =====
      await page.goto(editorUrl(id, 'rules'))
      const choices = page.getByTestId('battle-scene-choices')
      await choices.scrollIntoViewIfNeeded()
      const sw = choices.getByRole('switch', { name: /Show staves in the battle scene/ })
      await expect(sw).not.toBeChecked()
      await choices.locator('label.toggle').filter({ hasText: 'Show staves in the battle scene' }).first().click()
      await expect(sw).toBeChecked()
      await saved(page)
      expect(readDisk(id, 'game.json').battleSceneStaves).toBe(true)
      expect(await game.problems(id)).toEqual([])
      if (label === 'phone') await expectNoHScroll(page)
      if (SHOTS) await choices.screenshot({ path: `${SHOTS}/editor-scene3-rules-${label}.png` })
      await choices.locator('label.toggle').filter({ hasText: 'Show staves in the battle scene' }).first().click()
      await expect(sw).not.toBeChecked()
      await saved(page)
      expect(readDisk(id, 'game.json')).not.toHaveProperty('battleSceneStaves')  // off is no key

      // ===== Art: the stage with a layout by distance =====
      await seedArt(request, id, { fx: sheetPng(4, 2, 16) }, { sheets: [{ id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16, name: 'Effects' }] })
      await openArt(page, id)
      const stage = page.getByTestId('battle-stage')
      await stage.scrollIntoViewIfNeeded()
      await stage.locator('label.toggle').filter({ hasText: 'Use a fixed stage' }).first().click()
      await saved(page)
      await expect(stage.getByTestId('stage-bands')).toBeVisible()
      await stage.getByRole('button', { name: 'Add a layout' }).click()
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.stage.ranges).toEqual([{ from: 4, feet: { left: [expect.any(Number), expect.any(Number)], right: [expect.any(Number), expect.any(Number)] } }])
      expect(await game.problems(id)).toEqual([])
      await stage.getByLabel('Layout 1: From (squares apart)').fill('5')
      await stage.getByLabel('Layout 1: Shows').selectOption('each')
      await saved(page)
      const band = () => readDisk(id, 'data/graphics.json').battle.stage.ranges[0]
      expect(band()).toMatchObject({ from: 5, view: 'each', pan: { panMs: 250, leftView: 0 } })
      expect(await game.problems(id)).toEqual([])
      await expect(stage.getByTestId('band-0-pan')).toBeVisible()
      // a view that does not show its fighter is refused in the loader's terms
      const width = readDisk(id, 'data/graphics.json').battle.stage.width
      await stage.getByLabel('Layout 1: Right view').fill('0')
      await saved(page)
      await expect(stage.getByRole('alert').filter({ hasText: 'is not inside the picture' })).toBeVisible()
      expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('is not inside the picture while the camera is on it'))).toBe(true)
      await stage.getByLabel('Layout 1: Right view').fill(String(width))
      await stage.getByLabel('Layout 1: Camera time (ms)').fill('400')
      await saved(page)
      await expect(stage.getByRole('alert').filter({ hasText: 'is not inside the picture' })).toHaveCount(0)
      expect(band().pan).toEqual({ panMs: 400, leftView: 0, rightView: width })
      expect(await game.problems(id)).toEqual([])
      if (label === 'phone') await expectNoHScroll(page)
      if (SHOTS) await stage.screenshot({ path: `${SHOTS}/editor-scene3-bands-${label}.png` })
      await stage.getByLabel('Layout 1: Shows').selectOption('both')
      await saved(page)
      expect(band()).not.toHaveProperty('pan')
      expect(await game.problems(id)).toEqual([])
      await stage.getByRole('button', { name: 'Remove layout 1' }).click()
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.stage).not.toHaveProperty('ranges')  // an empty list is no key

      // ===== Art: an effect for the staff, with blending, position, speed, hide and fade =====
      const card = page.getByTestId('battle-effects')
      await card.scrollIntoViewIfNeeded()
      await card.getByRole('button', { name: 'Add effect' }).click()
      await saved(page)
      const e1 = card.getByTestId('effect-0')
      await e1.getByLabel('Effect 1: Weapon', { exact: true }).selectOption('mend_staff')
      await e1.getByLabel('Effect 1: Blending').selectOption('add')
      await e1.getByRole('button', { name: 'Add a hiding window (effect 1)' }).click()
      await e1.getByLabel('Hide the backdrop or platforms 1: Lasts (ms)').fill('900')
      await e1.getByLabel('Hide the backdrop or platforms 1: Fill colour').fill('#102030')
      await e1.getByRole('button', { name: 'Add a fade (effect 1)' }).click()
      await e1.getByLabel('Fade the scene 1: Over').selectOption('backdrop')
      await e1.getByLabel('Fade the scene 1: How strong (0 to 1)').fill('0.5')
      await saved(page)
      const fx = () => readDisk(id, 'data/graphics.json').battle.effects
      expect(fx()[0]).toMatchObject({
        weapon: 'mend_staff', blend: 'add', hide: [{ atMs: 0, ms: 900, what: ['backdrop'], color: '#102030' }],
        fade: [{ atMs: 0, ms: 300, to: '#000000', alpha: 0.5, holdMs: 300, over: 'backdrop' }],
      })
      expect(await game.problems(id)).toEqual([])
      // both layers hidden; at least one stays
      await e1.getByTestId('effect-hide-0').locator('label.toggle').filter({ hasText: 'The platforms' }).first().click()
      await saved(page)
      expect(fx()[0].hide[0].what.sort()).toEqual(['backdrop', 'platforms'])
      expect(await game.problems(id)).toEqual([])
      // a stage-covering picture and where it goes
      await e1.getByLabel('Effect 1: Drawn', { exact: true }).selectOption('screen')
      await e1.getByLabel('Effect 1: Position among the layers').selectOption('front')
      await saved(page)
      expect(fx()[0]).toMatchObject({ layer: 'screen', order: 'front' })
      await e1.getByLabel('Effect 1: Drawn', { exact: true }).selectOption('front')
      await saved(page)
      expect(fx()[0]).not.toHaveProperty('order')  // the position goes with the layer
      expect(await game.problems(id)).toEqual([])
      // a flight by speed: refused without a stage, accepted with one
      await card.getByRole('button', { name: 'Add effect' }).click()
      const e2 = card.getByTestId('effect-1')
      await e2.getByLabel('Effect 2: How it moves').selectOption('travel')
      await e2.getByLabel('Effect 2: Flight lasts').selectOption('speed')
      await saved(page)
      expect(fx()[1]).toMatchObject({ mode: 'travel', speed: 240 })
      expect(fx()[1]).not.toHaveProperty('travelMs')
      expect(await game.problems(id)).toEqual([])  // (the stage from above is still on)
      await stage.locator('label.toggle').filter({ hasText: 'Use a fixed stage' }).first().click()
      await saved(page)
      await expect(e2.getByRole('alert').filter({ hasText: 'needs a fixed stage' })).toBeVisible()
      expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('needs a fixed "stage"'))).toBe(true)
      await e2.getByLabel('Effect 2: Flight lasts').selectOption('time')
      await saved(page)
      expect(fx()[1]).not.toHaveProperty('speed')
      expect(await game.problems(id)).toEqual([])
      if (label === 'phone') await expectNoHScroll(page)
      if (SHOTS) await e1.screenshot({ path: `${SHOTS}/editor-scene3-effect-${label}.png` })
    })
  })
}
