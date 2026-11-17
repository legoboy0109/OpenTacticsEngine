import { FORMULA_DEFS, type FormulaKey } from '../../src/model/presetDefs'
import { gotoSection, editorUrl, expect, readDisk, saved, test } from './fixtures'

const LABEL: Record<FormulaKey, string> = {
  damage: 'Damage', hit: 'Chance to hit', crit: 'Critical hits', strikes: 'Attacks per fight',
  xpHit: 'Experience for a hit', xpKill: 'Experience for a defeat', capture: 'Capture speed',
}

test('@smoke switching the damage preset writes the expression and the engine accepts it', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Damage')
  await page.goto(editorUrl(id, 'rules'))
  const damage = page.getByLabel('Damage: recipe')
  await expect(damage).toHaveValue('attack-minus-defense') // recognised from the template's own text
  await damage.selectOption({ label: 'Damage chart' })
  await saved(page)
  expect(readDisk(id, 'game.json').formulas.damage).toBe('vs * (a.hp / a.maxhp) * (1 - 0.1 * (tb.defense or 0))')
  expect(readDisk(id, 'game.json').editor.formulas.damage.preset).toBe('damage-chart')
  expect(await game.problems(id)).toEqual([])
  await damage.selectOption({ label: 'Fixed amount' })
  await saved(page)
  expect(readDisk(id, 'game.json').formulas.damage).toBe('5')
  expect(await game.problems(id)).toEqual([])
})

test('every preset of every formula is accepted by the real engine', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await game.create('strategy-rpg', 'AllPresets')
  await page.goto(editorUrl(id, 'rules'))
  await page.getByRole('switch', { name: 'Capturing buildings' }).check()
  await saved(page)
  for (const key of Object.keys(LABEL) as FormulaKey[]) {
    const select = page.getByLabel(`${LABEL[key]}: recipe`)
    for (const p of [...FORMULA_DEFS[key].presets.map((x) => x.name), 'Custom']) {
      await select.selectOption({ label: p })
      await saved(page)
      expect(await game.problems(id), `${key} / ${p}`).toEqual([])
    }
  }
})

test('@smoke turning Experience off and on keeps its settings', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Xp')
  await page.goto(editorUrl(id, 'rules'))
  const before = readDisk(id, 'game.json').rules.progression
  const sw = page.getByRole('switch', { name: 'Experience and levels' })
  await page.getByLabel('Experience per level').fill('250')
  await sw.uncheck()
  await saved(page)
  const off = readDisk(id, 'game.json').rules.progression
  expect(off).toMatchObject({ ...before, xpPerLevel: 250, enabled: false })
  await expect(page.getByLabel('Experience per level')).toHaveCount(0)
  await sw.check()
  await saved(page)
  expect(readDisk(id, 'game.json').rules.progression).toMatchObject({ ...before, xpPerLevel: 250, enabled: true })
  await expect(page.getByLabel('Experience per level')).toHaveValue('250')
})

test('add a stat, a resource and a movement type', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Lists')
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('Add a stat').fill('Luck')
  await page.getByLabel('Add a stat').press('Enter')
  await page.getByLabel('Add a resource').fill('Gold')
  await page.getByRole('button', { name: 'Add resource' }).click()
  await page.getByLabel('Add a movement type').fill('Boat')
  await page.getByRole('button', { name: 'Add movement type' }).click()
  await saved(page)
  const g = readDisk(id, 'game.json')
  expect(g.stats.at(-1)).toEqual({ id: 'luck', name: 'Luck' })
  expect(g.resources).toEqual([{ id: 'gold', name: 'Gold' }])
  expect(g.moveTypes.at(-1)).toEqual({ id: 'boat', name: 'Boat' })
  expect(await game.problems(id)).toEqual([])
  // The new stat shows up on every unit.
  await gotoSection(page, 'Units')
  await expect(page.getByLabel('Luck Value')).toBeVisible()
})

test('deleting a stat in use asks, cleans units, and flags the combat maths that used it', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'DelStat')
  await page.goto(editorUrl(id, 'rules'))
  // Health and Movement cannot go.
  await page.getByRole('button', { name: 'Delete HP' }).click()
  await expect(page.getByRole('dialog')).toContainText('used as Health')
  await page.getByRole('dialog').getByRole('button', { name: 'OK' }).click()

  await page.getByRole('button', { name: 'Delete Skill' }).click()
  const dialog = page.getByRole('dialog')
  await expect(dialog).toContainText('Lord')
  await expect(dialog).toContainText('Chance to hit')
  await dialog.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  const units = readDisk(id, 'data/units.json').units
  expect(units.every((u: any) => !('skl' in u.stats) && !('skl' in (u.growths ?? {})))).toBe(true)
  expect(readDisk(id, 'game.json').stats.some((s: any) => s.id === 'skl')).toBe(false)
  await expect(page.getByText('Some combat maths needs your attention')).toBeVisible()
  expect(await game.problems(id)).toEqual([])
  // Picking another stat clears the warning.
  await page.getByLabel('Skill stat').selectOption({ label: 'Speed' })
  await page.locator('[data-formula=crit]').getByLabel('Stat', { exact: true }).selectOption({ label: 'Skill'.replace('Skill', 'Speed') })
  await saved(page)
  await expect(page.getByText('Some combat maths needs your attention')).toHaveCount(0)
})
