// "Promotion" card of a character (docs/SRPG.md "A promotion level per character"): the lowest level at which this character may promote, in place
// of the level its class's promotion says. Shown when the game has levels, or the character already has one.
import { characterPromotionLevel, promotionLevelChecks, setCharacterPromotionLevel } from '../../../model/promotionLevels'
import { maxLevelOf, progressionOn } from '../../../model/promotion'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { C } from './promotionText'

export function CharacterPromotionCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  if (!c || (!progressionOn(doc) && c.promotion === undefined)) return null
  const own = promotionLevelChecks(doc).filter((x) => x.section === 'characters' && x.ids?.includes(id))
  return (
    <Card title={C.title} help={C.help} id="character-promotion">
      <div className="stack" data-testid="character-promotion">
        <Field label={C.fromLevel} help={C.fromLevelHelp}>
          <NumberInput value={characterPromotionLevel(doc, id)} allowEmpty min={1} max={maxLevelOf(doc)} placeholder={C.same}
            onChange={(v) => edit((d) => setCharacterPromotionLevel(d, id, v), `character:${id}:promotionLevel`)} />
        </Field>
        {own.map((x) => <Banner key={x.text} tone="warn" title={x.text} />)}
      </div>
    </Card>
  )
}
