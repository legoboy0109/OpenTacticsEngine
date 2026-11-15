// The bonus part of a technology: a switch, which unit types (all, or only some) and one plus / minus number per stat.
import { setBonusOn, setBonusStat, setBonusTypes } from '../../model/research'
import { useDoc, useEdit } from '../../store/context'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { MultiSelect } from '../../ui/MultiSelect'
import { Select } from '../../ui/Select'
import { Field } from '../../ui/Field'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'

export function ResearchBonus({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const tech = doc.research?.find((t) => t.id === id)
  if (!tech) return null
  const key = `tech:${id}`
  const bonus = tech.bonus
  const units = doc.units ?? []
  const types = bonus?.types ?? []
  const stats = doc.game?.stats ?? []
  // Stats the file names that the game no longer has stay visible so they can be cleared (the checks flag them).
  const known = new Set(stats.map((s) => s.id))
  const rows = [...stats.map((s) => ({ key: s.id, label: s.name })), ...Object.keys(bonus?.stats ?? {}).filter((k) => !known.has(k)).map((k) => ({ key: k, label: k }))]

  return (
    <div className="stack">
      <Toggle label={T.bonusOn} help={T.bonusOnHelp} checked={!!bonus} onChange={(on) => edit((d) => setBonusOn(d, id, on))} />
      {bonus && (
        <>
          <Field label={T.bonusWho} help={types.length ? T.bonusWhoHelp : undefined}>
            <Select value={types.length ? 'only' : 'all'}
              options={[{ value: 'all', label: T.bonusAll }, { value: 'only', label: T.bonusOnly }]}
              onChange={(v) => edit((d) => setBonusTypes(d, id, v === 'all' ? [] : units[0] ? [units[0].id] : []))} />
          </Field>
          {types.length > 0 && (
            <MultiSelect label={T.bonusOnly} value={types} options={units.map((u) => ({ value: u.id, label: u.name }))}
              onChange={(v) => edit((d) => setBonusTypes(d, id, v))} />
          )}
          {rows.length === 0
            ? <p className="help">{T.bonusNoStats}</p>
            : (
              <KeyNumberTable label={T.bonusStats} help={T.bonusStatsHelp} rows={rows}
                columns={[{ title: T.bonusCol, values: bonus.stats, placeholder: '0', onChange: (k, v) => edit((d) => setBonusStat(d, id, k, v), `${key}:bonus:${k}`) }]} />
            )}
        </>
      )}
    </div>
  )
}
