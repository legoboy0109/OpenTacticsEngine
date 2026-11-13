import { describe, expect, it } from 'vitest'
import { makeId, projectSlug, slugify, uniqueId, uniqueProjectId } from '../../src/model/ids'

describe('slugify', () => {
  it('makes lowercase underscore ids', () => expect(slugify('Iron Sword')).toBe('iron_sword'))
  it('folds accents', () => expect(slugify('Café Étoile')).toBe('cafe_etoile'))
  it('falls back for non-ASCII or empty names', () => {
    expect(slugify('日本語')).toBe('item')
    expect(slugify('   ')).toBe('item')
    expect(slugify('', { fallback: 'unit' })).toBe('unit')
  })
  it('trims separators and punctuation', () => expect(slugify('  --Hello, World!--  ')).toBe('hello_world'))
  it('caps the length', () => expect(slugify('a'.repeat(80)).length).toBe(40))
})

describe('uniqueness', () => {
  it('appends _2, _3', () => {
    expect(uniqueId('sword', [])).toBe('sword')
    expect(uniqueId('sword', ['sword'])).toBe('sword_2')
    expect(uniqueId('sword', ['sword', 'sword_2'])).toBe('sword_3')
  })
  it('makeId avoids reserved names', () => expect(makeId('Level', [], { reserved: ['level'] })).toBe('level_2'))
  it('project ids use hyphens and stay within 40 characters', () => {
    expect(projectSlug('My Game!')).toBe('my-game')
    expect(projectSlug('日本')).toBe('game')
    const id = uniqueProjectId('x'.repeat(60), ['x'.repeat(40)])
    expect(id.length).toBeLessThanOrEqual(40)
    expect(id.endsWith('-2')).toBe(true)
    expect(id).toMatch(/^[a-z0-9][a-z0-9-]{0,39}$/)
  })
})
