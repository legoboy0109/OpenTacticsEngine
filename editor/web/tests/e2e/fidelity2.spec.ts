// "Fidelity 2" through the editor's own forms (docs/EDITOR_UI.md): an ability with no effect of its own, a summon's candidate items, a damage range,
// a terrain's weather costs, a map's weather and battle tables, the game's two scene choices, platforms, and the staged fight command. The proof that
// matters: the exact JSON the forms wrote, that the real loader accepts it (/problems is empty), and the 360 px phone layout.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test, writeDisk } from './fixtures'
import { openMap, tab } from './mapkit'
import { openEvent, shot } from './storykit'

test.use({ actionTimeout: 10_000 }) // a wrong label fails in seconds, not minutes

const json = (o: unknown) => JSON.stringify(o, null, 1)
const png = (w: number, h: number): Buffer => Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]),
  Buffer.from([w >> 24, (w >> 16) & 255, (w >> 8) & 255, w & 255, h >> 24, (h >> 16) & 255, (h >> 8) & 255, h & 255, 8, 6]), Buffer.alloc(8)])

/** A game of its own: a conditions game with an inflict-only ability, a summon with candidate items, a hurt range, a swamp with weather costs,
 *  two backgrounds and platforms, and a map with a weather, tables and a staged fight at the start. */
async function fid2Game(game: { create: (t: string, n?: string) => Promise<string> }): Promise<string> {
  const id = await game.create('blank', 'Fidelity 2')
  const w = (rel: string, text: string | Buffer) => writeDisk(id, rel, text as string)
  const mk = (rel: string) => { void rel }
  mk('')
  const g = readDisk(id, 'game.json')
  const base = {
    ...g, stats: [{ id: 'hp', name: 'HP' }, { id: 'atk', name: 'Attack' }, { id: 'def', name: 'Defense' }, { id: 'mov', name: 'Move' }],
    roles: { hp: 'hp', move: 'mov' }, moveTypes: [{ id: 'foot', name: 'Foot' }],
    rules: { defeat: ['noUnits'], counterAttacks: true, items: { weapons: 'carried', inventorySize: 4 }, progression: { enabled: true, xpPerLevel: 100, maxLevel: 20, xpHit: '30', xpKill: '60' } },
    formulas: { damage: 'a.atk - b.def', hit: '90' }, battleScene: 'scene', battleSceneSides: 'playerRight', maps: [{ id: 'arena', name: 'Arena', file: 'maps/arena.json' }],
  }
  delete (base as any).styles
  w('game.json', json(base))
  w('data/terrain.json', json({ terrain: [{ id: 'plain', name: 'Plain' }, { id: 'swamp', name: 'Swamp', moveCost: { foot: 2 }, weatherCost: { rain: { foot: 4 }, snow: { foot: 1 } } }] }))
  w('data/weapons.json', json({ weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }] }))
  w('data/items.json', json({ items: [{ id: 'iron_sword', name: 'Iron Sword', kind: 'weapon', weapon: 'sword', uses: 3 }, { id: 'fang', name: 'Fang', kind: 'weapon', weapon: 'sword' }] }))
  w('data/conditions.json', json({ conditions: [
    { id: 'boost', name: 'Boost', turns: 4, blocks: [{ when: 'always', do: [{ stats: { atk: 2 } }] }] },
    { id: 'poison', name: 'Poison', turns: 5, blocks: [{ when: 'turnStart', do: [{ hurt: [1, 3], to: 'self' }] }] }] }))
  w('data/abilities.json', json({ abilities: [
    { id: 'rally', name: 'Rally', target: 'ally', inflict: { condition: 'boost', turns: 4 }, xp: 20 },
    { id: 'call_pet', name: 'Call Pet', target: 'cell', range: [1, 2], effect: { summon: { type: 'hound', limit: 2, replace: true, items: [{ item: 'fang', minLevel: 3 }, { item: 'iron_sword' }], pick: 'random' } } }] }))
  w('data/units.json', json({ units: [
    { id: 'knight', name: 'Knight', moveType: 'foot', stats: { hp: 30, atk: 8, def: 2, mov: 4 }, wields: ['blade'], items: ['iron_sword'], abilities: ['rally', 'call_pet'] },
    { id: 'hound', name: 'Hound', moveType: 'foot', stats: { hp: 8, atk: 3, def: 1, mov: 4 }, wields: ['blade'], items: ['fang'] }] }))
  w('art/bgs.png', png(240, 68))
  w('art/plat.png', png(96, 16))
  w('data/graphics.json', json({
    sheets: [{ id: 'bgs', file: 'art/bgs.png', cellWidth: 120, cellHeight: 68 }, { id: 'plat', file: 'art/plat.png', cellWidth: 48, cellHeight: 16 }],
    battle: { backgrounds: { meadow: { sheet: 'bgs', cell: [0, 0] }, cave: { sheet: 'bgs', cell: [1, 0] } }, defaultBackground: 'meadow',
      platforms: { grass: { sheet: 'plat', cell: [0, 0], far: [1, 0] }, boards: { sheet: 'plat', cell: [1, 0] } }, terrainPlatform: { plain: 'grass' }, defaultPlatform: 'boards' } }))
  w('maps/arena.json', json({
    name: 'Arena', grid: { type: 'square', width: 6, height: 3 }, legend: { '.': 'plain', s: 'swamp' }, tiles: ['......', '..ss..', '......'], weather: 'rain',
    battleBackgrounds: { swamp: 'cave', default: 'meadow' }, battlePlatforms: { swamp: 'boards' },
    sides: [{ id: 'blue', controller: 'human' }, { id: 'red', controller: 'ai' }],
    units: [{ type: 'knight', id: 'hero', side: 'blue', x: 1, y: 1, leader: true }, { type: 'knight', id: 'pal', side: 'blue', x: 0, y: 1 }, { type: 'knight', id: 'bandit', side: 'red', x: 2, y: 1 }],
    events: [{ id: 'duel', when: { battle: 'start' }, do: [{ fight: { attacker: 'hero', defender: 'bandit', strikes: [{ by: 'attacker', result: 'hit', damage: 3 }, { by: 'defender', result: 'miss' }] } }] }] }))
  return id
}

