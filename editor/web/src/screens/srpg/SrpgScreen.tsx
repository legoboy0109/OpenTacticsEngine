// The frame of the four strategy-RPG screens (Characters, Skills, Abilities, Supports). E1 to E4 each put their screen
// in `children`; the frame decides whether there is anything to show: the feature is switched off (a link to Rules), the
// file could not be read (the loader's words), or the screen itself.
import type { ReactNode } from 'react'
import type { SrpgFeature } from '../../model/srpgTypes'
import { srpgFeatureOn } from '../../model/srpgFeatures'
import { Link, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useProject, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { PageHeader } from '../../ui/Card'
import { T } from './text'

export const SRPG_FILE: Record<SrpgFeature, string> = {
  characters: 'data/characters.json', skills: 'data/skills.json', abilities: 'data/abilities.json', conditions: 'data/conditions.json', supports: 'data/supports.json',
}

export function SrpgScreen({ feature, children }: { feature: SrpgFeature; children?: ReactNode }) {
  const { title, intro } = T.screens[feature]
  return (
    <div className="screen">
      <PageHeader title={title} help={intro} />
      <Needs parts={['game']}><Gate feature={feature} title={title}>{children}</Gate></Needs>
    </div>
  )
}

function Gate({ feature, title, children }: { feature: SrpgFeature; title: string; children?: ReactNode }) {
  const doc = useDoc()
  const { problems } = useProject()
  const store = useStore()
  if (srpgFeatureOn(doc, feature)) return <>{children ?? <p className="help" data-testid="srpg-empty">{T.notAvailableYet}</p>}</>
  // Null on screen means either "switched off" or "the file could not be read": editing the second would overwrite it.
  const bad = problems.filter((p) => p.file === SRPG_FILE[feature])
  if (bad.length > 0) {
    return (
      <Banner tone="error" title={T.unreadable(title, SRPG_FILE[feature])}>
        {bad.map((p, i) => <p key={i}>{p.message}</p>)}
        <p>{T.unreadableHelp}</p>
      </Banner>
    )
  }
  return (
    <div className="blocked" data-testid="srpg-off">
      <p>{T.off(title)}</p>
      <p className="help">{T.offHelp} <Link to={projectPath(store.id, 'rules')}>{T.rulesLink}</Link></p>
    </div>
  )
}
