// "Rename" for a character's internal name (its id): asks for the new one, says what follows and what does not, and renames
// everywhere in one Undo step (model/characterRename.ts).
import { useMemo, useState } from 'react'
import { characterIdProblem, formulaMentions, renameCharacter, renameEffects } from '../../model/characterRename'
import { makeId } from '../../model/ids'
import { tidyId } from '../../model/storyIds'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Dialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

interface Props { id: string; onClose: () => void; onRenamed: (id: string) => void }

export function RenameCharacterDialog({ id, onClose, onRenamed }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  const [text, setText] = useState(id)
  // Computed once per opening: they describe the game as it is now.
  const effects = useMemo(() => renameEffects(doc, id), [doc, id])
  const mentions = useMemo(() => formulaMentions(doc, id), [doc, id])
  if (!c) return null
  const problem = text === id ? null : characterIdProblem(doc, id, text)
  const ok = text !== id && !problem
  const submit = () => {
    if (!ok) return
    edit((d) => renameCharacter(d, id, text).doc)
    onRenamed(text)
  }
  // The name made from the person's name, like a new character gets (it avoids every character and unit story name already used).
  const suggest = () => {
    const taken = [...(doc.characters ?? []).map((x) => x.id).filter((x) => x !== id)]
    for (const m of Object.values(doc.maps)) for (const u of m?.units ?? []) if (typeof u.id === 'string') taken.push(u.id)
    setText(makeId(c.name, taken, { fallback: 'character' }))
  }
  return (
    <Dialog open title={T.renameTitle(c.name)} onClose={onClose} onSubmit={submit}
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>Cancel</button>
          <button type="submit" className="btn primary" disabled={!ok}>{T.renameConfirm}</button>
        </>
      )}>
      <div className="stack tight" data-testid="rename-character">
        <Field label={T.renameLabel} help={T.renameHelp} error={problem}>
          <TextInput value={text} mono onChange={(v) => setText(tidyId(v))} autoFocus />
        </Field>
        <div className="row">
          <button type="button" className="btn sm" onClick={suggest}>{T.renameSuggest}</button>
        </div>
        {effects.length > 0 && (
          <div data-testid="rename-follows">
            <p>{T.renameFollows}</p>
            <ul className="usage-list">{effects.map((l, i) => <li key={i}>{l}</li>)}</ul>
          </div>
        )}
        {mentions.length > 0 && (
          <Banner tone="warn" title={T.renameFormulasTitle}>
            <ul className="usage-list">{mentions.map((l, i) => <li key={i}>{l}</li>)}</ul>
            <span>{T.renameFormulasHelp}</span>
          </Banner>
        )}
        <Banner tone="info" title={T.renameSavesTitle}>{T.renameSaves}</Banner>
        <p className="help">{T.renameUndo}</p>
      </div>
    </Dialog>
  )
}
