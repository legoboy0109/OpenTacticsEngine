// The Battle effects card of the Art screen (docs/BATTLE_SCENE.md "Effects", docs/EDITOR_UI.md "Art"): adding effects, their sounds, hit lead-in, layer,
// flash and shake, an effect on the striker, a flying one that returns and lingers, lists and by-unit-type choices, and a refusal in the
// loader's terms. What the editor writes must load in the real game loader. Desktop window and 360 px.
import { expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { openArt, seedArt, sheetPng } from './artkit'
import { seedSound, silentGame, wav } from './soundkit'

const SHOTS = process.env.BATTLE_SHOTS

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`effects: sounds, lead-in, layers, flash and shake, striker, return, lists and by type (${label})`, async ({ page, request, game }) => {
      const id = await silentGame(request, game, 'strategy-rpg', `Battle Effects ${label}`)
      await seedSound(request, id, { 'zap.wav': wav(0.2), 'boom.wav': wav(0.3, 22050, 220) }, { sounds: { zap: { file: 'audio/zap.wav' }, boom: { file: 'audio/boom.wav' } } })
      await seedArt(request, id, { fx: sheetPng(4, 2, 16) }, { sheets: [{ id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16, name: 'Effects' }] })
      await openArt(page, id)
      const card = page.getByTestId('battle-effects')
      await card.scrollIntoViewIfNeeded()
      await expect(card.getByText('No effects yet.')).toBeVisible()
      const weapons = readDisk(id, 'data/weapons.json').weapons
      const units = readDisk(id, 'data/units.json').units
      const fx = () => readDisk(id, 'data/graphics.json').battle.effects

      // 1. a weapon's effect on the foe, with a lead-in, behind the fighters, with two sounds, a flash and a shake
      await card.getByRole('button', { name: 'Add effect' }).click()
      await saved(page)
      expect(fx()[0]).toMatchObject({ weapon: weapons[0].id, sheet: 'fx', frames: [[0, 0]] })
      const e1 = card.getByTestId('effect-0')
      await e1.getByLabel('Effect 1: Name').fill('charge')
      await e1.getByLabel('Effect 1: Frames').fill('3')
      await e1.getByLabel('Effect 1: Appears on').selectOption('striker')
      await e1.getByLabel('Effect 1: Starts before the hit (ms)').fill('233')
      await e1.getByLabel('Effect 1: Drawn').selectOption('behind')
      await e1.getByRole('button', { name: 'Add a sound (effect 1)' }).click()
      await e1.getByLabel('Sound at 0 ms', { exact: true }).selectOption('boom')
      await e1.getByRole('button', { name: 'Add a sound (effect 1)' }).click()
      await e1.getByLabel('Sound at 100 ms', { exact: true }).selectOption('boom')
      await e1.getByLabel('Sounds: Sound: milliseconds after the start').nth(1).fill('233')
      await e1.getByRole('button', { name: 'Add a flash (effect 1)' }).click()
      await e1.getByLabel('Flashes 1: Starts (ms after the effect)').fill('100')
      await e1.getByLabel('Flashes 1: Colour').fill('#ff8000')
      await e1.getByRole('button', { name: 'Add a shake (effect 1)' }).click()
      await e1.getByLabel('Shakes 1: Strength (pixels)').fill('3')
      await saved(page)
      expect(fx()[0]).toMatchObject({
        id: 'charge', weapon: weapons[0].id, frames: [[0, 0], [1, 0], [2, 0]], at: 'striker', hitAtMs: 233, layer: 'behind',
        sounds: { '0': 'boom', '233': 'boom' }, flash: [{ atMs: 100, ms: 80, color: '#ff8000' }], shake: [{ atMs: 0, ms: 200, strength: 3 }],
      })
      expect(await game.problems(id)).toEqual([])

      // 2. a library effect that flies, comes back and stays at the foe a moment
      await card.getByRole('button', { name: 'Add effect' }).click()
      const e2 = card.getByTestId('effect-1')
      await e2.getByLabel(/^Effect 2: Plays for/).selectOption('library')
      await e2.getByLabel('Effect 2: Name').fill('bolt')
      await e2.getByLabel('Effect 2: How it moves').selectOption('travel')
      await e2.getByLabel('Effect 2: Flight time (ms)').fill('300')
      await e2.getByLabel('Effect 2: Stays at the foe (ms)').fill('120')
      await e2.locator('label.toggle').filter({ hasText: 'Flies back after arriving' }).click()
      await saved(page)
      expect(fx()[1]).toMatchObject({ id: 'bolt', mode: 'travel', travelMs: 300, lingerMs: 120, returns: true })
      expect(fx()[1]).not.toHaveProperty('weapon')
      await expect(e2.getByLabel('Effect 2: Starts before the hit (ms)')).toHaveCount(0)  // (a flying effect hits on arrival)
      await expect(e2.getByLabel('Effect 2: Appears on')).toHaveCount(0)
      expect(await game.problems(id)).toEqual([])

      // 3. a screen-covering effect that is turned round with the striker
      await card.getByRole('button', { name: 'Add effect' }).click()
      const e3 = card.getByTestId('effect-2')
      await e3.getByLabel(/^Effect 3: Plays for/).selectOption('library')
      await e3.getByLabel('Effect 3: Name').fill('wash')
      await e3.getByLabel('Effect 3: Drawn').selectOption('screen')
      await e3.locator('label.toggle').filter({ hasText: 'Turn round when the striker is on the right' }).click()
      await saved(page)
      expect(fx()[2]).toMatchObject({ id: 'wash', layer: 'screen', mirror: true })

      // 4. the weapon plays the list "bolt" after its own pictures, and units of one type play "wash" instead
      await e1.getByRole('button', { name: 'Add an effect to the list' }).click()
      await e1.getByLabel('Also plays (after this one’s own pictures): Listed effect 1').selectOption('bolt')
      await e1.getByRole('button', { name: 'Add a unit type' }).click()
      await saved(page)
      const type = units[0].id
      expect(fx()[0]).toMatchObject({ effects: 'bolt', byType: { [type]: [] } })
      await e1.getByRole('button', { name: `Add an effect for ${type}` }).click()
      await e1.getByLabel(`${type}: ${type}: effect 1`).selectOption('wash')
      await saved(page)
      expect(fx()[0].byType).toEqual({ [type]: 'wash' })
      expect(await game.problems(id)).toEqual([])
      if (SHOTS) await card.screenshot({ path: `${SHOTS}/editor-effects-card-${label}.png` })

      // 5. a refusal in the loader's terms: a second effect for the same weapon
      await card.getByRole('button', { name: 'Add effect' }).click()
      const e4 = card.getByTestId('effect-3')
      await e4.getByLabel(/^Effect 4: Plays for/).selectOption('weapon')
      await e4.getByLabel('Effect 4: Weapon').selectOption(weapons[0].id)
      await saved(page)
      await expect(e4.getByRole('alert').filter({ hasText: 'there is already an effect for' })).toBeVisible()
      expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('a second effect for'))).toBe(true)
      // deleting it clears the problem; deleting "bolt" takes it out of the list that named it
      await card.getByRole('button', { name: /^Delete effect/ }).nth(3).click()
      await card.getByRole('button', { name: 'Delete effect bolt' }).click()
      await saved(page)
      expect(fx()[0].effects).toBeUndefined()
      expect(await game.problems(id)).toEqual([])
      await expectNoHScroll(page)
    })
  })
}
