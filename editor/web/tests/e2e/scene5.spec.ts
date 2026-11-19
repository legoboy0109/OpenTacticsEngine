// The fifth batch of opt-in battle scene keys in the editor (docs/BATTLE_SCENE.md "Holding a pose", "Platforms", "Effect timing by distance", "Range bands"):
// the Rules screen's platform choice, an animation's "Waits on frame" / "Dodge starts at frame" / "Dodge starts later by", the scene's waiting time, and an
// effect's end offset by distance, camera start and "Not moved by distance". What the editor writes must load in the real game loader.
// Desktop window and 360 px.
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test, writeDisk } from './fixtures'
import { openArt, seedArt, sheetPng } from './artkit'

const SHOTS = process.env.BATTLE_SHOTS

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`the platform choice, held poses and the effect keys of the fifth batch (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Scene Five ${label}`)
      writeDisk(id, 'data/items.json', JSON.stringify({ items: [{ id: 'mend_staff', name: 'Mend Staff', kind: 'staff', group: 'staff', uses: 3, range: 2, target: 'ally', effects: [{ heal: 5 }] }] }, null, 1))

      // ===== Rules: which platform each fighter stands on =====
      await page.goto(editorUrl(id, 'rules'))
      const choices = page.getByTestId('battle-scene-choices')
      await choices.scrollIntoViewIfNeeded()
      const pick = choices.getByLabel('Which platform each fighter stands on')
      await expect(pick).toHaveValue('same')
      await pick.selectOption('own')
      await saved(page)
      expect(readDisk(id, 'game.json').battlePlatformFrom).toBe('own')
      await expect(choices.getByRole('alert').filter({ hasText: 'no platforms yet' })).toBeVisible()  // it changes nothing without platforms
      expect(await game.problems(id)).toEqual([])
      if (label === 'phone') await expectNoHScroll(page)
      await pick.selectOption('same')
      await saved(page)
      expect(readDisk(id, 'game.json')).not.toHaveProperty('battlePlatformFrom')  // "same" is no key

      // ===== Art: a set with a strike and a dodge that wait =====
      await seedArt(request, id, { fighters: sheetPng(6, 3), fx: sheetPng(4, 2, 16) }, {
        sheets: [{ id: 'fighters', file: 'art/fighters.png', cellWidth: 24, cellHeight: 24, name: 'Fighters' }, { id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16, name: 'Effects' }],
      })
      await openArt(page, id)
      const anims = page.getByTestId('battle-animations')
      await anims.scrollIntoViewIfNeeded()
      await page.getByLabel('Name of a new set').fill('knight')
      await page.getByRole('button', { name: 'Add set' }).click()
      await saved(page)
      await page.getByRole('button', { name: 'Add Attack' }).click()
      await page.getByLabel('Attack: Row').fill('2')
      await page.getByLabel('Attack: Frames').fill('4')
      await page.getByLabel('Attack: Hit on frame').fill('3')
      await page.getByLabel('Attack: Waits on frame').fill('4')
      await page.getByRole('button', { name: 'Add Dodge', exact: true }).click()
      await page.getByLabel('Dodge: Row').fill('3')
      await page.getByLabel('Dodge: Frames').fill('6')
      await page.getByLabel('Dodge: Dodge starts at frame').fill('3')
      await page.getByLabel('Dodge: Waits on frame').fill('4')
      await page.getByLabel('Dodge: Dodge starts later by (ms)').fill('80')
      await saved(page)
      const set = () => readDisk(id, 'data/graphics.json').battle.sets.knight.animations
      expect(set().attack).toMatchObject({ hit: 2, holdFrame: 3 })
      expect(set().dodge).toMatchObject({ startFrame: 2, holdFrame: 3, startDelayMs: 80 })
      expect(await game.problems(id)).toEqual([])
      // a wait before the blow is refused in the loader's terms, and goes away when fixed
      await page.getByLabel('Attack: Waits on frame').fill('2')
      await saved(page)
      await expect(page.getByTestId('battle-checks')).toContainText('before the hit frame 3')
      expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('comes before the hit frame 2'))).toBe(true)
      await page.getByLabel('Attack: Waits on frame').fill('')
      await saved(page)
      expect(set().attack).not.toHaveProperty('holdFrame')
      expect(await game.problems(id)).toEqual([])
      // the waiting time of the scene: 533 and 0 are the defaults and are written as no key
      const wait = page.getByTestId('scene-timing')
      await wait.scrollIntoViewIfNeeded()
      await expect(wait.getByLabel('Result shown for (ms)')).toHaveValue('533')
      await wait.getByLabel('Result shown for (ms)').fill('700')
      await wait.getByLabel('Longer when the same fighter strikes again (ms)').fill('120')
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.scene).toEqual({ resultHoldMs: 700, resultHoldAgainMs: 120 })
      expect(await game.problems(id)).toEqual([])
      await wait.getByLabel('Result shown for (ms)').fill('533')
      await wait.getByLabel('Longer when the same fighter strikes again (ms)').fill('0')
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle).not.toHaveProperty('scene')
      if (label === 'phone') await expectNoHScroll(page)
      if (SHOTS) await anims.screenshot({ path: `${SHOTS}/editor-scene5-anims-${label}.png` })

      // ===== Art: a flying effect with an end offset by distance, a camera start and a row that is not moved =====
      const card = page.getByTestId('battle-effects')
      await card.scrollIntoViewIfNeeded()
      await card.getByRole('button', { name: 'Add effect' }).click()
      await saved(page)
      const e1 = card.getByTestId('effect-0')
      await e1.getByLabel('Effect 1: Weapon', { exact: true }).selectOption('mend_staff')
      await e1.getByLabel('Effect 1: How it moves').selectOption('travel')
      await e1.getByLabel('Effect 1: Stops short of the foe (pixels)').fill('-40')
      await e1.getByLabel('Effect 1: Camera leaves after (ms)').fill('300')
      await saved(page)
      const fx = () => readDisk(id, 'data/graphics.json').battle.effects[0]
      expect(fx()).toMatchObject({ mode: 'travel', endOffset: -40, panAtMs: 300 })
      expect(await game.problems(id)).toEqual([])
      // the box stops at the loader's limits (-400 to 400): a number beyond is said, not written
      await e1.getByLabel('Effect 1: Stops short of the foe (pixels)').fill('401')
      await expect(e1.getByRole('alert').filter({ hasText: 'At most 400.' })).toBeVisible()
      await e1.getByLabel('Effect 1: Stops short of the foe (pixels)').fill('-401')
      await expect(e1.getByRole('alert').filter({ hasText: 'At least -400.' })).toBeVisible()
      await e1.getByLabel('Effect 1: Stops short of the foe (pixels)').fill('-400')
      await saved(page)
      expect(fx().endOffset).toBe(-400)
      expect(await game.problems(id)).toEqual([])
      await e1.getByLabel('Effect 1: Stops short of the foe (pixels)').fill('-40')
      await saved(page)
      // "Not moved by distance" on the effect itself (only for an effect with no delays of its own)
      await e1.locator('label.toggle').filter({ hasText: 'Not moved by distance' }).first().click()
      await saved(page)
      expect(fx().atRangeFixed).toBe(true)
      expect(await game.problems(id)).toEqual([])
      await e1.locator('label.toggle').filter({ hasText: 'Not moved by distance' }).first().click()
      await saved(page)
      expect(fx()).not.toHaveProperty('atRangeFixed')
      // a delay step that also changes where the shot stops, and a step with only an end offset
      await e1.getByRole('button', { name: 'Add a delay (effect 1)' }).click()
      await e1.getByLabel('Later when far away 1: Stops short by (pixels)').fill('12')
      await saved(page)
      expect(fx().atRange).toEqual([{ from: 2, delayMs: 200, endOffset: 12 }])
      await e1.getByLabel('Later when far away 1: Starts later by (ms)').fill('')
      await saved(page)
      expect(fx().atRange).toEqual([{ from: 2, endOffset: 12 }])  // a step needs a delay or an end offset; this one has the offset
      expect(await game.problems(id)).toEqual([])
      await e1.getByLabel('Later when far away 1: Stops short by (pixels)').fill('')
      await saved(page)
      await expect(card.getByRole('alert').filter({ hasText: 'give a delay or' })).toBeVisible()
      expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('needs a "delayMs"'))).toBe(true)
      await e1.getByLabel('Later when far away 1: Starts later by (ms)').fill('300')
      await saved(page)
      expect(await game.problems(id)).toEqual([])
      // a fade of the delayed effect that starts on time
      await e1.getByRole('button', { name: 'Add a fade (effect 1)' }).click()
      await e1.getByTestId('effect-fade-0').locator('label.toggle').filter({ hasText: 'Not moved by distance' }).first().click()
      await saved(page)
      expect(fx().fade[0]).toMatchObject({ atRangeFixed: true })
      expect(fx().atRange).toEqual([{ from: 2, delayMs: 300 }])
      expect(await game.problems(id)).toEqual([])
      if (label === 'phone') await expectNoHScroll(page)
      if (SHOTS) await e1.screenshot({ path: `${SHOTS}/editor-scene5-effect-${label}.png` })
    })
  })
}
