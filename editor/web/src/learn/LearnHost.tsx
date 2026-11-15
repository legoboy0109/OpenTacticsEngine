// Mounted once by App: loads the remembered state, and shows the Learn list and the lesson panel above every screen.
import { useEffect } from 'react'
import { useRoute } from '../router'
import { learn } from './instance'
import { LearnList } from './LearnList'
import { LessonPanel } from './LessonPanel'
import { runningLessonId } from './state'
import { useLearn } from './useLearn'

export function LearnHost() {
  const route = useRoute()
  const snap = useLearn()
  useEffect(() => { void learn.load() }, [])
  // A running lesson whose practice game was deleted says so: checked on page load and whenever the game list shows.
  const running = runningLessonId(snap.state)
  useEffect(() => { if (snap.loaded && running) void learn.checkPractice() }, [snap.loaded, running, route.kind])
  return (
    <>
      <LessonPanel />
      <LearnList />
    </>
  )
}
