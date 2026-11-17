// A chapter opened from the timeline: its normal form (name, map, next chapter, both scenes) under a "Back to the timeline"
// button. A scene box scrolls to its scene; the timeline comes back as it was (its view, selection and open levels are remembered).
import { useEffect } from 'react'
import { navigate, projectPath } from '../../../router'
import { useDoc, useStore } from '../../../store/context'
import { Icon } from '../../../ui/Icon'
import { ChapterForm } from '../ChapterForm'
import { memoryOf } from './memory'
import { TL } from './text'

export function TimelineChapter({ id }: { id: string }) {
  const store = useStore()
  const doc = useDoc()
  const known = doc.campaign?.chapters.some((c) => c.id === id)
  useEffect(() => {
    const which = memoryOf(store.id).scrollTo
    if (!which) return
    memoryOf(store.id).scrollTo = null
    document.querySelector<HTMLElement>(`[data-testid="scene-${which}"]`)?.scrollIntoView({ block: 'start' })
  }, [store.id, id])
  return (
    <div className="stack" data-testid="timeline-chapter">
      <div><button type="button" className="btn" data-testid="back-to-timeline" onClick={() => navigate(projectPath(store.id, 'campaign'))}><Icon name="back" />{TL.backToTimeline}</button></div>
      {known ? <ChapterForm id={id} /> : <p className="help">{TL.noChapters}</p>}
    </div>
  )
}
