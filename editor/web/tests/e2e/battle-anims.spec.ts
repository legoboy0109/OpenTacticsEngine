// Battle animations in the editor (docs/BATTLE_SCENE.md): the default switch on the Rules screen, an animation set made on the Art screen
// (frames from a row, the hit frame, the live preview), who uses it, backgrounds and the Terrain picker. The saved files must load in the
// real game loader (the helper validates with it), on a desktop window and at 360 px.
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { openArt, seedArt, sheetPng } from './artkit'

const SHOTS = process.env.BATTLE_SHOTS

const seed = (request: import('@playwright/test').APIRequestContext, id: string) =>
  seedArt(request, id, { fighters: sheetPng(4, 3), bg: sheetPng(2, 2, 48) }, {
    sheets: [
      { id: 'fighters', file: 'art/fighters.png', cellWidth: 24, cellHeight: 24, name: 'Fighters' },
      { id: 'bg', file: 'art/bg.png', cellWidth: 48, cellHeight: 48, name: 'Backdrops' },
    ],
  })

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`the default for fights is a switch on the Rules screen (${label})`, async ({ page, game }) => {
      const id = await game.create('strategy-rpg', `Battle Default ${label}`)
      await page.goto(editorUrl(id, 'rules'))
      const card = page.getByRole('radiogroup', { name: 'How fights are shown' })
      await expect(card.getByRole('radio', { name: 'On the map' })).toHaveAttribute('aria-checked', 'true')
      await card.getByRole('radio', { name: 'Battle scene' }).click()
      await saved(page)
      expect(readDisk(id, 'game.json').battleScene).toBe('scene')
      expect(await game.problems(id)).toEqual([])
      await card.getByRole('radio', { name: 'On the map' }).click()
      await saved(page)
      expect('battleScene' in readDisk(id, 'game.json')).toBe(false)  // the default is no key at all
      await expectNoHScroll(page)
    })

    test(`make an animation set, mark the hit frame, give it to a unit type, pick backgrounds (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Battle Set ${label}`)
      await seed(request, id)
      await openArt(page, id)
      const card = page.getByTestId('battle-animations')
      await card.scrollIntoViewIfNeeded()
      // a set starts with a stand
      await page.getByLabel('Name of a new set').fill('knight')
      await page.getByRole('button', { name: 'Add set' }).click()
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.sets.knight).toEqual({ animations: { stand: { sheet: 'fighters', frames: [[0, 0]] } } })
      expect(await game.problems(id)).toEqual([])
      await expect(page.getByTestId('battle-preview')).toBeVisible()
      // the stand: three frames from the first row, a slower pace
      await page.getByLabel('Stand: Frames').fill('3')
      await page.getByLabel('Stand: Milliseconds per frame').fill('200')
      // an attack from the second row, four frames: no hit frame yet is "worth a look" and not a refusal
      await page.getByRole('button', { name: 'Add Attack' }).click()
      await page.getByLabel('Attack: Row').fill('2')
      await page.getByLabel('Attack: Frames').fill('4')
      await expect(page.getByTestId('battle-checks')).toContainText('“attack” has no hit frame marked')
      await expect(page.getByTestId('battle-checks')).toContainText('not used by any unit type')
      await page.getByLabel('Attack: Hit on frame').fill('3')
      await page.getByRole('button', { name: 'Add Hurt' }).click()
      await page.getByLabel('Hurt: Row').fill('3')
      await page.getByLabel('Hurt: Frames').fill('2')
      await page.getByLabel('Foot point: across').fill('12')
      await page.getByLabel('Foot point: down').fill('23')
      // who uses it
      await page.getByRole('button', { name: 'Add a rule' }).click()
      await page.getByLabel('Rule 1: Unit type').selectOption('lord')
      await saved(page)
      const g = readDisk(id, 'data/graphics.json')
      const set = g.battle.sets.knight
      expect(set.anchor).toEqual([12, 23])
      expect(set.animations.stand).toMatchObject({ sheet: 'fighters', frames: [[0, 0], [1, 0], [2, 0]], ms: 200 })
      expect(set.animations.attack).toMatchObject({ sheet: 'fighters', frames: [[0, 1], [1, 1], [2, 1], [3, 1]], hit: 2 })
      expect(set.animations.hurt.frames).toEqual([[0, 2], [1, 2]])
      expect(g.battle.assign).toHaveLength(1)
      expect(g.battle.assign[0]).toMatchObject({ set: 'knight', type: 'lord' })
      await expect(page.getByTestId('battle-checks')).toHaveCount(0)
      expect(await game.problems(id)).toEqual([])          // the real loader accepts what the editor wrote
      // the live preview plays; the fight preview shows both fighters (a canvas that changes over time)
      const canvas = page.getByTestId('battle-preview')
      const snap = () => canvas.evaluate((el) => (el as HTMLCanvasElement).toDataURL())
      await page.getByRole('button', { name: 'Preview a fight' }).click()
      await expect(page.getByRole('button', { name: 'Back to one animation' })).toHaveAttribute('aria-pressed', 'true')
      const seen = new Set<string>()
      for (let i = 0; i < 8; i++) { seen.add(await snap()); await page.waitForTimeout(400) }
      expect(seen.size).toBeGreaterThan(1)   // it moves
      if (SHOTS) await page.screenshot({ path: `${SHOTS}/editor-battle-set-${label}.png`, fullPage: true })
      await page.getByRole('button', { name: 'Back to one animation' }).click()
      // backgrounds: one, the default, and for a terrain on the Terrain screen
      await page.getByLabel('Name of a new background').fill('meadow')
      await page.getByRole('button', { name: 'Add background' }).click()
      await page.getByLabel('Default background').selectOption('meadow')
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle).toMatchObject({ backgrounds: { meadow: { sheet: 'fighters', cell: [0, 0] } }, defaultBackground: 'meadow' })
      await expectNoHScroll(page)
      await page.goto(editorUrl(id, 'terrain', 'plain'))
      await page.getByLabel('Battle background').selectOption('meadow')
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.terrainBackground).toEqual({ plain: 'meadow' })
      expect(await game.problems(id)).toEqual([])
      if (SHOTS) await page.screenshot({ path: `${SHOTS}/editor-battle-terrain-${label}.png`, fullPage: true })
    })

    test(`deleting what a set is for cleans the rules; deleting the set asks first (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Battle Clean ${label}`)
      await seed(request, id)
      await openArt(page, id)
      await page.getByLabel('Name of a new set').fill('knight')
      await page.getByRole('button', { name: 'Add set' }).click()
      await page.getByRole('button', { name: 'Add a rule' }).click()
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.assign).toHaveLength(1)
      await page.getByRole('button', { name: 'Delete set' }).click()
      await expect(page.getByRole('dialog')).toContainText('rule that give it')
      await page.getByRole('button', { name: 'Delete set' }).last().click()
      await saved(page)
      expect('battle' in readDisk(id, 'data/graphics.json')).toBe(false)
      // removing the image takes its sets along
      await page.getByLabel('Name of a new set').fill('again')
      await page.getByRole('button', { name: 'Add set' }).click()
      await saved(page)
      expect(Object.keys(readDisk(id, 'data/graphics.json').battle.sets)).toEqual(['again'])
      expect(await game.problems(id)).toEqual([])
    })
  })
}
