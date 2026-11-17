// "Extra weapon groups" of a character (docs/SRPG.md section 7, "A character's own groups"): kinds of weapon or staff this person can use
// in addition to their class's. It never removes one the class has. Shown when weapons are carried or the game has staves (as on a unit
// type), or when the character already has some. A group's starting rank is set on "Personal ranks" right below.
import { setCharacterWields } from '../../../model/characters'
import { staffItems } from '../../../model/gear'
import { carriedWeapons } from '../../../model/itemRules'
import { groupsOf } from '../../../model/ranks'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { MultiSelect } from '../../../ui/MultiSelect'
import { W } from './wieldsText'

export function CharacterWieldsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  if (!c) return null
  const carried = carriedWeapons(doc)
  const hasStaves = staffItems(doc).length > 0
  if (!carried && !hasStaves && (c.wields?.length ?? 0) === 0) return null
  const own = c.wields ?? []
  const fromClass = doc.units?.find((u) => u.id === c.type)?.wields ?? []
  // Every group a weapon or staff has, a group the class already wields included (ticking it changes nothing, so it is shown as the class's).
  const groups = [...new Set([...groupsOf(doc), ...own])].sort()
  const options = groups.map((g) => ({ value: g, label: fromClass.includes(g) ? W.byClass(g) : g, disabled: fromClass.includes(g) }))
  return (
    <Card title={W.title} help={carried ? (hasStaves ? W.helpBoth : W.help) : hasStaves ? W.helpStaves : W.off} id="character-wields">
      <div className="stack" data-testid="character-wields">
        <MultiSelect label={W.label} emptyText={W.none} options={options} value={own}
          onChange={(v) => edit((d) => setCharacterWields(d, id, v), `character:${id}:wields`)} />
        {fromClass.length > 0 && <p className="help">{W.classHas(fromClass)}</p>}
      </div>
    </Card>
  )
}
