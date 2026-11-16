import { useMemo } from 'react'
import { characterChecks } from '../../model/characterChecks'
import { useDoc, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Link, projectPath } from '../../router'

import { T } from './text'

/** Editor-side checks for characters (the loader's own refusals are in the Problems panel). One line per character,
 *  each a link to the character. Nothing when all is well. */
export function CharacterChecks() {
  const doc = useDoc()
  const store = useStore()
  const list = useMemo(() => characterChecks(doc), [doc])
  if (list.length === 0) return null
  return (
    <Banner tone="info" title={T.checksTitle}>
      <ul className="usage-list" data-testid="character-checks">
        {list.map((c) => (
          <li key={c.characterId}><Link to={projectPath(store.id, 'characters', c.characterId)}>{doc.characters?.find((x) => x.id === c.characterId)?.name ?? c.characterId}</Link>: {c.text}</li>
        ))}
      </ul>
    </Banner>
  )
}
