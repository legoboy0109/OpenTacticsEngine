import { useState } from 'react'
import { customFormulasMentioning, formulasUsingStat } from '../../model/formulas'
import { FORMULA_DEFS } from '../../model/presetDefs'
import { findUsages } from '../../model/references'
import { addStat, moveStat, newStatId, removeStat, renameStat, setRole, statRemovalBlock } from '../../model/stats'
import { useDoc, useEdit, useStore } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { NamedListCard } from './NamedListCard'
import { T } from './text'

export function StatsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const game = doc.game!
  const [deleting, setDeleting] = useState<string | null>(null)
  const target = game.stats.find((s) => s.id === deleting)

  const statOptions = game.stats.map((s) => ({ value: s.id, label: s.name }))
  const confirm = () => {
    if (!target) return
    const affected = formulasUsingStat(game, target.id).map((k) => FORMULA_DEFS[k].label)
    edit((d) => removeStat(d, target.id))
    if (affected.length) store.setNotice(T.afterStatDelete(target.name, affected))
    setDeleting(null)
  }
  const notes = target
    ? [
        T.statNote,
        ...formulasUsingStat(game, target.id).map((k) => T.formulaNote(FORMULA_DEFS[k].label)),
        ...customFormulasMentioning(game, target.id).map(T.customFormulaNote),
      ]
    : []

  return (
    <>
      <NamedListCard title={T.stats} help={T.statsHelp} noun={T.statNoun} items={game.stats}
        onAdd={(name) => { const id = newStatId(doc, name); edit((d) => addStat(d, name, id)) }}
        onRename={(id, name) => edit((d) => renameStat(d, id, name), `stat:${id}`)}
        onMove={(a, b) => edit((d) => moveStat(d, a, b))}
        onDelete={setDeleting}>
        <div className="form-grid cols-2">
          <Field label={T.health} help={T.healthHelp}>
            <Select value={game.roles.hp} options={statOptions} onChange={(v) => edit((d) => setRole(d, 'hp', v))} />
          </Field>
          <Field label={T.movement} help={T.movementHelp}>
            <Select value={game.roles.move} options={statOptions} onChange={(v) => edit((d) => setRole(d, 'move', v))} />
          </Field>
        </div>
      </NamedListCard>
      <DeleteConfirm open={!!target} what={target ? T.deleteStatTitle(target.name) : ''} usages={target ? findUsages(doc, 'stat', target.id) : []}
        notes={notes} blockedReason={target ? statRemovalBlock(doc, target.id) : null} onCancel={() => setDeleting(null)} onConfirm={confirm} />
    </>
  )
}
