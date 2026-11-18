// The Stage card of the Art screen (docs/BATTLE_SCENE.md "Fixed stage", docs/EDITOR_UI.md "Art"): switching the stage on, its size, the foot
// points close and at range, the platform places, align / fit / range line, the live preview, refusals the way the game's loader says them,
// and a platform's right-hand pictures. The saved files must load in the real game loader. Desktop window and 360 px.
import { expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { clickPiece, openArt, seedArt, sheetPng } from './artkit'

const SHOTS = process.env.BATTLE_SHOTS

for (const [label, viewport] of [['desktop', { width: 1280, height: 900 }], ['phone', { width: 360, height: 740 }]] as const) {
  test.describe(label, () => {
    test.use({ viewport })
    test.skip(({ browserName }) => browserName !== 'chromium', 'Chromium only')

    test(`set up a fixed stage: size, feet, platforms, align, fit, range line, preview (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Battle Stage ${label}`)
      // one image of 4 x 2 pieces of 32 x 32 (the backdrop and the platform pictures come from it)
      await seedArt(request, id, { scene: sheetPng(4, 2, 32) }, { sheets: [{ id: 'scene', file: 'art/scene.png', cellWidth: 32, cellHeight: 32, name: 'Scene' }] })
      await openArt(page, id)
      await page.getByLabel('Name of a new background').fill('arena')
      await page.getByRole('button', { name: 'Add background' }).click()
      await page.getByLabel('Default background').selectOption('arena')
      await saved(page)
      const card = page.getByTestId('battle-stage')
      // (a switch is clicked by its label: the track over the input would catch a click on the input itself)
      const flip = async (name: string, on: boolean) => {
        const sw = card.getByRole('switch', { name, exact: true })
        if ((await sw.isChecked()) !== on) await card.locator('label.toggle').filter({ hasText: name }).first().click()
        await expect(sw).toBeChecked({ checked: on })
      }
      await card.scrollIntoViewIfNeeded()
      await expect(card.getByTestId('stage-preview')).toHaveCount(0)
      // off by default: nothing in the file; on: the size of the default backdrop and two fighters in the lower middle
      await flip('Use a fixed stage', true)
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.stage).toEqual({ width: 32, height: 32, feet: { left: [12, 21], right: [20, 21] } })
      await expect(card.getByTestId('stage-backdrop-note')).toContainText('the same as the stage')
      await expect(card.getByTestId('stage-preview')).toBeVisible()
      // the size of a handheld stage and the feet in it
      await card.getByLabel('Stage width').fill('240')
      await card.getByLabel('Stage height').fill('160')
      await expect(card.getByTestId('stage-backdrop-note')).toContainText('not the stage’s size')
      await card.getByLabel('Where the fighters stand, left fighter: across').fill('88')
      await card.getByLabel('Where the fighters stand, left fighter: down').fill('104')
      await card.getByLabel('Where the fighters stand, right fighter: across').fill('152')
      await card.getByLabel('Where the fighters stand, right fighter: down').fill('104')
      await flip('Different at range', true)
      await card.getByLabel('Where they stand at range, left fighter: across').fill('56')
      await card.getByLabel('Where they stand at range, right fighter: across').fill('184')
      await flip('Place the platforms', true)
      await flip('Where the platforms are at range', true)
      await card.getByLabel('Where the platforms are, left fighter: across').fill('56')
      await card.getByLabel('Where the platforms are, left fighter: down').fill('96')
      await card.getByLabel('Where the platforms are, right fighter: across').fill('120')
      await card.getByLabel('Where the platforms are, right fighter: down').fill('96')
      await card.getByLabel('Keep visible').selectOption('bottom')
      await card.getByLabel('When the stage is too tall').selectOption('contain')
      await flip('Show the dotted range line', false)
      await saved(page)
      const stage = readDisk(id, 'data/graphics.json').battle.stage
      expect(stage).toMatchObject({
        width: 240, height: 160, feet: { left: [88, 104], right: [152, 104] }, feetFar: { left: [56, 104], right: [184, 104] },
        platform: { left: [56, 96], right: [120, 96] }, align: 'bottom', fit: 'contain', rangeLine: false,
      })
      expect(stage.platformFar).toBeDefined()
      expect(await game.problems(id)).toEqual([])  // the real loader accepts what the editor wrote
      // the preview: both fighters and the platforms are drawn at the chosen places; "At range" shows the far ones
      await expect(card.getByTestId('stage-preview-left')).toBeVisible()
      await expect(card.getByTestId('stage-preview-right')).toBeVisible()
      const footX = async (side: string) => Number(await card.getByTestId(`stage-preview-${side}`).locator('rect').last().getAttribute('x'))
      expect([await footX('left'), await footX('right')]).toEqual([87, 151])  // the 2 x 2 foot mark starts one pixel before the foot point
      await card.getByRole('radio', { name: 'At range' }).click()
      expect([await footX('left'), await footX('right')]).toEqual([55, 183])
      if (SHOTS) await card.screenshot({ path: `${SHOTS}/editor-stage-card-${label}.png` })
      // refusals, in the loader's words: a foot point outside the stage, a size that is too small
      await card.getByLabel('Where the fighters stand, right fighter: across').fill('240')
      await saved(page)
      await expect(card.getByRole('alert').filter({ hasText: 'outside the stage' })).toBeVisible()
      expect((await game.problems(id)).some((p: { message: string }) => p.message.includes('is outside the stage'))).toBe(true)
      await card.getByLabel('Where the fighters stand, right fighter: across').fill('152')
      await saved(page)
      await expect(card.getByRole('alert').filter({ hasText: 'outside the stage' })).toHaveCount(0)
      expect(await game.problems(id)).toEqual([])
      // defaults are no keys: back to the middle, cover and the line on
      await card.getByLabel('Keep visible').selectOption('center')
      await card.getByLabel('When the stage is too tall').selectOption('cover')
      await flip('Show the dotted range line', true)
      await saved(page)
      const back = readDisk(id, 'data/graphics.json').battle.stage
      expect('align' in back || 'fit' in back || 'rangeLine' in back).toBe(false)
      // switching the platforms' places off removes the far ones too; switching the stage off removes the key
      await flip('Place the platforms', false)
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.stage).not.toHaveProperty('platformFar')
      await flip('Use a fixed stage', false)
      await saved(page)
      expect('stage' in (readDisk(id, 'data/graphics.json').battle ?? {})).toBe(false)
      await expectNoHScroll(page)
    })

    test(`a platform can have its own right-hand pictures (${label})`, async ({ page, request, game }) => {
      const id = await game.create('strategy-rpg', `Battle Platform Right ${label}`)
      await seedArt(request, id, { scene: sheetPng(4, 2, 32) }, { sheets: [{ id: 'scene', file: 'art/scene.png', cellWidth: 32, cellHeight: 32, name: 'Scene' }] })
      await openArt(page, id)
      await page.getByLabel('Name of a new platform').fill('slab')
      await page.getByRole('button', { name: 'Add platform' }).click()
      const card = page.getByTestId('battle-platforms')
      const choose = async (button: string, col: number, row: number) => {
        await card.getByRole('button', { name: button }).click()
        const dialog = page.getByRole('dialog', { name: /^Picture for/ })
        await clickPiece(page, dialog.getByTestId('piece-grid'), col, row)
        await dialog.getByRole('button', { name: 'Use this picture' }).click()
      }
      await choose('Add a far picture', 1, 0)
      await choose('Add a right-hand picture', 2, 0)
      await choose('Add a right-hand far picture', 3, 0)
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.platforms.slab).toEqual({ sheet: 'scene', cell: [0, 0], far: [1, 0], right: [2, 0], farRight: [3, 0] })
      expect(await game.problems(id)).toEqual([])
      await card.getByRole('button', { name: 'Remove the right-hand picture of slab' }).click()
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.platforms.slab).toEqual({ sheet: 'scene', cell: [0, 0], far: [1, 0], farRight: [3, 0] })
      await card.getByRole('button', { name: 'Remove the far picture of slab' }).click()
      await saved(page)
      expect(readDisk(id, 'data/graphics.json').battle.platforms.slab).toEqual({ sheet: 'scene', cell: [0, 0] })  // the far twin goes with it
      await expectNoHScroll(page)
    })
  })
}
