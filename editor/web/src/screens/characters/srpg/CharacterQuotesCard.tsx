// The Quotes card of a character (docs/EVENTS.md "Battle and defeat quotes"): battle quotes (who it is said against, how often,
// the lines) and the defeat quote(s). The lines are edited with the SAME forms as a story's `say` commands (SceneCommands,
// limited to `say`), so portraits, speakers and free names work as everywhere else.
import { useMemo } from 'react'
import {
  addQuote, battleQuotes, defeatLines, defeatSets, moveQuote, onceOf, removeQuote, setDefeatLines, setQuoteAgainst, setQuoteLines, setQuoteOnce,
  type QuoteOnce,
} from '../../../model/quotes'
import { talkBase } from '../../../model/supportTalk'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Select } from '../../../ui/Select'
import { SceneCommands, type SceneSource } from '../../campaign/SceneCommands'
import { QT } from './quotesText'
import './quotes.css'

/** The line editor of one list of lines; the list is found again by its place at every edit, so an edit lands on the right one. */
function Lines({ eventId, read, write, name, owner }: { eventId: string; read: SceneSource['read']; write: SceneSource['write']; name: string; owner: string }) {
  // A line with no speaker is said by the character the quote belongs to, so the empty speaker choice says so.
  const source = useMemo<SceneSource>(() => ({ eventId, read, write, goto: false, only: ['say'], base: talkBase, emptySpeaker: QT.thisCharacter(owner) }), [eventId, read, write, owner])
  return <SceneCommands source={source} name={name} />
}

function BattleQuote({ id, index, count }: { id: string; index: number; count: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const owner = doc.characters?.find((c) => c.id === id)?.name ?? id
  const q = battleQuotes(doc, id)[index]
  const read = useMemo<SceneSource['read']>(() => (d) => battleQuotes(d, id)[index]?.lines ?? [], [id, index])
  const write = useMemo<SceneSource['write']>(() => (d, l) => setQuoteLines(d, id, index, l), [id, index])
  if (!q) return null
  const n = index + 1
  const key = `quote:${id}:${index}`
  // Characters other than itself; a damaged `against` (a missing character) stays selectable so the form never silently changes it.
  const others = (doc.characters ?? []).filter((c) => c.id !== id)
  const known = q.against === undefined || others.some((c) => c.id === q.against)
  const options = [{ value: '', label: QT.anyone }, ...others.map((c) => ({ value: c.id, label: c.name })), ...(known ? [] : [{ value: q.against!, label: `${q.against} (missing)` }])]
  return (
    <div className="quote-block" data-testid={`battle-quote-${n}`}>
      <div className="quote-head">
        <h3 className="sub-title">{QT.quote(n)}</h3>
        <div className="quote-actions">
          <button type="button" className="btn sm" aria-label={QT.moveUp(n)} disabled={index === 0} onClick={() => edit((d) => moveQuote(d, id, index, index - 1))}>↑</button>
          <button type="button" className="btn sm" aria-label={QT.moveDown(n)} disabled={index === count - 1} onClick={() => edit((d) => moveQuote(d, id, index, index + 1))}>↓</button>
          <button type="button" className="btn sm danger" aria-label={QT.remove(n)} onClick={() => edit((d) => removeQuote(d, id, index))}>Remove</button>
        </div>
      </div>
      <div className="quote-fields">
        <Field label={QT.against} help={QT.againstHelp}>
          <Select value={q.against ?? ''} options={options} aria-label={`${QT.against} (${QT.quote(n)})`}
            onChange={(v) => edit((d) => setQuoteAgainst(d, id, index, v === '' ? undefined : v))} />
        </Field>
        <Field label={QT.once} help={QT.onceHelp}>
          <Select value={onceOf(q)} options={QT.onceChoices} aria-label={`${QT.once} (${QT.quote(n)})`}
            onChange={(v) => edit((d) => setQuoteOnce(d, id, index, v as QuoteOnce))} />
        </Field>
      </div>
      <h4 className="quote-lines-title">{QT.lines}</h4>
      <div data-testid={`battle-quote-lines-${n}`}>
        <Lines eventId={key} read={read} write={write} name={QT.linesName(n)} owner={owner} />
      </div>
    </div>
  )
}

function DefeatSet({ id, set, title, help, name }: { id: string; set: 'falls' | 'retreats'; title: string; help: string; name: string }) {
  const doc = useDoc()
  const lines = defeatLines(doc, id, set)
  const owner = doc.characters?.find((c) => c.id === id)?.name ?? id
  const read = useMemo<SceneSource['read']>(() => (d) => defeatLines(d, id, set), [id, set])
  const write = useMemo<SceneSource['write']>(() => (d, l) => setDefeatLines(d, id, set, l), [id, set])
  return (
    <div className="quote-block" data-testid={`defeat-quote-${set}`}>
      <h3 className="sub-title">{title}</h3>
      <p className="help">{lines.length ? help : `${QT.noLines} ${help}`}</p>
      <Lines eventId={`quote:${id}:${set}`} read={read} write={write} name={name} owner={owner} />
    </div>
  )
}

export function CharacterQuotesCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  if (!doc.characters?.some((c) => c.id === id)) return null
  const battle = battleQuotes(doc, id)
  const sets = defeatSets(doc)
  const one = sets.length === 1
  return (
    <Card title={QT.title} help={QT.help} id="character-quotes">
      <div className="stack" data-testid="character-quotes">
        <section className="stack tight">
          <h3 className="sub-title">{QT.battleTitle}</h3>
          <p className="help">{QT.battleHelp}</p>
          {battle.length === 0 && <p className="help" data-testid="no-battle-quotes">{QT.noBattle}</p>}
          {battle.map((_, i) => <BattleQuote key={i} id={id} index={i} count={battle.length} />)}
          <div><button type="button" className="btn" data-testid="add-battle-quote" onClick={() => edit((d) => addQuote(d, id))}>{QT.add}</button></div>
        </section>
        <section className="stack tight">
          <h3 className="sub-title">{QT.defeatTitle}</h3>
          <p className="help">{QT.defeatHelp}</p>
          {sets.includes('falls') && <DefeatSet id={id} set="falls" title={one ? QT.fallsOnly : QT.falls} help={one ? QT.fallsOnlyHelp : QT.fallsHelp} name={QT.fallsName} />}
          {sets.includes('retreats') && <DefeatSet id={id} set="retreats" title={QT.retreats} help={QT.retreatsHelp} name={QT.retreatsName} />}
        </section>
      </div>
    </Card>
  )
}
