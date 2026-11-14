import { useMemo } from 'react'
import { itemChecks } from '../../model/itemChecks'
import type { Section } from '../../model/types'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { T } from './text'

/** What the loader would refuse about items on this screen's part of the game, in plain words (nothing when all is well). */
export function ItemChecks({ section }: { section: Section }) {
  const doc = useDoc()
  const list = useMemo(() => itemChecks(doc).filter((c) => c.section === section), [doc, section])
  if (list.length === 0) return null
  return (
    <Banner tone="warn" title={T.checksTitle}>
      <ul className="usage-list" data-testid="item-checks">{list.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
    </Banner>
  )
}
