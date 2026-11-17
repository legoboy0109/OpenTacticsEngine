// "Battle backgrounds": named pictures for the scene, the default one, and (per terrain) in the Terrain section.
import { useState } from 'react'
import { addBackground, backgroundList, defaultBackground, newBackgroundId, removeBackground, setBackgroundPicture, setDefaultBackground } from '../../../model/battleArt'
import { sheetsOf } from '../../../model/graphics'
import { tidyId } from '../../../model/storyIds'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { PortraitPicker } from '../PortraitPicker'
import { PortraitThumb } from '../PortraitThumb'
import { T } from './text'

export function BackgroundsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = backgroundList(doc)
  const [choosing, setChoosing] = useState<string | null>(null)
  const [text, setText] = useState('')
  const hasImages = sheetsOf(doc).length > 0
  const id = newBackgroundId(doc, text || 'scene')
  const add = () => { if (text) { edit((d) => addBackground(d, id)); setText('') } }
  const target = list.find((b) => b.id === choosing)
  return (
    <Card title={T.bgTitle} help={T.bgHelp} id="battle-backgrounds">
      <div className="stack tight" data-testid="battle-backgrounds">
        {!hasImages && <p className="help">{T.bgNeedImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{T.bgNone}</p>}
        <ul className="portrait-list" aria-label={T.bgTitle}>
          {list.map((b) => (
            <li key={b.id} className="portrait-row" data-testid={`background-${b.id}`}>
              <PortraitThumb sheet={b.sheet} cell={b.cell} label={T.bgName(b.id)} height={48} />
              <strong>{b.id}</strong>
              <button type="button" className="btn sm" onClick={() => setChoosing(b.id)}>{T.bgChoose}</button>
              <button type="button" className="btn sm" aria-label={T.bgDelete(b.id)} onClick={() => edit((d) => removeBackground(d, b.id))}><Icon name="trash" />{T.bgDelete(b.id)}</button>
            </li>
          ))}
        </ul>
        {hasImages && (
          <div className="add-row">
            <input className="input" aria-label={T.bgNewName} placeholder={T.bgNewName} value={text} autoComplete="off" spellCheck={false}
              onChange={(e) => setText(tidyId(e.target.value))}
              onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); add() } }} />
            <button type="button" className="btn sm" disabled={!text} onClick={add}><Icon name="plus" />{T.bgAdd}</button>
          </div>
        )}
        {list.length > 0 && (
          <Field label={T.bgDefault}>
            <Select aria-label={T.bgDefault} value={defaultBackground(doc)} options={[{ value: '', label: T.bgNoDefault }, ...list.map((b) => ({ value: b.id, label: b.id }))]}
              onChange={(v) => edit((d) => setDefaultBackground(d, v))} />
          </Field>
        )}
      </div>
      {target && (
        <PortraitPicker id={target.id} current={{ sheet: target.sheet, cell: target.cell }} onClose={() => setChoosing(null)}
          onChoose={(sheet, cell) => { edit((d) => setBackgroundPicture(d, target.id, sheet, cell)); setChoosing(null) }} />
      )}
    </Card>
  )
}
