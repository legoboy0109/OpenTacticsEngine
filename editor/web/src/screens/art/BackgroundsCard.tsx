// "Backgrounds": the full-screen pictures of story scenes. Each has a name (what a "Show a background" command refers to) and a
// picture: the whole of one of the game's images, or one piece of it. Deleting one that commands use asks first and takes those
// commands out with it, all in one undo step.
import { useState } from 'react'
import { addBackground, backgroundIdProblem, backgroundList, backgroundUsageLines, moveBackground, removeBackground, renameBackground, setBackgroundPicture } from '../../model/backgrounds'
import { sheetsOf } from '../../model/graphics'
import { tidyId } from '../../model/storyIds'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { Icon } from '../../ui/Icon'
import { IdInput } from '../../ui/IdInput'
import { RowButtons } from '../../ui/RowButtons'
import { BackgroundPicker } from './BackgroundPicker'
import { BackgroundThumb } from './BackgroundThumb'
import { T } from './text'

export function BackgroundsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = backgroundList(doc)
  const hasImages = sheetsOf(doc).length > 0
  const [choosing, setChoosing] = useState<string | null>(null)
  const [removing, setRemoving] = useState<string | null>(null)
  const [text, setText] = useState('')
  const problem = text === '' ? null : backgroundIdProblem(doc, text)
  const add = (id: string) => { if (!backgroundIdProblem(doc, id)) edit((d) => addBackground(d, id)) }
  const target = list.find((b) => b.id === choosing)
  const uses = removing ? backgroundUsageLines(doc, [removing]) : []
  const remove = (id: string) => { edit((d) => removeBackground(d, id)); setRemoving(null) }
  return (
    <Card title={T.backgroundsTitle} help={T.backgroundsHelp} id="backgrounds">
      <div className="stack tight" data-testid="backgrounds">
        {!hasImages && <p className="help">{T.backgroundsNeedImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{T.backgroundsNone}</p>}
        <ul className="portrait-list" aria-label={T.backgroundsTitle}>
          {list.map((b, i) => (
            <li key={b.id} className="portrait-row" data-testid={`background-${b.id}`}>
              <BackgroundThumb background={b.background} label={T.backgroundPreviewLabel(b.id)} />
              <div className="portrait-fields">
                <IdInput aria-label={T.backgroundName(i + 1)} value={b.id} validate={(t) => backgroundIdProblem(doc, t, b.id)} onChange={(v) => edit((d) => renameBackground(d, b.id, v), `background:${b.id}:name`)} />
                <button type="button" className="btn sm" onClick={() => setChoosing(b.id)}>{T.backgroundChoose}</button>
              </div>
              <RowButtons index={i} count={list.length} up={T.backgroundUp(b.id)} down={T.backgroundDown(b.id)} del={T.backgroundDelete(b.id)}
                onUp={() => edit((d) => moveBackground(d, i, i - 1))} onDown={() => edit((d) => moveBackground(d, i, i + 1))}
                onDelete={() => { if (backgroundUsageLines(doc, [b.id]).length) setRemoving(b.id); else remove(b.id) }} />
            </li>
          ))}
        </ul>
        {hasImages && (
          <>
            <div className="add-row">
              <input className="input" aria-label={T.backgroundNewName} placeholder={T.backgroundNewName} value={text} autoComplete="off" spellCheck={false}
                onChange={(e) => setText(tidyId(e.target.value))}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); if (text && !problem) { add(text); setText('') } } }} />
              <button type="button" className="btn sm" disabled={!text || !!problem} onClick={() => { add(text); setText('') }}><Icon name="plus" />{T.backgroundAdd}</button>
            </div>
            {problem && <p className="error-text" role="alert">{problem}</p>}
          </>
        )}
      </div>
      {target && (
        <BackgroundPicker id={target.id} current={target.background} onClose={() => setChoosing(null)}
          onChoose={(sheet, cell) => { edit((d) => setBackgroundPicture(d, target.id, sheet, cell)); setChoosing(null) }} />
      )}
      <ConfirmDialog open={!!removing} title={T.backgroundDeleteTitle(removing ?? '')} confirmLabel={T.backgroundDeleteConfirm} danger
        onConfirm={() => removing && remove(removing)} onCancel={() => setRemoving(null)}>
        <p>{T.backgroundDeleteIntro}</p>
        <ul className="usage-list">{uses.map((u, i) => <li key={i}>{u.text}</li>)}</ul>
      </ConfirmDialog>
    </Card>
  )
}
