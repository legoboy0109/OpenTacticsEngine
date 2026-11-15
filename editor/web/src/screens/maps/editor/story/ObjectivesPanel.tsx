// The "Objectives" tab: what the player has to do on this map. Empty means the game's own defeat rules decide.
import { useEffect, useMemo } from 'react'
import { storyChecks } from '../../../../model/storyChecks'
import { setObjectives } from '../../../../model/storyEdit'
import { objectiveOverlays } from '../../../../model/storyOverlays'
import type { MapViewState } from '../viewState'
import { ObjectiveEditor } from './ObjectiveEditor'
import { StoryChecks } from './StoryChecks'
import { StoryProvider, useStory } from './StoryContext'
import { S } from './text'

export function ObjectivesPanel({ mapId, vs }: { mapId: string; vs: MapViewState }) {
  return <StoryProvider mapId={mapId} vs={vs}><ObjectivesBody /></StoryProvider>
}

function ObjectivesBody() {
  const { doc, map, mapId, vs, apply } = useStory()
  const o = map.objectives
  const checks = useMemo(() => storyChecks(doc, mapId).filter((c) => c.text.startsWith('The objectives')), [doc, mapId])
  // The map draws the areas of the objectives (green to win, red to lose) while this tab is open.
  useEffect(() => { vs.set({ overlays: objectiveOverlays(o) }) }, [vs, o])
  useEffect(() => () => vs.set({ overlays: [] }), [vs])
  const empty = !o || (!o.text && !o.win?.length && !o.lose?.length)
  return (
    <div className="stack tight" data-testid="objectives-panel">
      <h3>{S.objectivesTitle}</h3>
      <p className="help">{S.objectivesHelp}</p>
      <StoryChecks checks={checks} testId="objective-checks" />
      {empty && <p className="help" data-testid="no-objectives">{S.noObjectives}</p>}
      <ObjectiveEditor name="map" value={o ?? {}} onChange={(next, k) => apply((m) => setObjectives(m, next), k && `objectives:${k}`)} />
      <p className="help">{S.objectiveNote}</p>
    </div>
  )
}
