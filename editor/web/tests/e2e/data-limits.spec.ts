// The number boxes follow the loader's ranges (engine/core/data/loader*.cpp, docs/GAME_FORMAT.md "Strict files"): a value the game would
// refuse to load is refused in the box with a message and never reaches the file. The edge itself is accepted and saved.
import type { Locator } from '@playwright/test'
import { readFileSync } from 'node:fs'
import { diskPath, editorUrl, expect, gotoSection, readDisk, saved, test, writeDisk } from './fixtures'
import { emptySkills, newSkill, openSkills, rule } from './skillskit'

/** A data file read with its leading comment lines taken out (the template files start with one). */
const commentFree = (id: string, rel: string): any => JSON.parse(readFileSync(diskPath(id, rel), 'utf8').replace(/^\s*\/\/.*$/gm, ''))

/** Types `over` (refused: the message shows), then `edgeValue` (accepted: no message). */
async function edge(box: Locator, over: string, message: string, edgeValue: string): Promise<void> {
  await box.fill(over)
  await expect(box.locator('xpath=..').getByRole('alert')).toHaveText(message)
  await box.fill(edgeValue)
  await expect(box.locator('xpath=..').getByRole('alert')).toHaveCount(0)
}

test('a skill rule chance is a percent from 0 to 100', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Chance limits', { srpgContent: true })
  await emptySkills(page, id)
  await openSkills(page, id)
  await newSkill(page, 'Lucky')
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('turnStart')
  await r1.getByLabel('Chance', { exact: true }).selectOption('percent')
  const box = r1.getByLabel('Percent', { exact: true })
  await edge(box, '101', 'At most 100.', '100')
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills[0].blocks[0].chance).toBe(100)
  await edge(box, '-1', 'At least 0.', '0')
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills[0].blocks[0].chance).toBe(0)
})

test('stat numbers stop at 99999, move costs at 9999, the crit multiplier at 1000', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Number limits', { srpgContent: true })

  // A unit type's stat.
  await page.goto(editorUrl(id, 'units', 'soldier'))
  const stat = page.getByLabel('Strength Value').first()
  await edge(stat, '100000', 'At most 99999.', '99999')
  await edge(page.getByLabel('Strength Growth %').first(), '100000', 'At most 99999.', '99999')
  await saved(page)

  // A character's personal bonus: both ways.
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  const personal = page.locator('.md-detail').getByLabel('Strength personal bonus')
  await edge(personal, '100000', 'At most 99999.', '99999')
  await edge(personal, '-100000', 'At least -99999.', '-99999')
  await saved(page)
  expect(readDisk(id, 'data/characters.json').characters.find((c: any) => c.id === 'eirik').stats.str).toBe(-99999)

  // A move cost.
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  const cost = page.getByLabel('Foot cost').first()
  await edge(cost, '10000', 'At most 9999.', '9999')
  await saved(page)
  expect(readDisk(id, 'data/terrain.json').terrain.find((t: any) => t.id === 'forest').moveCost.foot).toBe(9999)

  // The critical multiplier (Rules > combat maths).
  await gotoSection(page, 'Rules')
  const mult = page.locator('[data-formula=crit]').getByLabel('Critical hits multiply damage by')
  await edge(mult, '1001', 'At most 1000.', '1000')
  await saved(page)
  expect(readDisk(id, 'game.json').formulas.critMultiplier).toBe(1000)
  expect(await game.problems(id)).toEqual([])
})

test('a support bonus is -1000 to 1000', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Support limits', { srpgContent: true })
  await page.goto(editorUrl(id, 'supports'))
  const level = page.locator('[data-testid^=support-level-]').first()
  await level.getByLabel('Add a bonus').fill('Hit chance')
  await level.getByLabel('Add a bonus').press('Enter')
  const hit = level.getByLabel('Hit chance Amount')
  await edge(hit, '1001', 'At most 1000.', '1000')
  await edge(hit, '-1001', 'At least -1000.', '-1000')
  await saved(page)
  expect(JSON.stringify(readDisk(id, 'data/supports.json'))).toContain('"hit":-1000')
  expect(await game.problems(id)).toEqual([])
})

test('a value already in the file beyond the range is shown as it is, with the loader\'s message, until that box is edited', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Over range on load', { srpgContent: true })
  const units = commentFree(id, 'data/units.json')
  units.units.find((u: any) => u.id === 'soldier').stats.str = 100000
  writeDisk(id, 'data/units.json', JSON.stringify(units))

  await page.goto(editorUrl(id, 'units', 'soldier'))
  await expect(page.getByLabel('Strength Value').first()).toHaveValue('100000') // not clamped on load
  await page.getByRole('button', { name: /^\d+ problems?$/ }).click()
  const panel = page.getByRole('region', { name: 'Problems' })
  await expect(panel).toContainText('"stats"."str" must be a whole number from -99999 to 99999')
  await page.getByRole('button', { name: 'Close problems' }).click()

  // The loader reports one problem at a time: put the unit right, then the terrain beyond its range.
  units.units.find((u: any) => u.id === 'soldier').stats.str = 5
  writeDisk(id, 'data/units.json', JSON.stringify(units))
  const terrain = commentFree(id, 'data/terrain.json')
  terrain.terrain.find((t: any) => t.id === 'forest').moveCost.foot = 10000
  writeDisk(id, 'data/terrain.json', JSON.stringify(terrain))
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await expect(page.getByLabel('Foot cost').first()).toHaveValue('10000')
  await page.getByRole('button', { name: /^\d+ problems?$/ }).click()
  await expect(page.getByRole('region', { name: 'Problems' })).toContainText('"moveCost"."foot" must be a whole number from 0 (cannot enter) to 9999')
  await page.getByRole('button', { name: 'Close problems' }).click()
  await page.getByLabel('Foot cost').first().fill('9999') // editing the box is what brings it back into range
  await saved(page)
  expect(readDisk(id, 'data/terrain.json').terrain.find((t: any) => t.id === 'forest').moveCost.foot).toBe(9999)
})
