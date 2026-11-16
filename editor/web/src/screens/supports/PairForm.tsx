// The form of the selected pair: its two characters, the points it starts with, and a card for each level.
import { navigate, projectPath } from '../../router'
import { addLevel, canAddLevel, pairIndex, pairKey, pairsOf, partnerChoices, setPairStart, MAX_POINTS } from '../../model/supports'
import { retargetPair } from '../../model/srpgRefs'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { LevelCard } from './LevelCard'
import { T } from './text'

export function PairForm({ pairId }: { pairId: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const i = pairIndex(doc, pairId)
  const pair = pairsOf(doc)[i]
  if (!pair) return null
  // Changing a character changes the pair's key (its address): follow it so the form stays open.
  const choose = (slot: 'a' | 'b', id: string) => {
    edit((d) => retargetPair(d, i, slot, id))
    navigate(projectPath(store.id, 'supports', pairKey({ ...pair, [slot]: id })))
  }
  const options = (slot: 'a' | 'b') => {
    const list = partnerChoices(doc, i, slot).map((c) => ({ value: c.id, label: c.name }))
    // A character the game no longer has (a damaged file) stays selectable so the form never silently changes it.
    return list.some((o) => o.value === pair[slot]) ? list : [{ value: pair[slot], label: `${pair[slot]} (missing)` }, ...list]
  }
  return (
    <div className="stack">
      <Card title={T.pairCard} help={T.changeHelp}>
        <FormGrid cols={3}>
          <Field label={T.first}><Select value={pair.a} options={options('a')} onChange={(v) => choose('a', v)} /></Field>
          <Field label={T.second}><Select value={pair.b} options={options('b')} onChange={(v) => choose('b', v)} /></Field>
          <Field label={T.start} help={T.startHelp}>
            <NumberInput value={pair.start} min={0} max={MAX_POINTS} allowEmpty placeholder="0" onChange={(v) => edit((d) => setPairStart(d, i, v), `support:${pairId}:start`)} />
          </Field>
        </FormGrid>
      </Card>
      <p className="help">{T.levelsHelp}</p>
      {pair.levels.map((_, n) => <LevelCard key={n} pairKey={pairId} level={n} />)}
      <div>
        <button type="button" className="btn" disabled={!canAddLevel(pair)} onClick={() => edit((d) => addLevel(d, i))}>{T.addLevel}</button>
        {!canAddLevel(pair) && <p className="help">{T.maxLevelsReached}</p>}
      </div>
    </div>
  )
}
