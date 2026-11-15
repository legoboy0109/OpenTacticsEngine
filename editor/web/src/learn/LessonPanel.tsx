// The lesson panel: docked at the side on wide windows, a bottom sheet on narrow ones (both can fold into a slim bar).
// It stays while a lesson runs, across screens and reloads. Layout room is made by CSS on <html data-learn-layout>.
import { useEffect, useRef } from 'react'
import { SECTION_LABEL } from '../shell/Sidebar'
import { useMediaQuery, useStored } from '../ui/hooks'
import { Icon } from '../ui/Icon'
import { Highlight } from './Highlight'
import { learn } from './instance'
import { LESSONS } from './lessons'
import { currentStep, isAtEnd, marks } from './runner'
import { lessonStatus, runningLessonId } from './state'
import { T } from './text'
import { useLearn } from './useLearn'
import type { Lesson, Step } from './types'

/** Wide enough for a side panel next to the screen's content; below this the panel is a bottom sheet. */
const DOCK_QUERY = '(min-width: 1000px)'

export function LessonPanel() {
  const snap = useLearn()
  const wide = useMediaQuery(DOCK_QUERY)
  const [collapsed, setCollapsed] = useStored('ote.learnCollapsed', false)
  const el = useRef<HTMLElement>(null)
  const id = runningLessonId(snap.state)
  const lesson = id ? LESSONS.find((l) => l.id === id) : undefined
  const progress = id ? snap.state.lessons[id] : undefined
  const shown = !!lesson && !!progress

  // Make room: the screen's content gives up the space the panel takes (never covered by it).
  useEffect(() => {
    const root = document.documentElement
    if (!shown) { delete root.dataset.learnLayout; root.style.removeProperty('--learn-h'); return }
    root.dataset.learnLayout = collapsed || !wide ? 'sheet' : 'dock'
    return () => { delete root.dataset.learnLayout; root.style.removeProperty('--learn-h') }
  }, [shown, collapsed, wide])
  useEffect(() => {
    const node = el.current
    if (!node) return
    const root = document.documentElement
    const measure = () => root.style.setProperty('--learn-h', `${node.offsetHeight}px`)
    measure()
    const ro = new ResizeObserver(measure)
    ro.observe(node)
    return () => ro.disconnect()
  }, [shown, collapsed, wide])

  if (!lesson || !progress || !id) return null
  const inPractice = snap.projectId === progress.game
  const atEnd = isAtEnd(lesson, progress)
  const step = currentStep(lesson, progress)
  const gone = snap.gone === id
  const onScreen = inPractice && (!step?.where || snap.section === step.where.section)
  // The control to ring: the step's own once its screen is open, otherwise the sidebar link that gets there.
  const target = !inPractice || atEnd || gone ? undefined : onScreen || !step?.where ? step?.point : `nav-${step.where.section}`
  const doneCount = marks(lesson, progress).filter((m) => m === 'done' || m === 'skipped').length
  const bar = gone ? T.goneTitle : atEnd ? T.endTitle : `${T.stepOf(progress.step + 1, lesson.steps.length)}: ${step?.title ?? ''}`

  return (
    <>
      <Highlight name={collapsed ? undefined : target} />
      <aside ref={el} className="learn-panel" data-collapsed={collapsed || undefined} data-wide={wide || undefined} aria-label={T.panelLabel}>
        {collapsed ? (
          <button type="button" className="learn-bar" aria-expanded="false" onClick={() => setCollapsed(false)} title={T.show}>
            <span className="learn-bar-text">{bar}</span>
            <span className="learn-bar-show">{T.barShow}<Icon name="up" /></span>
          </button>
        ) : (
          <>
            <header className="learn-head">
              <div className="learn-head-text">
                <p className="learn-lesson">{lesson.title}</p>
                <p className="learn-progress-text">{gone ? T.goneTitle : atEnd ? T.endTitle : T.stepOf(progress.step + 1, lesson.steps.length)}</p>
              </div>
              <button type="button" className="btn ghost sm" aria-expanded="true" onClick={() => setCollapsed(true)} title={T.hide}>
                <Icon name="down" /><span className="learn-hide-text">{T.hide}</span>
              </button>
            </header>
            <div className="learn-meter" role="progressbar" aria-label={T.progressLabel(doneCount, lesson.steps.length)} aria-valuemin={0} aria-valuemax={lesson.steps.length} aria-valuenow={doneCount}>
              <span style={{ width: `${(doneCount / lesson.steps.length) * 100}%` }} />
            </div>
            <div className="learn-body" aria-live="polite">
              {doneCount > 0 && !gone && <DoneList lesson={lesson} step={progress.step} skipped={progress.skipped} />}
              {gone ? <Gone lessonId={id} />
                : atEnd ? <End lesson={lesson} inPractice={inPractice} canPlay={snap.canPlay} />
                : step && <Current step={step} inPractice={inPractice} onScreen={onScreen} />}
            </div>
            <footer className="learn-foot">
              <button type="button" className="btn ghost sm" onClick={() => learn.stop()}>{T.stopLesson}</button>
            </footer>
          </>
        )}
      </aside>
    </>
  )
}

