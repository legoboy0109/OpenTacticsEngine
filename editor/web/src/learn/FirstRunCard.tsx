// The game list's one-time offer of the beginner lesson. Shown only while there is no saved Learn state at all.
import { learn } from './instance'
import { FIRST_LESSON_ID } from './lessons'
import { isFirstRun } from './state'
import { T } from './text'
import { useLearn } from './useLearn'

export function FirstRunCard() {
  const snap = useLearn()
  if (!snap.available || !isFirstRun(snap.state)) return null
  return (
    <section className="learn-offer" aria-label={T.offerTitle} data-testid="learn-offer">
      <div>
        <h2>{T.offerTitle}</h2>
        <p>{T.offerBody}</p>
        <p className="help">{T.offerHelp}</p>
      </div>
      <div className="learn-offer-actions">
        <button type="button" className="btn primary" disabled={snap.busy} onClick={() => void learn.answerOffer(FIRST_LESSON_ID, true)}>{snap.busy ? T.starting : T.offerStart}</button>
        <button type="button" className="btn" disabled={snap.busy} onClick={() => void learn.answerOffer(FIRST_LESSON_ID, false)}>{T.offerLater}</button>
      </div>
      {snap.error && <p className="error-text" role="alert">{snap.error}</p>}
    </section>
  )
}
