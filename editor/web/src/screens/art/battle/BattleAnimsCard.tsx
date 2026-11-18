// "Battle animations" on the Art screen: the animation sets of fighters, who uses which, and the scene's backgrounds
// (docs/BATTLE_SCENE.md; docs/EDITOR_UI.md "Art").
import { useState } from 'react'
import { useArtState } from '../../../art/useArt'
import { addSet, newSetId, setIdProblem, setList } from '../../../model/battleArt'
import { battleChecks } from '../../../model/battleChecks'
import { sheetsOf } from '../../../model/graphics'
import { tidyId } from '../../../model/storyIds'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { AssignCard } from './AssignCard'
import { BackgroundsCard } from './BackgroundsCard'
import { PlatformsCard } from './PlatformsCard'
import { StageCard } from './StageCard'
import { SetEditor } from './SetEditor'
import { T } from './text'

export function BattleAnimsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  useArtState()
  const sets = setList(doc)
  const [picked, setPicked] = useState('')
  const [text, setText] = useState('')
  const current = sets.find((s) => s.id === picked) ?? sets[0]
  const problem = text === '' ? null : setIdProblem(doc, text)
  const hasImages = sheetsOf(doc).length > 0
  const checks = battleChecks(doc, (file) => store.art.fileInfo(file))
  const add = (name: string) => {
    const id = newSetId(doc, name)
    if (setIdProblem(doc, id)) return
    edit((d) => addSet(d, id))
    setPicked(id)
    setText('')
  }
  return (
    <>
      <Card title={T.title} help={T.help} id="battle-animations" className="battle-card">
        <div className="stack" data-testid="battle-animations">
          {!hasImages && <p className="help">{T.needImage}</p>}
          {hasImages && sets.length === 0 && <p className="help">{T.none}</p>}
          {sets.length > 0 && (
            <Field label={T.setPick}>
              <Select aria-label={T.setPick} value={current?.id ?? ''} options={sets.map((s) => ({ value: s.id, label: s.id }))} onChange={setPicked} />
            </Field>
          )}
          {hasImages && (
            <div className="add-row">
              <input className="input" aria-label={T.newName} placeholder={T.newName} value={text} autoComplete="off" spellCheck={false}
                onChange={(e) => setText(tidyId(e.target.value))}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); if (text && !problem) add(text) } }} />
              <button type="button" className="btn sm" disabled={!text || !!problem} onClick={() => add(text)}><Icon name="plus" />{T.add}</button>
            </div>
          )}
          {problem && <p className="error-text" role="alert">{problem}</p>}
          {current && <SetEditor key={current.id} id={current.id} set={current.set} onGone={() => setPicked('')} />}
          {checks.length > 0 && (
            <div className="stack tight" data-testid="battle-checks">
              {checks.map((c, i) => <p key={i} className={c.blocks ? 'error-text' : 'help'} role={c.blocks ? 'alert' : undefined}>{c.blocks ? `${T.blocked}: ` : `${T.checks}: `}{c.text}</p>)}
            </div>
          )}
        </div>
      </Card>
      <AssignCard />
      <BackgroundsCard />
      <PlatformsCard />
      <StageCard />
    </>
  )
}
