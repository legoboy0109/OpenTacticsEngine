// "Portraits": the pictures shown beside dialogue. Each has a name (what a Say command's "Portrait" refers to) and a
// piece of one of the game's images. The story names of the units in the maps are offered as quick starts.
import { useState } from 'react'
import { addPortrait, movePortrait, portraitIdProblem, portraitList, portraitSuggestions, removePortrait, renamePortrait, setPortraitPicture } from '../../model/portraits'
import { sheetsOf } from '../../model/graphics'
import { tidyId } from '../../model/storyIds'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Icon } from '../../ui/Icon'
import { RowButtons } from '../../ui/RowButtons'
import { IdInput } from '../../ui/IdInput'
import { PortraitPicker } from './PortraitPicker'
import { PortraitThumb } from './PortraitThumb'
import { T } from './text'

export function PortraitsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = portraitList(doc)
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
        {!hasImages && <p className="help">{T.portraitsNeedImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{T.portraitsNone}</p>}
        <ul className="portrait-list" aria-label={T.portraitsTitle}>
          {list.map((p, i) => (
            <li key={p.id} className="portrait-row" data-testid={`portrait-${p.id}`}>
              <PortraitThumb sheet={p.portrait.sheet} cell={p.portrait.cell} label={T.portraitPreviewLabel(p.id)} />
              <div className="portrait-fields">
                <IdInput aria-label={T.portraitName(i + 1)} value={p.id} validate={(t) => portraitIdProblem(doc, t, p.id)} onChange={(v) => edit((d) => renamePortrait(d, p.id, v), `portrait:${p.id}:name`)} />
                <button type="button" className="btn sm" onClick={() => setChoosing(p.id)}>{T.portraitChoose}</button>
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
