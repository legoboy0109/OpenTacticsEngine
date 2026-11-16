// The "Personal stat caps" card of a character (docs/SRPG.md section 7): a plus or minus per stat, added to the cap of the class (or the
// game). The Result column shows the cap the character ends up with. Shown once the game has stat caps (or the character has some).
import { updateCharacter } from '../../../model/characters'
import { classCap, setCapIn } from '../../../model/ranks'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { CapRows } from '../../ranks/CapRows'
import { R } from '../../ranks/text'

export function CharacterCapsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  if (!c || (doc.caps === null && !c.capAdjust)) return null
  return (
    <Card title={R.charCapsTitle} help={R.charCapsHelp} id="character-caps">
      <CapRows testId="character-caps" mode="adjust" values={c.capAdjust} base={(s) => classCap(doc, c.type, s)}
        onChange={(s, v) => edit((d) => updateCharacter(d, id, { capAdjust: setCapIn(d.characters?.find((x) => x.id === id)?.capAdjust, s, v) }), `character:${id}:cap:${s}`)} />
    </Card>
  )
}
