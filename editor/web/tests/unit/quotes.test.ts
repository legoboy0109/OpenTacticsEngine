// Battle and defeat quotes (docs/EVENTS.md): the edits behind the Quotes card, the checks that mirror the loader, the delete question and
// the clean-up. Game: tests/fixtures/srpg-quotes (eirik, bram, gorm and tolly have quotes; mira has none).
import { describe, expect, it } from 'vitest'
import { characterChecks } from '../../src/model/characterChecks'
import { setFallenRule } from '../../src/model/campaign'
import {
  addQuote, battleQuotes, cleanQuotes, defeatLines, defeatSets, moveQuote, onceOf, quotesOf, quoteUsageLines, removeQuote, setDefeatLines, setQuoteAgainst,
  setQuoteLines, setQuoteOnce,
} from '../../src/model/quotes'
import { quoteChecks, QUOTE_TEXT } from '../../src/model/quoteChecks'
import { characterUsageLines, deleteCharacter } from '../../src/model/srpgRefs'
import type { ProjectDoc } from '../../src/model/types'
import { loadQuotesFixture } from './fixtures'

/** The fixture with fallen units coming back, so both defeat sets are in use (as written it says "gone" and its retreats lines are unused). */
const game = (): ProjectDoc => setFallenRule(loadQuotesFixture(), 'return')

describe('the fixture as a document', () => {
  it('has the quotes the engine tests use, and no editor check against them once its retreats lines are in use', () => {
    const d = game()
    expect(battleQuotes(d, 'eirik').map((q) => q.against)).toEqual(['gorm', undefined])
    expect(battleQuotes(d, 'gorm').map(onceOf)).toEqual(['campaign', 'always'])
    expect(defeatLines(d, 'gorm', 'falls').map((l) => l.speaker)).toEqual([undefined, 'mira'])
    expect(quotesOf(d, 'mira')).toEqual({})
    expect(quoteChecks(d)).toEqual([])
  })
})

describe('the Quotes card edits', () => {
  it('adds a quote with one empty line, once per map, against anyone', () => {
    const d = addQuote(game(), 'mira')
    expect(quotesOf(d, 'mira')).toEqual({ battle: [{ lines: [{ say: '' }] }] })
  })
  it('sets who it is against (anyone removes the key) and how often (the default is not written)', () => {
    let d = setQuoteAgainst(game(), 'eirik', 1, 'bram')
    expect(battleQuotes(d, 'eirik')[1].against).toBe('bram')
    d = setQuoteAgainst(d, 'eirik', 1, undefined)
    expect('against' in battleQuotes(d, 'eirik')[1]).toBe(false)
    d = setQuoteOnce(d, 'eirik', 1, 'always')
    expect(battleQuotes(d, 'eirik')[1].once).toBe('always')
    d = setQuoteOnce(d, 'eirik', 1, 'map')
    expect('once' in battleQuotes(d, 'eirik')[1]).toBe(false)
  })
  it('reorders and removes; the last removal takes the whole quotes key when nothing else is left', () => {
    let d = moveQuote(game(), 'eirik', 0, 1)
    expect(battleQuotes(d, 'eirik').map((q) => q.against)).toEqual([undefined, 'gorm'])
    d = removeQuote(removeQuote(game(), 'tolly', 0), 'tolly', 0)
    expect(d.characters!.find((c) => c.id === 'tolly')).toEqual({ id: 'tolly', name: 'Tolly', type: 'grunt' })
  })
  it('keeps the lines it is given (an empty list stays until the maker adds a line; the check says so)', () => {
    const d = setQuoteLines(game(), 'bram', 0, [])
    expect(battleQuotes(d, 'bram')[0].lines).toEqual([])
    expect(quoteChecks(d).map((c) => c.text)).toEqual([QUOTE_TEXT.noLines(1)])
  })
  it('sets a defeat set and an emptied set goes away (and the defeat object with it)', () => {
    let d = setDefeatLines(game(), 'mira', 'falls', [{ say: 'Oh no.' }])
    expect(quotesOf(d, 'mira')).toEqual({ defeat: { falls: [{ say: 'Oh no.' }] } })
    d = setDefeatLines(d, 'mira', 'falls', [])
    expect(quotesOf(d, 'mira')).toEqual({})
    expect(d.characters!.find((c) => c.id === 'mira')!.quotes).toBeUndefined()
  })
  it('offers one defeat set when fallen units are gone and two when they come back', () => {
    const d = loadQuotesFixture()
    expect(defeatSets(d)).toEqual(['falls'])
    expect(defeatSets(game())).toEqual(['falls', 'retreats'])
    expect(defeatSets({ ...d, campaign: null })).toEqual(['falls'])
  })
})

