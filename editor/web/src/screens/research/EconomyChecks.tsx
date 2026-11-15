import { useMemo } from 'react'
import { economyChecks } from '../../model/economyChecks'
import { fogChecks } from '../../model/fogChecks'
import type { Section } from '../../model/types'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { T } from './text'

/** What the loader would refuse about research, upgrades, repair, fuel, ammunition, supply and fog of war on this screen's part of the
 *  game, in plain words (nothing when all is well). Same style as ItemChecks. */
export function EconomyChecks({ section }: { section: Section }) {
  const doc = useDoc()
  const list = useMemo(() => [...economyChecks(doc), ...fogChecks(doc)].filter((c) => c.section === section), [doc, section])
  if (list.length === 0) return null
  return (
    <Banner tone="warn" title={T.checksTitle}>
      <ul className="usage-list" data-testid="economy-checks">{list.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
    </Banner>
  )
}
