// "Fidelity 3" through the editor's own forms (docs/EDITOR_UI.md): an ability that uses up its item, summon candidates with a ceiling, a weight and "no
// item", a promotion level per character, weather that closes a way, several preparation shops with music, the new sound slots and the promotion
// sounds. The proof that matters: the exact JSON the forms wrote and that the real loader accepts it (/problems is empty), on a desktop window
// and a 360 px phone.
import { cpSync, rmSync, readdirSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import { diskPath, editorUrl, expect, expectNoHScroll, readDisk, saved, test, writeDisk } from './fixtures'
import { openMap, tab } from './mapkit'
import { shot } from './storykit'

test.use({ actionTimeout: 10_000 })

const repoFixture = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-prep')
const json = (o: unknown) => JSON.stringify(o, null, 1)
const read = (id: string, rel: string) => readDisk(id, rel)

/** The preparation fixture plus the files these forms edit. */
async function fid3Game(game: { create: (t: string, n?: string) => Promise<string> }): Promise<string> {
  const id = await game.create('blank', 'Fidelity 3')
  for (const dir of ['maps', 'data']) for (const f of readdirSync(diskPath(id, dir))) rmSync(diskPath(id, `${dir}/${f}`), { recursive: true, force: true })
  cpSync(repoFixture, diskPath(id, '.'), { recursive: true })
  rmSync(diskPath(id, 'README.md'), { force: true })
  const g = read(id, 'game.json')
  g.rules.progression = { enabled: true, xpPerLevel: 100, maxLevel: 20 }
  writeDisk(id, 'game.json', json(g))
  const t = read(id, 'data/terrain.json')
  t.terrain.push({ id: 'bog', name: 'Bog', moveCost: { foot: 2 } })
  writeDisk(id, 'data/terrain.json', json(t))
  const items = read(id, 'data/items.json')
  items.items.push({ id: 'wand', name: 'Wand', kind: 'accessory', uses: 3, skills: ['zapper'] })
  writeDisk(id, 'data/items.json', json(items))
  writeDisk(id, 'data/skills.json', json({ skills: [{ id: 'zapper', name: 'Zapper', blocks: [{ when: 'always', do: [{ ability: 'zap' }] }] }] }))
  writeDisk(id, 'data/abilities.json', json({ abilities: [
    { id: 'zap', name: 'Zap', target: 'ally', effect: { heal: 5 } },
    { id: 'call', name: 'Call', target: 'cell', range: [1, 2], effect: { summon: { type: 'raider', limit: 2, items: [{ item: 'potion' }, { item: 'tonic', minLevel: 3 }], pick: 'random' } } }] }))
  const u = read(id, 'data/units.json')
  for (const unit of u.units) if (unit.id === 'soldier') { unit.items = ['wand']; unit.promotions = [{ to: 'glider', fromLevel: 5 }]; unit.abilities = ['call'] }
  writeDisk(id, 'data/units.json', json(u))
  writeDisk(id, 'data/characters.json', json({ characters: [{ id: 'hero1', name: 'Hero', type: 'soldier' }] }))
  writeDisk(id, 'data/audio.json', json({
    music: { camp: { file: 'audio/camp.ogg' }, boot: { file: 'audio/boot.ogg', loop: false } },
    sounds: { tick: { file: 'audio/tick.wav' }, boom: { file: 'audio/boom.wav' } } }))
  return id
}

const ability = (id: string, a: string) => read(id, 'data/abilities.json').abilities.find((x: any) => x.id === a)

async function run(page: Page, game: { problems: (id: string) => Promise<Array<{ file: string; message: string }>> }, id: string, phone: boolean): Promise<void> {
  const tag = phone ? 'phone' : 'desktop'
  expect(await game.problems(id)).toEqual([])

  // ===== An ability that uses up its item =====
  await page.goto(editorUrl(id, 'abilities', 'zap'))
  const spends = page.getByRole('checkbox', { name: /Uses up its item/ })
  await expect(spends).not.toBeChecked()
  await spends.check()
  await saved(page)
  expect(ability(id, 'zap').spendsItem).toBe(true)
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-ability-spends-item`)
  // (a wand with no uses would be refused: the form says so first)
  const items = read(id, 'data/items.json')
  items.items.find((i: any) => i.id === 'wand').uses = undefined
  writeDisk(id, 'data/items.json', json(items))
  await page.goto(editorUrl(id, 'abilities', 'zap'))
  await expect(page.getByTestId('ability-form').getByText(/gives it but has unlimited uses/)).toBeVisible()
  items.items.find((i: any) => i.id === 'wand').uses = 3
  writeDisk(id, 'data/items.json', json(items))

  // ===== Summon candidates: ceiling, weight, no item =====
  await page.goto(editorUrl(id, 'abilities', 'call'))
  const cands = page.getByTestId('summon-items')
  await cands.getByLabel('Candidate 1: highest summoner’s level', { exact: true }).fill('4')
  await cands.getByLabel('Candidate 1: weight', { exact: true }).fill('3')
  await cands.getByLabel('Candidate 2: item', { exact: true }).selectOption({ label: 'No item' })
  await saved(page)
  expect(ability(id, 'call').effect.summon.items).toEqual([{ item: 'potion', maxLevel: 4, weight: 3 }, { item: null, minLevel: 3 }])
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-summon-candidates`)

  // ===== A promotion level per character =====
  await page.goto(editorUrl(id, 'characters', 'hero1'))
  const own = page.getByTestId('character-promotion').getByLabel('May promote from level', { exact: true })
  await own.fill('2')
  await saved(page)
  expect(read(id, 'data/characters.json').characters[0].promotion).toEqual({ fromLevel: 2 })
  await page.goto(editorUrl(id, 'units', 'soldier'))
  const forWho = page.getByTestId('promotion-levelfor-1')
  await forWho.getByRole('button', { name: 'Add a character' }).click()
  await forWho.getByLabel('Promotion 1: from level', { exact: true }).fill('1')
  await saved(page)
  expect(read(id, 'data/units.json').units.find((x: any) => x.id === 'soldier').promotions[0].fromLevelFor).toEqual({ hero1: 1 })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-promotion-level`)

  // ===== Weather that closes a way =====
  await page.goto(editorUrl(id, 'terrain', 'bog'))
  await page.getByRole('checkbox', { name: 'Closed' }).first().check()
  await saved(page)
  const wc = read(id, 'data/terrain.json').terrain.find((x: any) => x.id === 'bog').weatherCost
  expect(Object.values(wc.rain)).toEqual(['blocked'])
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-weather-closed`)

  // ===== Several preparation shops =====
  await openMap(page, id, 'orchard')
  await tab(page, 'Preparation').click()
  await page.getByRole('button', { name: 'Use several shops' }).click()
  await page.getByRole('button', { name: 'Add a shop' }).click()
  await page.getByLabel('Name of shop 2', { exact: true }).fill('Market')
  await page.getByLabel('Add an item… (Market)', { exact: true }).selectOption({ label: 'Potion' })
  await page.getByLabel('Shop music (Shop)', { exact: true }).selectOption({ label: 'camp' })
  await saved(page)
  const prep = read(id, 'maps/orchard.json').prep
  expect(prep.shop).toBeUndefined()
  expect(prep.shops).toEqual([{ id: 'shop', name: 'Shop', sells: ['potion', 'tonic'], music: 'camp' }, { id: 'shop2', name: 'Market', sells: ['potion'] }])
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-prep-shops`)

  // ===== The new sound slots and the promotion sounds =====
  await page.goto(editorUrl(id, 'sound'))
  await page.getByLabel('Shop screens', { exact: true }).selectOption({ label: 'camp' })
  await page.getByLabel('When the game’s page opens', { exact: true }).selectOption({ label: 'boot' })
  await page.getByLabel('Level-up: each stat that rises', { exact: true }).selectOption({ label: 'tick' })
  await page.getByLabel('A save was written', { exact: true }).selectOption({ label: 'boom' })
  const promo = page.getByTestId('promotion-sounds')
  await promo.getByRole('button', { name: 'Add a sound' }).click()
  await promo.getByRole('button', { name: 'Add a sound' }).click()
  await promo.getByLabel('Promotion sound 2: time', { exact: true }).fill('750')
  await promo.getByLabel('Promotion sound 2: sound', { exact: true }).selectOption({ label: 'boom' })
  await saved(page)
  const use = read(id, 'data/audio.json').use
  expect(use).toMatchObject({ shopMusic: 'camp', bootJingle: 'boot', sounds: { levelUpTick: 'tick', save: 'boom' } })
  expect(use.promotionSounds).toEqual([{ atMs: 0, sound: 'tick' }, { atMs: 750, sound: 'boom' }])
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-sound-slots`)
}

test('@smoke fidelity 3 on the editor screens: the files are what the docs say and the game loads', async ({ page, game }) => {
  test.setTimeout(240_000)
  const id = await fid3Game(game)
  await page.goto(editorUrl(id, 'game'))
  await run(page, game, id, false)
})

test('@smoke on a 360 px phone the fidelity 3 forms fit without sideways scrolling', async ({ page, game }) => {
  test.setTimeout(240_000)
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await fid3Game(game)
  await page.goto(editorUrl(id, 'game'))
  await run(page, game, id, true)
})
