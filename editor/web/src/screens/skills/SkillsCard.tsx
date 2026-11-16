// The Skills card shared by unit types, characters, weapons and accessories: the skills it always gives (a list with a picker
// to add) and, where the owner can level up, "learns at a level" rows. The owners only differ in which keys they write, so
// each has a thin file (UnitSkillsCard, CharacterSkillsCard, ItemSkillsCard, WeaponSkillsCard) that passes its edit in.
import { skillName } from '../../model/skills'
import { maxLevel } from '../../model/characters'
import type { Learn } from '../../model/srpgTypes'
import { moveItem } from '../../model/util'
import { Link, projectPath } from '../../router'
import { useDoc, useStore } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { Num } from './Num'
import { T } from './text'
import './skills.css'

interface Props {
  /** Test id and name prefix: "unit-skills", "character-skills" ... */
  testId: string
  help: string
  skills: string[] | undefined
  onSkills: (next: string[] | undefined, key?: string) => void
  /** Owners that can learn skills by level pass both. */
  learns?: Learn[] | undefined
  onLearns?: (next: Learn[] | undefined, key?: string) => void
}

export function SkillsCard({ testId, help, skills, onSkills, learns, onLearns }: Props) {
  const doc = useDoc()
  const store = useStore()
  const defined = doc.skills ?? []
  const have = skills ?? []
  const addable = defined.filter((s) => !have.includes(s.id))
  const set = (next: string[]) => onSkills(next.length ? next : undefined)

  return (
    <Card title={T.cardTitle} help={help} id={testId} learn={testId}>
      <div className="item-rows" data-testid={testId}>
        {have.length === 0 && <p className="help">{defined.length === 0 ? T.cardNoneDefined : T.cardNone}</p>}
        {have.map((sid, i) => {
          const known = defined.some((s) => s.id === sid)
          const name = skillName(doc, sid)
          return (
            <div className="item-row" key={sid}>
              <span className="grow">{known ? <Link to={projectPath(store.id, 'skills', sid)}>{name}</Link> : T.cardGone(sid)}</span>
              <RowButtons index={i} count={have.length} up={T.cardMoveUp(name)} down={T.cardMoveDown(name)} del={T.cardRemove(name)}
                onUp={() => set(moveItem(have, i, i - 1))} onDown={() => set(moveItem(have, i, i + 1))} onDelete={() => set(have.filter((x) => x !== sid))} />
            </div>
          )
        })}
        {addable.length > 0 && (
          <Select data-learn={`${testId}-add`} aria-label={`${T.cardTitle}: ${T.cardAdd}`} value="" placeholder={T.cardAdd} options={addable.map((s) => ({ value: s.id, label: s.name }))}
            onChange={(v) => set([...have, v])} />
        )}
        {onLearns && <LearnRows learns={learns} onLearns={onLearns} testId={testId} />}
      </div>
    </Card>
  )
}

function LearnRows({ learns, onLearns, testId }: { learns: Learn[] | undefined; onLearns: (next: Learn[] | undefined, key?: string) => void; testId: string }) {
  const doc = useDoc()
  const defined = doc.skills ?? []
  const rows = learns ?? []
  const progression = doc.game?.rules?.progression?.enabled === true
  const top = maxLevel(doc)
  const set = (next: Learn[], key?: string) => onLearns(next.length ? next : undefined, key)
  // The loader refuses levels outside 2..max level, and levels only exist when the game levels up at all.
  if (!progression && rows.length === 0) return <><h4 className="skill-part">{T.learnsTitle}</h4><p className="help">{T.learnsNoLevels}</p></>
  return (
    <div className="stack" data-testid={`${testId}-learns`} data-learn={`${testId}-learns`}>
      <h4 className="skill-part">{T.learnsTitle}</h4>
      {rows.length === 0 && <p className="help">{T.learnsNone}</p>}
      {rows.map((l, i) => {
        const n = i + 1
        const options = defined.map((s) => ({ value: s.id, label: s.name }))
        if (!options.some((o) => o.value === l.skill)) options.push({ value: l.skill, label: T.cardGone(l.skill) })
        return (
          <div className="learn-row" role="group" aria-label={T.learnN(n)} key={i}>
            <Field label={T.learnLevel}><Num value={l.level} min={2} max={top} onChange={(v) => set(rows.map((x, j) => (j === i ? { ...x, level: v } : x)), `learn:${i}`)} /></Field>
            <Field label={T.learnSkill}><Select value={l.skill} options={options} onChange={(v) => set(rows.map((x, j) => (j === i ? { ...x, skill: v } : x)))} /></Field>
            <button type="button" className="btn ghost sm" aria-label={T.learnRemove(n)} onClick={() => set(rows.filter((_, j) => j !== i))}>Remove</button>
          </div>
        )
      })}
      {progression && defined.length > 0 && (
        <div><button type="button" className="btn sm" onClick={() => set([...rows, { level: Math.min(top, Math.max(2, ...rows.map((r) => r.level + 1))), skill: defined[0].id }])}>{T.learnsAdd}</button></div>
      )}
    </div>
  )
}
