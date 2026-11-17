import { useMemo } from 'react'
import { placeChecks } from '../../model/placeChecks'
import type { Section } from '../../model/types'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'

/** What the loader would refuse about doors, chests, places and keys on this screen's part of the game, and what is probably not meant,
 *  in plain words (nothing when all is well). Same style as ItemChecks. */
export function PlaceChecks({ section }: { section: Section }) {
  const doc = useDoc()
  const list = useMemo(() => placeChecks(doc).filter((c) => c.section === section && c.mapId === undefined), [doc, section])
  if (list.length === 0) return null
  return (
    <Banner tone="warn" title="Worth a look">
      <ul className="usage-list" data-testid="place-checks">{list.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
    </Banner>
  )
}
