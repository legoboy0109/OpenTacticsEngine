// The Learn list: every lesson with its state and Start / Continue / Start again.
import { Dialog } from '../ui/Dialog'
import { learn } from './instance'
import { LESSONS } from './lessons'
import { lessonStatus } from './state'
import { T } from './text'
import { useLearn } from './useLearn'
import type { Lesson } from './types'

export function LearnList() {
  const snap = useLearn()
  return (
    <Dialog open={snap.listOpen} title={T.listTitle} onClose={() => learn.closeList()}
      footer={<button type="button" className="btn" onClick={() => learn.closeList()}>{T.close}</button>}>
      <p className="help">{T.listIntro}</p>
      {snap.error && <p className="error-text" role="alert">{snap.error}</p>}
      <ul className="learn-list" aria-label={T.listTitle}>
        {LESSONS.map((l) => <LessonRow key={l.id} lesson={l} />)}
      </ul>
    </Dialog>
  )
}

function LessonRow({ lesson }: { lesson: Lesson }) {
  const snap = useLearn()
  const progress = snap.state.lessons[lesson.id]
  const status = lessonStatus(lesson, progress)
  const gone = snap.gone === lesson.id
  const text = status.kind === 'new' ? T.statusNew : status.kind === 'finished' ? T.statusFinished : T.statusProgress(status.step, status.total)
  return (
    <li className="learn-row" data-lesson={lesson.id} data-status={status.kind}>
      <div className="learn-row-text">
        <h3>{lesson.title}</h3>
        <p>{lesson.description}</p>
        <p className="learn-row-meta"><span>{T.minutes(lesson.minutes)}</span><span className="learn-row-status">{text}</span></p>
        {gone && <p className="error-text" role="alert">{T.goneInList(lesson.title)}</p>}
      </div>
      <div className="learn-row-actions">
        {status.kind === 'new' && <button type="button" className="btn primary sm" disabled={snap.busy} onClick={() => void learn.start(lesson.id)}>{snap.busy ? T.starting : T.start}</button>}
        {status.kind === 'progress' && !gone && <button type="button" className="btn primary sm" disabled={snap.busy} onClick={() => void learn.resume(lesson.id)}>{T.continue}</button>}
        {status.kind !== 'new' && <button type="button" className={`btn sm${status.kind === 'finished' || gone ? ' primary' : ''}`} disabled={snap.busy} onClick={() => void learn.start(lesson.id)}>{snap.busy ? T.starting : T.startAgain}</button>}
      </div>
    </li>
  )
}
