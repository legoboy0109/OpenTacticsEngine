// The Difficulty card of the Rules screen (docs/DIFFICULTY.md): the maker's own list of difficulties, which one is chosen unless the
// player picks, whether it can be changed later, and what each one changes.
import { useState } from 'react'
import {
  CHANGE_RULES, MAX_DIFFICULTIES, addDifficulty, changeRule, defaultDifficulty, difficultyList, moveDifficulty, percentStats, removeDifficulty,
  renameDifficulty, setChangeRule, setDefaultDifficulty, setDifficultyNumber, setDifficultyStat, setDifficultyText, setPercentStats, startDifficulties,
} from '../../model/difficulty'
import { gameDifficultyChecks } from '../../model/difficultyChecks'
import { difficultyUsages } from '../../model/difficultyRefs'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { Field, FieldGroup } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { D } from './difficultyText'
import { EnemyFields } from './EnemyFields'

export function DifficultyCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = difficultyList(doc)
  const stats = doc.game?.stats ?? []
  const [text, setText] = useState('')
  const [deleting, setDeleting] = useState<string | null>(null)
  const target = list.find((d) => d.id === deleting)
  const checks = gameDifficultyChecks(doc)
  const errors = checks.filter((c) => c.level === 'error')
  const hints = checks.filter((c) => c.level === 'hint')
  const add = () => {
    const name = text.trim()
    if (!name) return
    edit((d) => addDifficulty(d, name))
    setText('')
  }
  if (list.length === 0) {
    return (
      <Card title={D.title} help={D.help} id="difficulty-card">
        <div className="stack tight" data-testid="difficulty-none">
          <p className="help">{D.noneHelp}</p>
          <div className="row">
            <button type="button" className="btn" onClick={() => edit((d) => startDifficulties(d, true))}><Icon name="plus" />{D.startThree}</button>
            <button type="button" className="btn ghost" onClick={() => edit((d) => startDifficulties(d, false))}>{D.startOne}</button>
          </div>
          <p className="help">{D.startThreeHelp}</p>
        </div>
      </Card>
    )
  }
  // The stats "stats percent" scales: the game's own choice, else every stat but the movement stat.
  const chosen = percentStats(doc)
  const effective = chosen ?? stats.filter((s) => s.id !== doc.game?.roles.move).map((s) => s.id)
  const togglePercentStat = (id: string, on: boolean) => {
    const next = stats.map((s) => s.id).filter((x) => (x === id ? on : effective.includes(x)))
    const dflt = stats.filter((s) => s.id !== doc.game?.roles.move).map((s) => s.id)
    edit((d) => setPercentStats(d, next.length === dflt.length && next.every((x, i) => x === dflt[i]) ? undefined : next))
  }
  const current = defaultDifficulty(doc)
  const rule = changeRule(doc)
  return (
    <Card title={D.title} help={D.help} id="difficulty-card">
      <div className="stack" data-testid="difficulty-card">
        {errors.length > 0 && <Banner tone="warn" title="Check the difficulties">{errors.map((c) => c.text).join(' ')}</Banner>}
        <Field label={D.defaultLabel} help={D.defaultHelp}>
          <Select value={current} onChange={(v) => edit((d) => setDefaultDifficulty(d, v))} aria-label={D.defaultLabel}
            options={list.map((d) => ({ value: d.id, label: d.name || d.id }))} />
        </Field>
        <FieldGroup label={D.changeLabel} help={D.changeHelp}>
          <div className="kind-row" role="radiogroup" aria-label={D.changeLabel}>
            {CHANGE_RULES.map((r) => (
              <button key={r} type="button" role="radio" aria-checked={rule === r} className="pick pick-inline" onClick={() => edit((d) => setChangeRule(d, r))}>
                <span className="pick-name">{D.change[r].label}</span>
              </button>
            ))}
          </div>
          <p className="help">{D.change[rule].help}</p>
        </FieldGroup>
        <FieldGroup label={D.listTitle} help={D.listHelp}>
          <div className="stack" data-testid="difficulty-list">
            {list.map((d, i) => (
              <div className="difficulty-entry" key={d.id} data-testid={`difficulty-${d.id}`}>
                <div className="difficulty-head">
                  <TextInput value={d.name} aria-label={D.entryName(i + 1)} onChange={(v) => edit((doc2) => renameDifficulty(doc2, d.id, v), `difficulty:${d.id}:name`)} />
                  <button type="button" className="btn ghost sm icon" aria-label={D.moveUp(d.name)} disabled={i === 0} onClick={() => edit((x) => moveDifficulty(x, i, i - 1))}><Icon name="up" /></button>
                  <button type="button" className="btn ghost sm icon" aria-label={D.moveDown(d.name)} disabled={i === list.length - 1} onClick={() => edit((x) => moveDifficulty(x, i, i + 1))}><Icon name="down" /></button>
                  <button type="button" className="btn ghost sm icon" aria-label={D.remove(d.name)} onClick={() => setDeleting(d.id)}><Icon name="trash" /></button>
                </div>
                {d.id === current && <p className="help difficulty-default">{D.isDefault}</p>}
                <details>
                  <summary>{D.open}</summary>
                  <div className="stack tight difficulty-body">
                    <Field label={D.description} help={D.descriptionHelp}>
                      <TextInput value={d.description ?? ''} aria-label={`${D.description}: ${d.name}`} onChange={(v) => edit((x) => setDifficultyText(x, d.id, 'description', v), `difficulty:${d.id}:text`)} />
                    </Field>
                    <EnemyFields name={d.name || d.id} stats={stats} enemy={d.enemy} xpPercent={d.xpPercent} resourcePercent={d.resourcePercent}
                      onLevels={(v) => edit((x) => setDifficultyNumber(x, d.id, 'levels', v), `difficulty:${d.id}:levels`)}
                      onStatsPercent={(v) => edit((x) => setDifficultyNumber(x, d.id, 'statsPercent', v), `difficulty:${d.id}:pct`)}
                      onXp={(v) => edit((x) => setDifficultyNumber(x, d.id, 'xpPercent', v), `difficulty:${d.id}:xp`)}
                      onResource={(v) => edit((x) => setDifficultyNumber(x, d.id, 'resourcePercent', v), `difficulty:${d.id}:res`)}
                      onStat={(s, v) => edit((x) => setDifficultyStat(x, d.id, s, v), `difficulty:${d.id}:stat:${s}`)} />
                    {d.id !== current && <button type="button" className="btn sm" onClick={() => edit((x) => setDefaultDifficulty(x, d.id))}>{D.makeDefault(d.name)}</button>}
                  </div>
                </details>
              </div>
            ))}
            {list.length < MAX_DIFFICULTIES && (
              <div className="add-row">
                <input className="input" aria-label={D.addPlaceholder} placeholder={D.addPlaceholder} value={text} onChange={(e) => setText(e.target.value)}
                  onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); add() } }} autoComplete="off" />
                <button type="button" className="btn sm" disabled={!text.trim()} onClick={add}><Icon name="plus" />{D.addButton}</button>
              </div>
            )}
          </div>
        </FieldGroup>
        {stats.length > 0 && (
          <details>
            <summary>{D.percentStatsTitle}</summary>
            <div className="stack tight difficulty-body">
              <p className="help">{D.percentStatsHelp}</p>
              <div className="check-list">
                {stats.map((s) => <Check key={s.id} label={s.name} checked={effective.includes(s.id)} onChange={(v) => togglePercentStat(s.id, v)} />)}
              </div>
            </div>
          </details>
        )}
        {hints.map((h) => <p className="help" key={h.text}>{h.text}</p>)}
        <p className="help">{D.readByStory}</p>
      </div>
      <DeleteConfirm open={!!target} what={target ? D.deleteTitle(target.name) : ''} usages={target ? difficultyUsages(doc, target.id) : []}
        notes={target && list.length === 1 ? [D.deleteLast] : []}
        onCancel={() => setDeleting(null)} onConfirm={() => { if (target) edit((d) => removeDifficulty(d, target.id)); setDeleting(null) }} />
    </Card>
  )
}
