// A character's dialogue portrait. Portraits live in data/graphics.json under an id, and a character's id is the story
// id its dialogue uses, so this card edits the portrait named by the character's id with the Art screen's own picker.
import { useState } from 'react'
import { sheetsOf } from '../../model/graphics'
import { addPortrait, removePortrait, setPortraitPicture } from '../../model/portraits'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { PortraitPicker } from '../art/PortraitPicker'
import { PortraitThumb } from '../art/PortraitThumb'
import { T } from './text'

export function CharacterPortraitCard({ id, name }: { id: string; name: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const [choosing, setChoosing] = useState(false)
  const portrait = doc.graphics?.portraits?.[id]
  const hasImages = sheetsOf(doc).length > 0
  return (
    <Card title={T.portraitTitle} help={T.portraitHelp}>
      <div className="row wrap" data-testid="character-portrait">
        {portrait && <PortraitThumb sheet={portrait.sheet} cell={portrait.cell} label={T.portraitLabel(name)} height={72} />}
        {!portrait && <p className="help">{hasImages ? T.portraitNone : T.portraitNeedImage}</p>}
        {portrait && <button type="button" className="btn sm" onClick={() => setChoosing(true)}>{T.portraitChoose}</button>}
        {portrait && <button type="button" className="btn sm" onClick={() => edit((d) => removePortrait(d, id))}>{T.portraitRemove}</button>}
        {!portrait && hasImages && <button type="button" className="btn sm" onClick={() => { edit((d) => addPortrait(d, id)); setChoosing(true) }}>{T.portraitAdd}</button>}
      </div>
      {choosing && portrait && (
        <PortraitPicker id={id} current={portrait} onClose={() => setChoosing(false)}
          onChoose={(sheet, cell) => { edit((d) => setPortraitPicture(d, id, sheet, cell)); setChoosing(false) }} />
      )}
    </Card>
  )
}
