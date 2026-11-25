// The amber banner of mistakes in the newer art keys that the game would refuse (model/artChecks.ts): characters the game does not
// have, poses and frames outside their image, a unit offset or portrait size out of range.
import { useMemo } from 'react'
import { useArtState } from '../../art/useArt'
import { artChecks } from '../../model/artChecks'
import { useDoc, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'

export function ArtChecks() {
  const doc = useDoc()
  const store = useStore()
  const { version } = useArtState()
  // eslint-disable-next-line react-hooks/exhaustive-deps -- `version` changes when an image's size becomes known
  const list = useMemo(() => artChecks(doc, (f) => store.art.fileInfo(f)), [doc.graphics, doc.characters, doc.game, store, version])
  if (list.length === 0) return null
  return (
    <Banner tone="warn" title="The game will not load until these are fixed">
      <ul className="usage-list" data-testid="art-checks">{list.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
    </Banner>
  )
}
