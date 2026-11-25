// "Characters" on the Art screen: pick a character and give it a picture of its own (drawn on the map instead of its unit type's),
// with walking pictures and poses, using the same Picture card as a unit (docs/GRAPHICS.md "Character looks").
import { useState } from 'react'
import { useDoc } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { PictureCard } from '../../ui/picture/PictureCard'
import { T } from './text'

export function CharactersCard() {
  const doc = useDoc()
  const list = doc.characters ?? []
  const [picked, setPicked] = useState('')
  const own = list.filter((c) => !!doc.graphics?.characters?.[c.id]).length
  if (list.length === 0) return <Card title={T.charactersTitle} help={T.charactersHelp} id="characters"><p className="help">{T.charactersNone}</p></Card>
  const sel = list.find((c) => c.id === picked) ?? list[0]
  return (
    <>
      <Card title={T.charactersTitle} help={T.charactersHelp} id="characters">
        <div className="stack tight" data-testid="art-characters">
          <Field label={T.charactersPick}>
            <Select aria-label={T.charactersPick} value={sel.id} options={list.map((c) => ({ value: c.id, label: doc.graphics?.characters?.[c.id] ? `${c.name} (own picture)` : c.name }))} onChange={setPicked} />
          </Field>
          <p className="help">{T.charactersWithPicture(own)}</p>
        </div>
      </Card>
      <PictureCard key={sel.id} kind="characters" id={sel.id} name={sel.name} />
    </>
  )
}
