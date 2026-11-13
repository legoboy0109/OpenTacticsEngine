import { DEFEAT_ORDER, defeatConditions, setCounterAttacks, setDefeatCondition, type DefeatCondition } from '../../model/rules'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { FieldGroup } from '../../ui/Field'
import { Check, Toggle } from '../../ui/Toggle'
import { T } from './text'

const LABEL: Record<DefeatCondition, string> = { noUnits: T.defeatNoUnits, leaderDead: T.defeatLeader, lostHq: T.defeatHq }

export function BattleCard() {
  const doc = useDoc()
  const edit = useEdit()
  const active = defeatConditions(doc)
  return (
    <Card title={T.battle} help={T.battleHelp}>
      <div className="stack">
        <Toggle label={T.counter} help={T.counterHelp} checked={doc.game!.rules?.counterAttacks !== false}
          onChange={(v) => edit((d) => setCounterAttacks(d, v))} />
        <FieldGroup label={T.defeat} help={T.defeatHelp}>
          {DEFEAT_ORDER.map((c) => (
            <Check key={c} label={LABEL[c]} checked={active.includes(c)} disabled={active.includes(c) && active.length === 1}
              onChange={(v) => edit((d) => setDefeatCondition(d, c, v))} />
          ))}
        </FieldGroup>
      </div>
    </Card>
  )
}