function DoneList({ lesson, step, skipped }: { lesson: Lesson; step: number; skipped: string[] }) {
  return (
    <details className="learn-done" data-testid="learn-done">
      <summary>{T.doneSoFar(step)}</summary>
      <ol>
        {lesson.steps.slice(0, step).map((s) => (
          <li key={s.id} data-skipped={skipped.includes(s.id) || undefined}>
            <Icon name="check" />
            <span>{s.title}{skipped.includes(s.id) ? ` ${T.skippedMark}` : ''}</span>
          </li>
        ))}
      </ol>
    </details>
  )
}

function Current({ step, inPractice, onScreen }: { step: Step; inPractice: boolean; onScreen: boolean }) {
  return (
    <section className="learn-step" data-testid="learn-step">
      <h2 className="learn-step-title" data-testid="learn-step-title">{step.title}</h2>
      <p>{step.text}</p>
      {step.where && inPractice && <p className="learn-where">{T.happensOn(SECTION_LABEL[step.where.section])}</p>}
      {!inPractice && <p className="learn-where">{T.elsewhere}</p>}
      <div className="learn-actions">
        {!onScreen && <button type="button" className="btn primary sm" onClick={() => learn.takeMeThere()}>{T.takeMeThere}</button>}
        <button type="button" className="btn sm" onClick={() => learn.skipStep()}>{T.skipStep}</button>
      </div>
    </section>
  )
}

function Gone({ lessonId }: { lessonId: string }) {
  const snap = useLearn()
  return (
    <section className="learn-step" data-testid="learn-gone">
      <h2 className="learn-step-title">{T.goneTitle}</h2>
      <p>{T.goneBody}</p>
      {snap.error && <p className="error-text" role="alert">{snap.error}</p>}
      <div className="learn-actions">
        <button type="button" className="btn primary sm" disabled={snap.busy} onClick={() => void learn.start(lessonId)}>{snap.busy ? T.starting : T.startAgain}</button>
      </div>
    </section>
  )
}

function End({ lesson, inPractice, canPlay }: { lesson: Lesson; inPractice: boolean; canPlay: boolean }) {
  const snap = useLearn()
  const next = LESSONS.filter((l) => l.id !== lesson.id && lessonStatus(l, snap.state.lessons[l.id]).kind !== 'finished')
  return (
    <section className="learn-step" data-testid="learn-end">
      <h2 className="learn-step-title">{T.endTitle}</h2>
      <p className="learn-sub">{T.endMade}</p>
      <ul className="learn-made">{lesson.summary.map((s) => <li key={s}>{s}</li>)}</ul>
      <div className="learn-actions">
        {lesson.playable && (inPractice
          ? <button type="button" className="btn primary sm" disabled={!canPlay} onClick={() => learn.play()}><Icon name="play" />{T.endPlay}</button>
          : <button type="button" className="btn primary sm" onClick={() => learn.takeMeThere()}>{T.endOpen}</button>)}
        <button type="button" className="btn sm" onClick={() => learn.finish()}>{T.endDone}</button>
      </div>
      {next.length > 0 && (
        <>
          <p className="learn-sub">{T.endNext}</p>
          <ul className="learn-next">
            {next.map((l) => (
              <li key={l.id}>
                <span>{l.title}</span>
                <button type="button" className="btn sm" disabled={snap.busy} onClick={() => void learn.start(l.id)}>{T.start}</button>
              </li>
            ))}
          </ul>
        </>
      )}
    </section>
  )
}
