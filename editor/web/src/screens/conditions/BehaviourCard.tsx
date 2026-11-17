// "What it stops and forces": the four effects only a condition has, as plain switches (cannot act with two options, cannot move, forbid, acts on its own).
// The choices the loader refuses are greyed out with the reason beside them (cannot act together with acting on its own; the last effect; no room for another
// block). In the file they are effects of `always` blocks (model/conditionBehaviour.ts).
import { cannotActOf, forbidOf, hasOwn, FORBID_KINDS, ownBlock, setBerserk, setCannotAct, setCannotActOption, setCannotMove, setForbidGroups, setForbidKinds, setForbidOn } from '../../model/conditionBehaviour'
import { groupsOf } from '../../model/ranks'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { MultiSelect } from '../../ui/MultiSelect'
import { Check, Toggle } from '../../ui/Toggle'
import { T } from './text'

/** The reason (words) a switch is greyed out for, or undefined. */
const why = (b: ReturnType<typeof ownBlock>): string | undefined => (b === 'berserk' ? T.cannotActBerserk : b === 'cannotAct' ? T.berserkCannotAct : b === 'full' ? T.full : undefined)

export function BehaviourCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.conditions?.find((x) => x.id === id)
  if (!c) return null
  const act = cannotActOf(c)
  const forbid = forbidOf(c)
  const berserk = hasOwn(c, 'berserk')
  const move = hasOwn(c, 'cannotMove')
  const bAct = ownBlock(c, 'cannotAct'), bBerserk = ownBlock(c, 'berserk'), bForbid = ownBlock(c, 'forbid'), bMove = ownBlock(c, 'cannotMove')
  const groups = groupsOf(doc)
  const groupOptions = groups.map((g) => ({ value: g, label: g }))
  // Groups the file names that the game does not have stay on the list (marked), so they can be unticked.
  for (const g of forbid.groups) if (!groups.includes(g)) groupOptions.push({ value: g, label: `${g} (not a group of this game)` })

  return (
    <Card title={T.behaviourTitle} help={T.behaviourHelp} id="condition-behaviour">
      <div className="stack" data-testid="condition-behaviour">
        <Toggle label={T.cannotAct} checked={act.on} learn="condition-cannot-act" disabled={act.on ? bAct === 'last' : bAct !== ''}
          help={act.on ? (bAct === 'last' ? T.last : T.cannotActHelp) : why(bAct) ?? T.cannotActHelp} onChange={(on) => edit((d) => setCannotAct(d, id, on))} />
        {act.on && (
          <div className="indent stack">
            <Check label={T.counter} help={T.counterHelp} checked={act.counter} onChange={(on) => edit((d) => setCannotActOption(d, id, 'counter', on))} />
            <Check label={T.dodge} help={T.dodgeHelp} checked={act.dodge} onChange={(on) => edit((d) => setCannotActOption(d, id, 'dodge', on))} />
          </div>
        )}
        <Toggle label={T.cannotMove} checked={move} disabled={move ? bMove === 'last' : bMove !== ''}
          help={move ? (bMove === 'last' ? T.last : T.cannotMoveHelp) : why(bMove) ?? T.cannotMoveHelp} onChange={(on) => edit((d) => setCannotMove(d, id, on))} />
        <Toggle label={T.forbid} checked={forbid.on} disabled={forbid.on ? bForbid === 'last' : bForbid !== ''}
          help={forbid.on ? (bForbid === 'last' ? T.last : T.forbidHelp) : why(bForbid) ?? T.forbidHelp} onChange={(on) => edit((d) => setForbidOn(d, id, on))} />
        {forbid.on && (
          <div className="indent stack" data-testid="condition-forbid">
            <MultiSelect label={T.forbidKinds} options={FORBID_KINDS.map((k) => ({ value: k, label: T.forbidKindNames[k] }))} value={forbid.kinds}
              onChange={(v) => edit((d) => setForbidKinds(d, id, v))} />
            <MultiSelect label={T.forbidGroups} help={T.forbidGroupsHelp} options={groupOptions} value={forbid.groups} emptyText={T.forbidGroupsNone}
              onChange={(v) => edit((d) => setForbidGroups(d, id, v))} />
            {forbid.kinds.length === 0 && forbid.groups.length === 0 && <p className="help" role="alert" data-testid="condition-forbid-nothing">{T.forbidNothing}</p>}
          </div>
        )}
        <Toggle label={T.berserk} checked={berserk} disabled={berserk ? bBerserk === 'last' : bBerserk !== ''}
          help={berserk ? (bBerserk === 'last' ? T.last : T.berserkHelp) : why(bBerserk) ?? T.berserkHelp} onChange={(on) => edit((d) => setBerserk(d, id, on))} />
      </div>
    </Card>
  )
}
