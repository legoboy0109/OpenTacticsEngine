// A list of conditions ("Only if", and the conditions of an If command): each one is a row with its kind and fields
// always showing; add, reorder and delete like every list.
import { listAdd, listMove, listPatch, listRemove, listReplace } from '../../../../model/storyEdit'
import { newCondition } from '../../../../model/storyDefaults'
import { COMPARE_OPS, CONDITION_KINDS, asFilter, conditionKind, type CompareOp, type Condition, type ConditionKind } from '../../../../model/storyTypes'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { ConditionPick, FilterPicker, ItemPick, PickSelect, SidePick, SkillPick, SwitchPick, TeamPick, TechPick, UnitPick, VariablePick } from './pickers'
import { PairPick } from './forms/SupportForms'
import { RowButtons } from '../../../../ui/RowButtons'
import { useStory } from './StoryContext'
import { S } from './text'
import { EX } from './forms/extrasText'

interface Props {
  conditions: Condition[]
  onChange: (next: Condition[], key?: string) => void
  /** Makes the add box's name unique when several lists show at once. */
  name: string
}

/** A campaign scene can only test these (the loader refuses the others there). */
/** (a scene has no fog: `sees` is for map events only) */
const SCENE_CONDITIONS: ConditionKind[] = ['switch', 'variable', 'alive', 'dead', 'support']

const OPS = COMPARE_OPS.map((o) => ({ value: o, label: S.ops[o] }))

