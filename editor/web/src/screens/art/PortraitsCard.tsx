// "Portraits": the pictures shown beside dialogue. Each has a name (what a Say command's "Portrait" refers to) and a
// piece of one of the game's images. The story names of the units in the maps are offered as quick starts.
import { useState } from 'react'
import { MAX_PORTRAIT_EXPRESSIONS, addPortrait, framesText, movePortrait, parseFrames, portraitIdProblem, portraitList, portraitSuggestions, removePortrait, renamePortrait, setPortraitFacing, setPortraitFrames, setPortraitPicture, type PortraitFacing } from '../../model/portraits'
import { portraitSizeOf, setPortraitSize, sheetsOf } from '../../model/graphics'
import { tidyId } from '../../model/storyIds'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Icon } from '../../ui/Icon'
import { RowButtons } from '../../ui/RowButtons'
import { Field } from '../../ui/Field'
import { IdInput } from '../../ui/IdInput'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { PortraitPicker } from './PortraitPicker'
import { PortraitThumb } from './PortraitThumb'
import { T } from './text'

/** A list of pieces typed as "2,1  3,1"; what cannot be read is said under the box and not saved. */
function FramesField({ label, help, name, value, onCommit, most }: { label: string; help: string; name: string; value: string; onCommit: (cells: Array<[number, number]>) => void; most?: number }) {
  const [text, setText] = useState(value)
  const [error, setError] = useState('')
  const [seen, setSeen] = useState(value)
  if (value !== seen) { setSeen(value); setText(value); setError('') }  // the document changed (Undo, another edit)
  return (
    <Field label={label} help={help}>
      <TextInput aria-label={name} value={text} onChange={(v) => {
        setText(v)
        const r = parseFrames(v, most)
        if ('error' in r) setError(r.error)
        else { setError(''); onCommit(r.cells) }
      }} />
      {error && <p className="error-text" role="alert">{error}</p>}
    </Field>
  )
}

export function PortraitsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = portraitList(doc)
  const size = portraitSizeOf(doc)
  const hasImages = sheetsOf(doc).length > 0
  const [choosing, setChoosing] = useState<string | null>(null)
  const [text, setText] = useState('')
  const problem = text === '' ? null : portraitIdProblem(doc, text)
  const add = (id: string) => { if (!portraitIdProblem(doc, id)) edit((d) => addPortrait(d, id)) }
  const suggestions = portraitSuggestions(doc)
  const target = list.find((p) => p.id === choosing)
  return (
    <Card title={T.portraitsTitle} help={T.portraitsHelp} id="portraits">
      <div className="stack tight" data-testid="portraits">
        <div className="battle-fields" data-testid="portrait-size">
          <Field label={T.portraitWidth} help={T.portraitSizeHelp}>
            <NumberInput aria-label={`${T.portraitSize}: ${T.portraitWidth}`} allowEmpty min={24} max={160} placeholder="48" value={size?.[0]}
              onChange={(v) => edit((d) => setPortraitSize(d, v, v === undefined ? undefined : size?.[1] ?? 48), 'portrait:size:w')} />
          </Field>
          <Field label={T.portraitHeight}>
            <NumberInput aria-label={`${T.portraitSize}: ${T.portraitHeight}`} allowEmpty min={24} max={160} placeholder="48" value={size?.[1]}
              onChange={(v) => edit((d) => setPortraitSize(d, v === undefined ? undefined : size?.[0] ?? 48, v), 'portrait:size:h')} />
          </Field>
        </div>
        {!hasImages && <p className="help">{T.portraitsNeedImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{T.portraitsNone}</p>}
        <ul className="portrait-list" aria-label={T.portraitsTitle}>
          {list.map((p, i) => (
            <li key={p.id} className="portrait-row" data-testid={`portrait-${p.id}`}>
              <PortraitThumb sheet={p.portrait.sheet} cell={p.portrait.cell} label={T.portraitPreviewLabel(p.id)} />
              <div className="portrait-fields">
                <IdInput aria-label={T.portraitName(i + 1)} value={p.id} validate={(t) => portraitIdProblem(doc, t, p.id)} onChange={(v) => edit((d) => renamePortrait(d, p.id, v), `portrait:${p.id}:name`)} />
                <button type="button" className="btn sm" onClick={() => setChoosing(p.id)}>{T.portraitChoose}</button>
                <Field label={T.portraitFaces} help={T.portraitFacesHelp}>
                  <Select aria-label={`${p.id}: ${T.portraitFaces}`} value={p.portrait.faces ?? ''} options={[{ value: '', label: T.portraitFacesNone }, { value: 'left', label: T.portraitFacesLeft }, { value: 'right', label: T.portraitFacesRight }]}
                    onChange={(v) => edit((d) => setPortraitFacing(d, p.id, v === '' ? undefined : (v as PortraitFacing)))} />
                </Field>
                {(['talk', 'blink'] as const).map((key) => (
                  <FramesField key={key} label={key === 'talk' ? T.portraitTalk : T.portraitBlink} help={key === 'talk' ? T.portraitTalkHelp : T.portraitBlinkHelp} name={`${p.id}: ${key === 'talk' ? T.portraitTalk : T.portraitBlink}`}
                    value={framesText(p.portrait[key])} onCommit={(cells) => edit((d) => setPortraitFrames(d, p.id, key, cells), `portrait:${p.id}:${key}`)} />
                ))}
                <FramesField label={T.portraitExpressions} help={T.portraitExpressionsHelp} name={`${p.id}: ${T.portraitExpressions}`} most={MAX_PORTRAIT_EXPRESSIONS}
                  value={framesText(p.portrait.expressions)} onCommit={(cells) => edit((d) => setPortraitFrames(d, p.id, 'expressions', cells), `portrait:${p.id}:expressions`)} />
              </div>
              <RowButtons index={i} count={list.length} up={T.portraitUp(p.id)} down={T.portraitDown(p.id)} del={T.portraitDelete(p.id)}
                onUp={() => edit((d) => movePortrait(d, i, i - 1))} onDown={() => edit((d) => movePortrait(d, i, i + 1))} onDelete={() => edit((d) => removePortrait(d, p.id))} />
            </li>
          ))}
        </ul>
        {hasImages && (
          <>
            {suggestions.length > 0 && (
              <div className="stack tight">
                <p className="help">{T.portraitSuggest}</p>
                <div className="row wrap">
                  {suggestions.map((s) => (
                    <button key={s.id} type="button" className="btn sm" onClick={() => add(s.id)}>{T.portraitSuggestButton(s.label, s.id)}</button>
                  ))}
                </div>
              </div>
            )}
            <div className="add-row">
              <input className="input" aria-label={T.portraitNewName} placeholder={T.portraitNewName} value={text} autoComplete="off" spellCheck={false}
                onChange={(e) => setText(tidyId(e.target.value))}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); if (text && !problem) { add(text); setText('') } } }} />
              <button type="button" className="btn sm" disabled={!text || !!problem} onClick={() => { add(text); setText('') }}><Icon name="plus" />{T.portraitAdd}</button>
            </div>
            {problem && <p className="error-text" role="alert">{problem}</p>}
          </>
        )}
      </div>
      {target && (
        <PortraitPicker id={target.id} current={target.portrait} onClose={() => setChoosing(null)}
          onChoose={(sheet, cell) => { edit((d) => setPortraitPicture(d, target.id, sheet, cell)); setChoosing(null) }} />
      )}
    </Card>
  )
}
