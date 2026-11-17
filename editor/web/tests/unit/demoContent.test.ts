// Step D1 of docs/SRPG.md: the real demo and the strategy-rpg template carry the strategy-RPG content (the other tests use the
// frozen pre-D1 copy of the demo, see fixtures.ts).
import { describe, expect, it } from 'vitest'
import { placeChecks } from '../../src/model/placeChecks'
import { loadLiveDemoSrpg, loadTemplate } from './fixtures'

describe('strategy-RPG content', () => {
  it('the demo has its characters, skills, abilities and support pairs', () => {
    const d = loadLiveDemoSrpg()
    expect(d.characters?.map((c: any) => c.id)).toEqual(['eirik', 'kell', 'lyra', 'bram', 'sera', 'gorm', 'hald'])
    expect(d.skills).toHaveLength(33)
    expect(d.abilities).toHaveLength(5)
    expect(d.supports?.pairs).toHaveLength(4)
    expect((d.game as any).rules.rescue.mode).toBe('carry')
  })

  it('the template has the thirty starter skills, pair up and one support pair', () => {
    const t = loadTemplate('strategy-rpg')
    expect(t.skills).toHaveLength(30)
    expect((t.game as any).rules.rescue.mode).toBe('pairUp')
    expect(t.supports?.pairs).toHaveLength(1)
    expect(t.abilities?.map((a: any) => a.id)).toEqual(['shove'])
  })

  it('the template starts with a door, a chest and a village (each look its own kind), their two keys, and none of them on the map', () => {
    const t = loadTemplate('strategy-rpg')
    expect(t.structures?.map((s) => s.id)).toEqual(['door', 'door_open', 'chest', 'chest_open', 'village', 'village_visited', 'village_ruin'])
    const k = (id: string) => t.structures!.find((s) => s.id === id) as any
    expect(k('door').door).toEqual({ opens: 'door_open' })
    expect(k('door').moveCost).toEqual({ foot: 0, horse: 0 })  // the loader refuses a door that stops nobody
    expect(k('chest').chest).toEqual({ opens: 'chest_open' })
    expect(k('village').visit).toEqual({ becomes: 'village_visited', ruin: 'village_ruin' })
    expect(t.items?.filter((i: any) => i.opens).map((i: any) => [i.id, i.opens])).toEqual([['door_key', ['door']], ['chest_key', ['chest']]])
    expect(t.maps.ford?.structures ?? []).toEqual([])  // a starter set, not a showcase: the map stays a plain skirmish
    expect((t.game as any).rules.places).toBeUndefined()  // rules.places.actions stays at its default (the unit's menu)
    // The editor's own checks (mirroring the loader) refuse nothing; the only notes are "this key opens nothing placed yet" (nothing is placed).
    const found = placeChecks(t)
    expect(found.filter((c) => !c.note)).toEqual([])
    expect(found.map((c) => c.text)).toEqual([expect.stringContaining('Door Key: it is a key, but no door'), expect.stringContaining('Chest Key: it is a key, but no door')])
    // Every look has a picture in every style, as the engine's shipped-games test demands.
    for (const s of t.structures!) {
      expect((t.graphics as any).structures[s.id], s.id).toBeTruthy()
      expect((t.graphics as any).styles.isometric.structures[s.id], s.id).toBeTruthy()
    }
  })
})
