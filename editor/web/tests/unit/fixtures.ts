// Loads the real demo projects (JSON with comments) from ../../games as documents.
import { existsSync, readFileSync } from 'node:fs'
import path from 'node:path'
import type { ProjectDoc } from '../../src/model/types'

const games = path.resolve(import.meta.dirname, '../../../../games')
const templates = path.resolve(import.meta.dirname, '../../../templates')

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

export function loadDemo(name: 'demo-srpg' | 'demo-economy'): ProjectDoc { return loadFrom(games, name) }
export function loadTemplate(name: (typeof TEMPLATES)[number]): ProjectDoc { return loadFrom(templates, name) }

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
    weapons: read(name, 'data/weapons.json').weapons,
    units: read(name, 'data/units.json').units,
    maps,
    scripts: {},
    // Art is optional: a game without data/graphics.json has none.
    graphics: existsSync(path.join(root, name, 'data/graphics.json')) ? read(name, 'data/graphics.json') : null,
    campaign: existsSync(path.join(root, name, 'data/campaign.json')) ? read(name, 'data/campaign.json') : null,
  }
}
