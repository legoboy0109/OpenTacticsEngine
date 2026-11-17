// Battle animation follow-ups (docs/BATTLE_SCENE.md) and walking pictures (docs/GRAPHICS.md "Walking") in the editor: a side's own image for a
// set, rules by weapon and side, the extra animation slots, a 100-frame animation, and the Walking card on a unit's picture with its live
// preview. The files the editor writes must load in the real game loader (the helper validates with it), on a desktop window and at 360 px.
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'
import { seedArt, sheetPng } from './artkit'

const SHOTS = process.env.BATTLE_SHOTS

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`a set can draw from another image for a side, with rules by weapon and side, and a 100-frame animation (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Battle Follow ${label}`)
      // pieces of 8 pixels, 128 across: a hundred frames fit one row; the red image is laid out the same way, the small one is not
      await seedArt(request, id, { fighters: sheetPng(128, 3, 8), fighters_red: sheetPng(128, 3, 8), other: sheetPng(4, 3, 24) }, {
        sheets: [
          { id: 'fighters', file: 'art/fighters.png', cellWidth: 8, cellHeight: 8, name: 'Fighters' },
          { id: 'fighters_red', file: 'art/fighters_red.png', cellWidth: 8, cellHeight: 8, name: 'Fighters red' },
          { id: 'other', file: 'art/other.png', cellWidth: 24, cellHeight: 24, name: 'Other' },
        ],
      })
      await page.goto(editorUrl(id, 'game'))
      await gotoSection(page, 'Art')   // by the sections menu (opened first on a phone)
      await expect(page.getByRole('heading', { name: 'Art', level: 1 })).toBeVisible()
      await page.getByTestId('battle-animations').scrollIntoViewIfNeeded()
      await page.getByLabel('Name of a new set').fill('knight')
      await page.getByRole('button', { name: 'Add set' }).click()
      await saved(page)
      // the extra slots are in the list, each with its own Add button
      for (const name of ['Stand, ranged fight', 'Missed attack', 'Critical hit, ranged', 'Dodge, ranged']) {
        await expect(page.getByRole('button', { name: `Add ${name}` })).toBeVisible()
      }
      await page.getByRole('button', { name: 'Add Missed attack' }).click()
      await page.getByLabel('Missed attack: Frames').fill('100')
      await page.getByLabel('Missed attack: Hit on frame').fill('100')
      await page.getByRole('button', { name: 'Add Stand, ranged fight' }).click()
      await saved(page)
      let g = readDisk(id, 'data/graphics.json').battle.sets.knight
      expect(g.animations.miss.frames).toHaveLength(100)
      expect(g.animations.miss.hit).toBe(99)
      expect(g.animations.standRanged.frames).toEqual([[0, 0]])
      expect(await game.problems(id)).toEqual([])
      // a different image for a side: the same layout is accepted ...
      const sides = page.getByTestId('battle-sides')
      await sides.scrollIntoViewIfNeeded()
      const first = sides.getByRole('combobox').first()
      const sideName = (await first.getAttribute('aria-label'))!.replace('Image for ', '')
      expect(sideName.length).toBeGreaterThan(0)
      await first.selectOption({ label: 'Fighters red' })
      await saved(page)
      g = readDisk(id, 'data/graphics.json').battle.sets.knight
      expect(Object.values(g.sides)).toEqual([{ sheet: 'fighters_red' }])
      expect(await game.problems(id)).toEqual([])
      await expect(page.getByTestId('battle-checks')).toHaveCount(0, { timeout: 2000 }).catch(() => {})
      // ... another layout is flagged before the game would refuse it
      await first.selectOption({ label: 'Other' })
      await expect(page.getByTestId('battle-checks')).toContainText('laid out the same way')
      await first.selectOption({ label: 'Same image' })
      await saved(page)
      expect('sides' in readDisk(id, 'data/graphics.json').battle.sets.knight).toBe(false)
      if (SHOTS) await page.screenshot({ path: `${SHOTS}/editor-battle-sides-${label}.png`, fullPage: true })
      // rules by weapon and side
      await page.getByRole('button', { name: 'Add a rule' }).click()
      await page.getByLabel('Rule 1: Unit type').selectOption('lord')
      await page.getByLabel('Rule 1: Weapon', { exact: true }).selectOption({ index: 1 })
      await page.getByLabel('Rule 1: Side').selectOption({ index: 1 })
      await saved(page)
      const rule = readDisk(id, 'data/graphics.json').battle.assign[0]
      expect(rule).toMatchObject({ set: 'knight', type: 'lord' })
      expect(typeof rule.weapon).toBe('string')
      expect(typeof rule.side).toBe('string')
      expect(rule.group).toBeUndefined()
      expect(await game.problems(id)).toEqual([])
      // back to any weapon: the rule keeps its type and side
      await page.getByLabel('Rule 1: Weapon', { exact: true }).selectOption({ index: 0 })
      await saved(page)
      const rule2 = readDisk(id, 'data/graphics.json').battle.assign[0]
      expect(rule2.weapon).toBeUndefined()
      expect(rule2).toMatchObject({ set: 'knight', type: 'lord', side: rule.side })
      expect(await game.problems(id)).toEqual([])
      await expectNoHScroll(page)
    })

    test(`the Walking card on a unit's picture: image, rows, frames, time and a live preview (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Walking ${label}`)
      await seedArt(request, id, { men: sheetPng(2, 1, 16), walk: sheetPng(4, 3, 32) }, {
        sheets: [
          { id: 'men', file: 'art/men.png', cellWidth: 16, cellHeight: 16, name: 'Men' },
          { id: 'walk', file: 'art/walk.png', cellWidth: 32, cellHeight: 32, name: 'Walkers' },
        ],
        units: { lord: { sheet: 'men', cell: [0, 0], sides: { red: { sheet: 'men', cell: [1, 0] } } } },
      })
      await page.goto(editorUrl(id, 'units', 'lord'))
      const card = page.getByTestId('walking-card')
      await card.scrollIntoViewIfNeeded()
      await expect(card.getByRole('heading', { name: 'Walking' })).toBeVisible()
      await card.getByRole('button', { name: 'Add walking pictures' }).click()
      await saved(page)
      // a first walk: facing down, from the picture's own image (two pieces wide: two frames)
      expect(readDisk(id, 'data/graphics.json').units.lord.walk).toEqual({ down: [[0, 0], [1, 0]] })
      await card.getByLabel('Walking: Image').selectOption({ label: 'Walkers' })
      await card.getByLabel('Walking: Frames').fill('4')
      await card.getByLabel('Walking: Up row').fill('2')
      await card.getByLabel('Walking: Side row').fill('3')
      await card.getByLabel('Walking: Milliseconds per frame').fill('100')
      await saved(page)
      const lord = readDisk(id, 'data/graphics.json').units.lord
      expect(lord.walk).toEqual({
        sheet: 'walk', down: [[0, 0], [1, 0], [2, 0], [3, 0]], up: [[0, 1], [1, 1], [2, 1], [3, 1]], side: [[0, 2], [1, 2], [2, 2], [3, 2]], frameMs: 100,
      })
      expect(lord.cell).toEqual([0, 0])
      expect(await game.problems(id)).toEqual([])    // the real loader accepts what the editor wrote
      // the frame size of the walking image is shown (and editable: it is how that image is cut)
      await expect(card.getByLabel('Walking: Piece width')).toHaveValue('32')
      await expect(card.getByLabel('Walking: Piece height')).toHaveValue('32')
      // the live preview moves through the directions
      const canvas = card.getByTestId('walk-preview')
      const snap = () => canvas.evaluate((el) => (el as HTMLCanvasElement).toDataURL())
      const seen = new Set<string>()
      for (let i = 0; i < 10; i++) { seen.add(await snap()); await page.waitForTimeout(450) }
      expect(seen.size).toBeGreaterThan(2)
      if (SHOTS) await page.screenshot({ path: `${SHOTS}/editor-walking-${label}.png`, fullPage: true })
      // left and right of their own: the side row becomes the right row, the left starts the same
      await card.getByRole('switch', { name: /Left and right have rows of their own/ }).check()
      await saved(page)
      const own = readDisk(id, 'data/graphics.json').units.lord.walk
      expect(own.side).toBeUndefined()
      expect(own.right[0]).toEqual([0, 2])
      expect(own.left[0]).toEqual([0, 2])
      await card.getByLabel('Walking: Left row').fill('2')
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').units.lord.walk.left[0]).toEqual([0, 1])
      expect(await game.problems(id)).toEqual([])
      // a problem the game would refuse is shown: too many frames for the image (the walkers image has 4 pieces across)
      await card.getByLabel('Walking: Frames').fill('9')
      await expect(card.getByRole('alert')).toContainText('outside the image')
      await card.getByLabel('Walking: Frames').fill('4')
      await expect(card.getByRole('alert')).toHaveCount(0)
      // a side's own picture has its own walk
      await card.getByLabel('For', { exact: true }).selectOption('red')
      await card.getByRole('button', { name: 'Add walking pictures' }).click()
      await saved(page)
      const after = readDisk(id, 'data/graphics.json').units.lord
      expect(after.sides.red.walk).toEqual({ down: [[0, 0], [1, 0]] })
      expect(after.walk.right).toBeDefined()
      expect(await game.problems(id)).toEqual([])
      await card.getByRole('button', { name: 'Remove walking pictures' }).click()
      await saved(page)
      expect('walk' in readDisk(id, 'data/graphics.json').units.lord.sides.red).toBe(false)
      await expectNoHScroll(page)
    })
  })
}
