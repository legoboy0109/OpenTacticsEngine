// Loads the real demo projects (JSON with comments) from ../../games as documents.
import { readFileSync } from 'node:fs'
import path from 'node:path'
import type { ProjectDoc } from '../../src/model/types'

const games = path.resolve(import.meta.dirname, '../../../../games')

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

const read = (name: string, rel: string) => parseLoose(readFileSync(path.join(games, name, rel), 'utf8'))

export function loadDemo(name: 'demo-srpg' | 'demo-economy'): ProjectDoc {
  const game = read(name, 'game.json')
  const maps: ProjectDoc['maps'] = {}
  for (const m of game.maps) maps[m.id] = read(name, m.file)
  return {
    game,
    terrain: read(name, 'data/terrain.json').terrain,
    weapons: read(name, 'data/weapons.json').weapons,
    units: read(name, 'data/units.json').units,
    maps,
    scripts: {},
  }
}
