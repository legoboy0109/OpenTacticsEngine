// A usable item's two condition parts (docs/SRPG.md section 9): "Cures" (none / the ones I pick / every condition) and "Inflicts on the unit it is used on"
// (which, optional turns). Shown when the game has conditions or the item already has one of the keys.
import { useEffect, useState } from 'react'
import { conditionList } from '../../model/conditions'
import { itemCureOf, itemInflictOf, setItemCureIds, setItemCureMode, setItemInflictCondition, setItemInflictOn, setItemInflictTurns, type CureMode } from '../../model/conditionSites'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { ConditionPick } from '../conditions/ConditionPick'
import { C } from '../conditions/siteText'

export function ItemConditionFields({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  // "The conditions I pick" with nothing picked yet: nothing is written (the loader refuses an empty list); the first tick writes it.
  const [picking, setPicking] = useState(false)
  useEffect(() => setPicking(false), [id])
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const stored = itemCureOf(item)
  const cure = picking ? { mode: 'list' as CureMode, ids: [] as string[] } : stored
  const inflict = itemInflictOf(item)
  const has = conditionList(doc).length > 0
  if (!has && cure.mode === 'none' && !inflict.on) return doc.conditions === null ? null : <p className="help" data-testid="item-conditions-none">{C.itemNone}</p>
  const options = conditionList(doc).map((c) => ({ value: c.id, label: c.name }))
  for (const x of cure.ids) if (!options.some((o) => o.value === x)) options.push({ value: x, label: C.notOne(x) })
  return (
    <div className="stack" data-testid="item-conditions">
      <Field label={C.itemCures} help={C.itemCureHelp}>
        <Select value={cure.mode} options={(Object.keys(C.itemCureKinds) as CureMode[]).map((m) => ({ value: m, label: C.itemCureKinds[m], disabled: m === 'list' && !has && cure.mode !== 'list' }))}
          onChange={(m) => { if (m === 'list') { if (stored.mode !== 'list') setPicking(true) } else { setPicking(false); edit((d) => setItemCureMode(d, id, m as Exclude<CureMode, 'list'>)) } }} />
      </Field>
      {cure.mode === 'list' && <MultiSelect label={C.itemCureList} options={options} value={cure.ids} onChange={(v) => { if (v.length) setPicking(false); edit((d) => setItemCureIds(d, id, v)) }} />}
      {picking && <p className="help" data-testid="item-cure-pick">{C.cureNeedsOne}</p>}
      <Check label={C.itemInflict} help={C.itemInflictHelp} checked={inflict.on} disabled={!inflict.on && !has} onChange={(on) => edit((d) => setItemInflictOn(d, id, on))} />
      {inflict.on && (
        <div className="indent stack" data-testid="item-inflict">
          <ConditionPick condition={inflict.condition} turns={inflict.turns} onCondition={(c) => edit((d) => setItemInflictCondition(d, id, c))}
            onTurns={(n) => edit((d) => setItemInflictTurns(d, id, n), `item:${id}:inflictTurns`)} />
        </div>
      )}
    </div>
  )
}