const terrain = (id: string, t: string) => readDisk(id, 'data/terrain.json').terrain.find((x: any) => x.id === t)
const ability = (id: string, a: string) => readDisk(id, 'data/abilities.json').abilities.find((x: any) => x.id === a)

async function run(page: Page, game: { problems: (id: string) => Promise<Array<{ file: string; message: string }>> }, id: string, phone: boolean): Promise<void> {
  const tag = phone ? 'phone' : 'desktop'
  expect(await game.problems(id)).toEqual([])

  // ===== An ability with no effect of its own =====
  await page.goto(editorUrl(id, 'abilities', 'rally'))
  await expect(page.getByLabel('What it does', { exact: true })).toHaveValue('none')
  await expect(page.getByTestId('ability-summary')).toContainText('Gives Boost for 4 turns to an ally next to it, and nothing else; uses the unit’s action.')
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-ability-no-effect`)

  // ===== A summon's candidates, the pick and "replace" =====
  await page.goto(editorUrl(id, 'abilities', 'call_pet'))
  const items = page.getByTestId('summon-items')
  await expect(items.getByRole('checkbox', { name: /A new one replaces the oldest at the limit/ })).toBeChecked()
  await expect(items.getByTestId('summon-item-2')).toBeVisible()
  await items.getByRole('button', { name: 'Add a candidate' }).click()
  await saved(page)
  expect(ability(id, 'call_pet').effect.summon.items).toHaveLength(3)
  await items.getByRole('button', { name: 'Remove candidate 3' }).click()
  await items.getByLabel('Which one it carries').selectOption({ label: 'The highest level it reaches' })
  await items.getByRole('checkbox', { name: /A new one replaces the oldest at the limit/ }).uncheck()
  await saved(page)
  expect(ability(id, 'call_pet').effect.summon).toEqual({ type: 'hound', limit: 2, items: [{ item: 'fang', minLevel: 3 }, { item: 'iron_sword' }] })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-summon-items`)

  // ===== Damage that varies =====
  await page.goto(editorUrl(id, 'conditions', 'poison'))
  const hurts = page.getByLabel('Hurts', { exact: true })
  await expect(hurts).toHaveValue('range')
  const hurt = () => readDisk(id, 'data/conditions.json').conditions[1].blocks[0].do[0].hurt
  await hurts.selectOption({ label: 'A number of hit points' })
  await saved(page)
  expect(hurt()).toBe(5)
  await hurts.selectOption({ label: 'Anything from one number to another' })
  await saved(page)
  expect(hurt()).toEqual([1, 3])
  expect(await game.problems(id)).toEqual([])
  await shot(page, `${tag}-hurt-range`)

  // ===== A terrain's weather costs and platform =====
  await page.goto(editorUrl(id, 'terrain', 'swamp'))
  const rain = page.getByLabel('Foot, rain', { exact: true })
  await expect(rain).toHaveValue('4')
  await rain.fill('6')
  await page.getByLabel('Foot, snow', { exact: true }).fill('')
  await saved(page)
  expect(terrain(id, 'swamp').weatherCost).toEqual({ rain: { foot: 6 } })
  await page.getByLabel('Platform in the battle scene', { exact: true }).selectOption({ label: 'grass' })
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').battle.terrainPlatform).toEqual({ plain: 'grass', swamp: 'grass' })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-weather-costs`)

  // ===== The map: its weather and its tables =====
  await openMap(page, id, 'arena')
  await tab(page, 'Map').click()
  const settings = page.getByTestId('battle-scene-settings')
  await expect(settings.getByLabel('Weather at the start', { exact: true })).toHaveValue('rain')
  await settings.getByLabel('Weather at the start', { exact: true }).selectOption({ label: 'Snow' })
  await settings.getByLabel('Background for Plain', { exact: true }).selectOption({ label: 'cave' })
  await settings.getByLabel('Platform for Everything else', { exact: true }).selectOption({ label: 'grass' })
  await saved(page)
  const map = readDisk(id, 'maps/arena.json')
  expect(map.weather).toBe('snow')
  expect(map.battleBackgrounds).toEqual({ plain: 'cave', swamp: 'cave', default: 'meadow' })
  expect(map.battlePlatforms).toEqual({ swamp: 'boards', default: 'grass' })
  await settings.getByLabel('Weather at the start', { exact: true }).selectOption({ label: 'Clear sky' })
  await settings.getByLabel('Background for Plain', { exact: true }).selectOption({ label: 'The game’s choice' })
  await saved(page)
  expect(readDisk(id, 'maps/arena.json')).not.toHaveProperty('weather')
  expect(readDisk(id, 'maps/arena.json').battleBackgrounds).toEqual({ swamp: 'cave', default: 'meadow' })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-map-battle-scene`)

  // ===== The staged fight command =====
  await tab(page, 'Events').click()
  await openEvent(page, 'duel')
  await expect(page.getByTestId('command-fight')).toContainText('2 strikes written down')
  await page.getByTestId('command-fight').click()
  const fight = page.getByTestId('fight-form')
  await expect(fight).toBeVisible()
  await expect(fight.getByTestId('fight-strike-2')).toBeVisible()
  await fight.getByRole('button', { name: 'Add a strike' }).click()
  await fight.getByLabel('Strike 3: what happens', { exact: true }).selectOption({ label: 'Critical hit' })
  await fight.getByRole('checkbox', { name: /Nobody can die in it/ }).check()
  await saved(page)
  expect(readDisk(id, 'maps/arena.json').events[0].do[0].fight).toEqual({
    attacker: 'hero', defender: 'bandit', lethal: false,
    strikes: [{ by: 'attacker', result: 'hit', damage: 3 }, { by: 'defender', result: 'miss' }, { by: 'attacker', result: 'crit' }] })
  await fight.getByRole('checkbox', { name: /Write every strike down/ }).uncheck()
  await saved(page)
  expect(readDisk(id, 'maps/arena.json').events[0].do[0].fight).toEqual({ attacker: 'hero', defender: 'bandit', lethal: false })
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-stage-a-fight`)

  // ===== The game's choices (Rules) and the platforms (Art) =====
  await page.goto(editorUrl(id, 'rules'))
  const sides = page.getByLabel('Which side each fighter stands on', { exact: true })
  await expect(sides).toHaveValue('playerRight')
  await page.getByLabel('Background and platform come from', { exact: true }).selectOption({ label: 'The fighter that attacks' })
  await sides.selectOption({ label: 'By their places on the map' })
  await saved(page)
  const gj = readDisk(id, 'game.json')
  expect(gj.battleBackgroundFrom).toBe('attacker')
  expect(gj).not.toHaveProperty('battleSceneSides')
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
  await shot(page, `${tag}-rules-scene-choices`)

  await page.goto(editorUrl(id, 'art'))
  const plat = page.getByTestId('battle-platforms')
  await expect(plat.getByTestId('platform-grass')).toBeVisible()
  await plat.getByRole('button', { name: 'Delete platform boards' }).click()
  await saved(page)
  const gfx = readDisk(id, 'data/graphics.json').battle
  expect(gfx.platforms.boards).toBeUndefined()
  expect(gfx).not.toHaveProperty('defaultPlatform')  // the default pointed at it
  expect(readDisk(id, 'maps/arena.json').battlePlatforms).toEqual({ default: 'grass' })  // the swamp's entry went with the platform
  expect(await game.problems(id)).toEqual([])
  if (phone) await expectNoHScroll(page)
}

test('@smoke fidelity 2 on the editor screens: the files are what the docs say and the game loads', async ({ page, game }) => {
  test.setTimeout(240_000)
  const id = await fid2Game(game)
  await page.goto(editorUrl(id, 'game'))
  await gotoSection(page, 'Abilities')
  await run(page, game, id, false)
})

test('@smoke on a 360 px phone the fidelity 2 forms fit without sideways scrolling', async ({ page, game }) => {
  test.setTimeout(240_000)
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await fid2Game(game)
  await page.goto(editorUrl(id, 'game'))
  await run(page, game, id, true)
})