export function ConditionList({ conditions, onChange, name }: Props) {
  const story = useStory()
  // "Support level" needs a pair to be about: it is offered once the game has one.
  const hasPairs = (story.doc.supports?.pairs?.length ?? 0) > 0
  // "A unit has a skill" needs a skill to name (the loader refuses an unknown one).
  const kinds = (story.scene ? CONDITION_KINDS.filter((k) => SCENE_CONDITIONS.includes(k)) : CONDITION_KINDS)
    .filter((k) => k !== 'support' || hasPairs || conditions.some((c) => conditionKind(c) === 'support'))
    .filter((k) => k !== 'hasSkill' || story.doc.skills?.length)
    .filter((k) => k !== 'hasCondition' || story.doc.conditions?.length || conditions.some((c) => conditionKind(c) === 'hasCondition'))
  const patch = (i: number, p: Record<string, unknown>, key?: string) => onChange(listPatch(conditions, i, p), key && `${key}:${i}`)
  return (
    <div className="story-list conditions" data-testid={`conditions-${name}`}>
      {conditions.length === 0 && <p className="help">{S.noConditions}</p>}
      {conditions.map((c, i) => {
        const kind = conditionKind(c)
        return (
          <div className="story-row condition" role="group" aria-label={`${name}: ${S.conditionN(i + 1)}`} key={i}>
            <div className="story-row-head">
              <Field label={S.conditionKind}>
                <Select value={kind ?? 'switch'} options={kinds.map((k) => ({ value: k, label: S.conditionKinds[k] }))}
                  onChange={(k) => onChange(listReplace(conditions, i, newCondition(story, k as ConditionKind)))} />
              </Field>
              <RowButtons index={i} count={conditions.length} up={`Move ${S.conditionN(i + 1)} up`} down={`Move ${S.conditionN(i + 1)} down`} del={`Delete ${S.conditionN(i + 1)}`}
                onUp={() => onChange(listMove(conditions, i, i - 1))} onDown={() => onChange(listMove(conditions, i, i + 1))} onDelete={() => onChange(listRemove(conditions, i))} />
            </div>
            {kind === 'switch' && (
              <>
                <SwitchPick label={S.conditionSwitch} value={c.switch ?? ''} onChange={(v) => patch(i, { switch: v })} />
                <PickSelect label={S.conditionIs} value={c.is === false ? 'off' : 'on'} options={[{ value: 'on', label: S.on }, { value: 'off', label: S.off }]}
                  onChange={(v) => patch(i, { is: v === 'off' ? false : undefined })} />
              </>
            )}
            {kind === 'variable' && (
              <>
                <VariablePick label={S.conditionVariable} value={c.variable ?? ''} onChange={(v) => patch(i, { variable: v })} />
                <PickSelect label={S.conditionCompare} value={c.op ?? '='} options={OPS} onChange={(v) => patch(i, { op: v as CompareOp })} />
                <Field label={S.conditionValue}><NumberInput value={c.value ?? 0} onChange={(v) => { if (v !== undefined) patch(i, { value: v }, 'value') }} /></Field>
              </>
            )}
            {(kind === 'alive' || kind === 'dead') && story.scene && (
              <UnitPick label={S.conditionWho} value={typeof c[kind] === 'string' ? (c[kind] as string) : ''} onChange={(v) => patch(i, { [kind]: v })} />
            )}
            {(kind === 'alive' || kind === 'dead') && !story.scene && (
              <FilterPicker label={S.conditionWho} value={asFilter(c[kind])} onChange={(f) => patch(i, { [kind]: f ?? {} })} />
            )}
            {kind === 'round' && (
              <>
                <PickSelect label={S.conditionCompare} value={c.round?.op ?? '>='} options={OPS} onChange={(v) => patch(i, { round: { op: v as CompareOp, value: c.round?.value ?? 1 } })} />
                <Field label={S.conditionRound}>
                  <NumberInput value={c.round?.value ?? 1} min={0} onChange={(v) => { if (v !== undefined) patch(i, { round: { op: c.round?.op ?? '>=', value: v } }, 'round') }} />
                </Field>
              </>
            )}
            {kind === 'support' && (
              <>
                <PairPick value={c.support} onChange={(pair) => patch(i, { support: pair })} />
                <PickSelect label={S.supportLevelCompare} value={c.op ?? '>='} options={OPS} onChange={(v) => patch(i, { op: v as CompareOp })} />
                <Field label={S.supportLevelNumber} help={S.supportLevelNumberHelp}>
                  <NumberInput value={c.value ?? 1} min={0} max={5} onChange={(v) => { if (v !== undefined) patch(i, { value: v }, 'value') }} />
                </Field>
              </>
            )}
            {kind === 'researched' && (
              <>
                <TechPick label={S.conditionTech} value={c.researched ?? ''} onChange={(v) => patch(i, { researched: v })} />
                <SidePick label={S.conditionTechSide} value={c.side ?? ''} onChange={(v) => patch(i, { side: v })} />
              </>
            )}
            {kind === 'sees' && (
              <>
                <FilterPicker label={S.conditionSees} value={c.sees} onChange={(f) => patch(i, { sees: f ?? {} })} />
                <TeamPick label={S.conditionSeesTeam} value={c.team ?? ''} onChange={(v) => patch(i, { team: v })} help={S.conditionSeesHelp} />
              </>
            )}
            {kind === 'hasSkill' && (
              <>
                <SkillPick label={S.conditionSkill} value={c.hasSkill ?? ''} onChange={(v) => patch(i, { hasSkill: v })} />
                <FilterPicker label={S.conditionSkillWho} mode="optional" value={c.unit} onChange={(f) => patch(i, { unit: f })} />
                <p className="help">{S.conditionSkillHelp}</p>
              </>
            )}
            {kind === 'hasCondition' && (
              <>
                <ConditionPick label={S.conditionHasCondition} value={c.hasCondition ?? ''} onChange={(v) => patch(i, { hasCondition: v })} />
                <FilterPicker label={S.conditionHasConditionWho} mode="optional" value={c.unit} onChange={(f) => patch(i, { unit: f })} />
                <p className="help">{S.conditionHasConditionHelp}</p>
              </>
            )}
            {kind === 'actor' && (
              <>
                <FilterPicker label={EX.actorWho} value={c.actor} onChange={(f) => patch(i, { actor: f ?? {} })} />
                <p className="help">{EX.actorHelp}</p>
              </>
            )}
            {kind === 'carries' && (
              <>
                <ItemPick label={S.conditionItem} value={c.carries ?? ''} onChange={(v) => patch(i, { carries: v })} />
                <FilterPicker label={S.conditionCarrier} mode="optional" value={c.unit} onChange={(f) => patch(i, { unit: f })} />
              </>
            )}
          </div>
        )
      })}
      <Select aria-label={`${name}: ${S.addCondition}`} value="" placeholder={S.addCondition} options={kinds.map((k) => ({ value: k, label: S.conditionKinds[k] }))}
        onChange={(k) => onChange(listAdd(conditions, newCondition(story, k as ConditionKind)))} />
    </div>
  )
}
