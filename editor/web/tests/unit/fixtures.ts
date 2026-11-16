// Loads the real demo projects (JSON with comments) from ../../games as documents.
import { existsSync, readFileSync } from 'node:fs'
import path from 'node:path'
import type { ProjectDoc } from '../../src/model/types'

const games = path.resolve(import.meta.dirname, '../../../../games')
const templates = path.resolve(import.meta.dirname, '../../../templates')
const fixtures = path.resolve(import.meta.dirname, '../../../../tests/fixtures')

/** Removes // comments and trailing commas without touching strings. */
export function parseLoose(text: string): any {
  let out = ''
  let inStr = false
  for (let i = 0; i < text.length; i++) {
    const c = text[i]
    if (inStr) {
      out += c
      if (c === '\\') out += text[++i]
      else if (c === '"') inStr = false
    } else if (c === '"') { inStr = true; out += c }
    else if (c === '/' && text[i + 1] === '/') { while (i < text.length && text[i] !== '\n') i++; out += '\n' }
    else out += c
  }
  return JSON.parse(out.replace(/,(\s*[}\]])/g, '$1'))
}

const readIn = (root: string) => (name: string, rel: string) => parseLoose(readFileSync(path.join(root, name, rel), 'utf8'))

export const TEMPLATES = ['strategy-rpg', 'economy-tactics', 'blank'] as const

/** demo-economy as it is. demo-srpg is the FROZEN copy from before step D1 (tests/fixtures/plain-srpg, no art files): since D1 the live
 *  demo has characters, skills, promotions, abilities, carrying and supports, and these tests are about a game that has none of them
 *  (use loadLiveDemoSrpg for the real one). */
export function loadDemo(name: 'demo-srpg' | 'demo-economy'): ProjectDoc {
  return name === 'demo-srpg' ? loadFrom(fixtures, 'plain-srpg') : loadFrom(games, name)
}
/** The real, current games/demo-srpg with all its strategy-RPG content. */
export function loadLiveDemoSrpg(): ProjectDoc { return loadFrom(games, 'demo-srpg') }
/** The frozen copy of demo-economy from before it used upgrades, research, supply and fog (tests/fixtures/plain-economy,
 *  no art): the editor tests that need "a game with none of the optional economy data" use it, the live demo has some. */
export function loadPlainEconomy(): ProjectDoc { return loadFrom(fixtures, 'plain-economy') }
/** The engine's hexagon test games (tests/fixtures/hex-pointy, hex-flat; no art): a 12 x 9 map with an HQ of seven hexagons at (1,4) and (10,4). */
export function loadHexFixture(layout: 'pointy' | 'flat'): ProjectDoc { return loadFrom(fixtures, `hex-${layout}`) }
/** The engine's weapon ranks and stat caps test game (tests/fixtures/srpg-ranks): a ladder E-B, weapons with ranks, classes with ranks and caps. */
export function loadRanksFixture(): ProjectDoc { return loadFrom(fixtures, 'srpg-ranks') }
/** The engine's character-locked weapons test game (tests/fixtures/srpg-locks): three characters, weapons locked to one to three of them. */
export function loadLocksFixture(): ProjectDoc { return loadFrom(fixtures, 'srpg-locks') }
export function loadTemplate(name: (typeof TEMPLATES)[number]): ProjectDoc { return loadFrom(templates, name) }

/** Any template by id, including the hexagon ones (the lesson tests start from the template a lesson names). */
export function loadTemplateById(name: string): ProjectDoc { return loadFrom(templates, name) }

function loadFrom(root: string, name: string): ProjectDoc {
  const read = readIn(root)
  const game = read(name, 'game.json')
  const maps: ProjectDoc['maps'] = {}
  for (const m of game.maps) maps[m.id] = read(name, m.file)
  return {
    game,
    terrain: read(name, 'data/terrain.json').terrain,
    structures: existsSync(path.join(root, name, 'data/structures.json')) ? read(name, 'data/structures.json').structures : null,
    items: existsSync(path.join(root, name, 'data/items.json')) ? read(name, 'data/items.json').items : null,
    research: existsSync(path.join(root, name, 'data/research.json')) ? read(name, 'data/research.json').technologies : null,
    weapons: read(name, 'data/weapons.json').weapons,
    units: read(name, 'data/units.json').units,
    maps,
    scripts: {},
    // Art is optional: a game without data/graphics.json has none.
    graphics: existsSync(path.join(root, name, 'data/graphics.json')) ? read(name, 'data/graphics.json') : null,
    campaign: existsSync(path.join(root, name, 'data/campaign.json')) ? read(name, 'data/campaign.json') : null,
    // Sound is optional too (the demos and templates may or may not have it: tests must not rely on either).
    audio: existsSync(path.join(root, name, 'data/audio.json')) ? read(name, 'data/audio.json') : null,
    // The strategy-RPG files (docs/SRPG.md) are optional as well.
    characters: existsSync(path.join(root, name, 'data/characters.json')) ? read(name, 'data/characters.json').characters : null,
    skills: existsSync(path.join(root, name, 'data/skills.json')) ? read(name, 'data/skills.json').skills : null,
    abilities: existsSync(path.join(root, name, 'data/abilities.json')) ? read(name, 'data/abilities.json').abilities : null,
    supports: existsSync(path.join(root, name, 'data/supports.json')) ? read(name, 'data/supports.json') : null,
    ranks: existsSync(path.join(root, name, 'data/ranks.json')) ? read(name, 'data/ranks.json') : null,
    caps: existsSync(path.join(root, name, 'data/caps.json')) ? read(name, 'data/caps.json') : null,
  }
}
