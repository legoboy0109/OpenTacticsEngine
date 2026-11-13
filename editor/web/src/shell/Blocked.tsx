import type { ReactNode } from 'react'
import { sectionLoadProblems } from '../model/problems'
import type { ProjectDoc } from '../model/types'
import { useProject } from '../store/context'
import { Banner } from '../ui/Banner'

type Loadable = 'game' | 'terrain' | 'weapons' | 'units'
const FILE: Record<Loadable, string> = { game: 'game.json', terrain: 'data/terrain.json', weapons: 'data/weapons.json', units: 'data/units.json' }

/** Shows `children` only when every needed part of the project loaded. A part
 *  that did not (null) gets a clear message instead, and is never written back. */
export function Needs({ parts, children }: { parts: Loadable[]; children: ReactNode }) {
  const { doc, problems } = useProject()
  const missing = parts.find((p) => doc[p as keyof ProjectDoc] === null)
  if (!missing) return <>{children}</>
  const file = FILE[missing]
  const why = sectionLoadProblems(problems, file)
  return (
    <div className="blocked">
      <Banner tone="error" title={`This section can't be opened: ${file} could not be read.`}>
        {why.length > 0 ? why.map((p, i) => <p key={i}>{p.message}</p>) : <p>The file is missing or is not valid.</p>}
        <p>Nothing here will be changed or saved. Fix or restore that file (it is in the game folder), then reload this page. The other sections still work.</p>
      </Banner>
    </div>
  )
}
