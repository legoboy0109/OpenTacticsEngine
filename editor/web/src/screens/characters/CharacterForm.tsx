import { useState } from 'react'
import { clone } from '../../model/doc'
import { entryId } from '../../model/itemGrants'
import { inventorySize } from '../../model/itemRules'
import { maxLevel, setCharacterItems, setCharacterLevel, updateCharacter } from '../../model/characters'
import { featureEnabled } from '../../model/rules'
import { navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { allTags } from '../../model/units'
import { ChipsInput } from '../../ui/ChipsInput'
import { Field } from '../../ui/Field'
import { ItemListEditor } from '../../ui/ItemListEditor'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Toggle } from '../../ui/Toggle'
import { CharacterPortraitCard } from './CharacterPortraitCard'
import { CharacterStatsCard } from './CharacterStatsCard'
import { CharacterSrpgCards } from './srpg/CharacterSrpgCards'
import { RenameCharacterDialog } from './RenameCharacterDialog'
import { cap, T } from './text'

export function CharacterForm({ id, word }: { id: string; word: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const [renaming, setRenaming] = useState(false)
  const c = doc.characters?.find((x) => x.id === id)
  if (!c) return null
  const key = `character:${id}`
  const classes = doc.units ?? []
  const base = classes.find((u) => u.id === c.type)
  const items = doc.items ?? []
  const typeItems = (base?.items ?? []).map((e) => items.find((i) => i.id === entryId(e))?.name ?? entryId(e))
  const progression = featureEnabled(doc, 'progression')
  // A class the units list no longer has (a damaged file): keep it selectable so the form never silently changes it.
  const options = classes.some((u) => u.id === c.type) ? classes : [...classes, { id: c.type, name: `${c.type} (missing)` }]

  return (
    <div className="stack">
      <Card>
        <div className="stack">
          <FormGrid>
            <Field label={T.name} help={T.nameHelp}>
              <TextInput value={c.name} onChange={(v) => edit((d) => updateCharacter(d, id, { name: v }), `${key}:name`)} />
            </Field>
            <Field label={cap(word)} help={T.classHelp(word)}>
              <Select value={c.type} options={options.map((u) => ({ value: u.id, label: u.name }))} onChange={(v) => edit((d) => updateCharacter(d, id, { type: v }))} />
            </Field>
            {progression && (
              <Field label={T.level} help={T.levelHelp(maxLevel(doc))}>
                <NumberInput value={c.level} min={1} max={maxLevel(doc)} allowEmpty placeholder="1" onChange={(v) => edit((d) => setCharacterLevel(d, id, v), `${key}:level`)} />
              </Field>
            )}
          </FormGrid>
          <Field label={T.description} help={T.descriptionHelp}>
            <TextInput value={c.description ?? ''} onChange={(v) => edit((d) => updateCharacter(d, id, { description: v.trim() ? v : undefined }), `${key}:description`)} />
          </Field>
        </div>
      </Card>

      <CharacterPortraitCard id={id} name={c.name} />

      <CharacterStatsCard id={id} word={word} />

      <CharacterSrpgCards id={id} />

      <Card>
        <Field label={T.tags} help={T.tagsHelp}>
          <div data-testid="character-tags">
            <ChipsInput value={c.tags ?? []} suggestions={[...new Set([...allTags(doc), ...(doc.characters ?? []).flatMap((x) => x.tags ?? [])])]}
              onChange={(v) => edit((d) => updateCharacter(d, id, { tags: v.length ? v : undefined }))} />
          </div>
        </Field>
      </Card>

      {(items.length > 0 || c.items !== undefined) && (
        <Card>
          <div className="stack tight" data-testid="character-items">
            <Toggle label={T.itemsOwn} help={c.items === undefined ? T.itemsOwnOff(typeItems, word) : T.itemsOwnOn(word)}
              checked={c.items !== undefined} onChange={(v) => edit((d) => setCharacterItems(d, id, v ? clone(base?.items ?? []) : undefined))} />
            {c.items !== undefined && (
              <ItemListEditor label={T.itemsLabel} entries={c.items} items={items} allowDrop max={inventorySize(doc)} testId="character-start-items"
                onChange={(next, k) => edit((d) => setCharacterItems(d, id, next), k && `${key}:${k}`)} />
            )}
          </div>
        </Card>
      )}

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}>
          <div className="row">
            <TextInput value={c.id} readOnly mono />
            <button type="button" className="btn sm" onClick={() => setRenaming(true)}>{T.renameButton}</button>
          </div>
        </Field>
        {renaming && <RenameCharacterDialog id={id} onClose={() => setRenaming(false)} onRenamed={(nid) => { setRenaming(false); navigate(projectPath(store.id, 'characters', nid)) }} />}
      </Advanced>
    </div>
  )
}