describe('the checks mirror the loader', () => {
  const eirik = (d: ProjectDoc, fn: (q: NonNullable<ProjectDoc['characters']>[number]) => void) => { fn(d.characters!.find((c) => c.id === 'eirik')!) }
  const texts = (d: ProjectDoc) => quoteChecks(d).map((c) => c.text)
  it('empty lines', () => {
    expect(texts(setQuoteLines(game(), 'bram', 0, [{ say: '  ' }]))).toEqual([QUOTE_TEXT.emptyLine('Battle quote 1', 1)])
    expect(texts(setDefeatLines(game(), 'bram', 'falls', [{ say: '' }]))).toEqual([QUOTE_TEXT.emptyLine('Defeat quote', 1)])
  })
  it('an unknown character in against, a quote against oneself, and the same opponent twice', () => {
    expect(texts(setQuoteAgainst(game(), 'eirik', 0, 'nobody'))).toEqual([QUOTE_TEXT.unknownAgainst(1, 'nobody')])
    expect(texts(setQuoteAgainst(game(), 'eirik', 0, 'eirik'))).toEqual([QUOTE_TEXT.againstSelf(1)])
    expect(texts(setQuoteAgainst(game(), 'eirik', 1, 'gorm'))).toEqual([QUOTE_TEXT.duplicate(2, 'against Gorm')])
    expect(texts(addQuote(game(), 'eirik'))).toEqual([QUOTE_TEXT.emptyLine('Battle quote 3', 1), QUOTE_TEXT.duplicate(3, 'against anyone')])
  })
  it('a line spoken by an unknown character', () => {
    const d = setQuoteLines(game(), 'bram', 0, [{ say: 'Hah!', speaker: 'ghost' }])
    expect(texts(d)).toEqual([QUOTE_TEXT.unknownSpeaker('Battle quote 1', 'ghost')])
  })
  it('lines in a defeat set the game never uses', () => {
    const gone = loadQuotesFixture()  // eirik's and bram's retreats lines are not used while fallen units are gone
    expect(texts(gone)).toEqual([QUOTE_TEXT.unusedSet('retreats'), QUOTE_TEXT.unusedSet('retreats')])
    expect(texts(game())).toEqual([])
  })
  it('the Characters screen list of checks carries them', () => {
    let d = game()
    eirik(d, () => undefined)
    d = setQuoteAgainst(d, 'eirik', 0, 'nobody')
    expect(characterChecks(d).some((c) => c.characterId === 'eirik' && c.text === QUOTE_TEXT.unknownAgainst(1, 'nobody'))).toBe(true)
  })
})

describe('deleting a character', () => {
  it('lists the quotes that name it and takes them away', () => {
    const d = game()
    const used = quoteUsageLines(d, 'gorm').map((u) => u.text)
    expect(used).toEqual(['Eirik’s battle quote against it (the quote goes with it)'])
    expect(characterUsageLines(d, 'gorm').some((u) => u.text.includes('battle quote against it'))).toBe(true)
    const after = deleteCharacter(d, 'gorm')
    expect(battleQuotes(after, 'eirik').map((q) => q.against)).toEqual([undefined])
    expect(after.characters!.some((c) => c.id === 'gorm')).toBe(false)
  })
  it('drops the lines the deleted character speaks, and a quote left with no lines', () => {
    let d = game()
    d = setQuoteLines(d, 'bram', 0, [{ say: 'Together.', speaker: 'mira' }])
    expect(quoteUsageLines(d, 'mira').map((u) => u.text).length).toBe(3)  // bram's quote, gorm's two sets of lines
    const after = cleanQuotes(d, 'mira')
    expect(quotesOf(after, 'bram').battle).toBeUndefined()
    expect(defeatLines(after, 'gorm', 'falls').map((l) => l.say)).toEqual(['Impossible...'])
    expect(battleQuotes(after, 'gorm')[0].lines.map((l) => l.say)).toEqual(['Come, little ones.'])
  })
  it('does nothing when no quote names it', () => {
    const d = game()
    expect(cleanQuotes(d, 'tolly')).toBe(d)
  })
})
