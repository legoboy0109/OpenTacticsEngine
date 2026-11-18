// A small "Worth a look" banner for the editor-side checks of E3 (promotion, carrying). It sits inside the card the check is about,
// so the Units and Items screens needed no change. Nothing is shown when all is well.
import { useMemo } from 'react'
import type { SrpgCheck } from '../../model/abilityChecks'
import { promotionChecks } from '../../model/promotionChecks'
import { promotionLevelChecks } from '../../model/promotionLevels'
import { rescueChecks } from '../../model/rescueChecks'
import type { Section } from '../../model/types'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'

/** `id`: only the checks that name this unit type / item; without it, every check of the section (Rules). */
export function SrpgChecks({ section, id, testId }: { section: Section; id?: string; testId: string }) {
  const doc = useDoc()
  const list = useMemo<SrpgCheck[]>(
    () => [...promotionChecks(doc), ...promotionLevelChecks(doc), ...rescueChecks(doc)].filter((c) => c.section === section && (id === undefined || c.ids?.includes(id))),
    [doc, section, id],
  )
  if (list.length === 0) return null
  return (
    <Banner tone="warn" title="Worth a look">
      <ul className="usage-list" data-testid={testId}>{list.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
    </Banner>
  )
}
