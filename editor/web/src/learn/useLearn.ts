// React wiring for the Learn controller.
import { useEffect, useRef, useSyncExternalStore } from 'react'
import type { ProjectDoc, Section } from '../model/types'
import type { LearnSnapshot } from './controller'
import { learn } from './instance'
import { emitSignal } from './signals'

export const useLearn = (): LearnSnapshot => useSyncExternalStore(learn.subscribe, learn.getSnapshot)

/** ProjectShell tells the lesson runner which game and screen are open and whether Play can be pressed. */
export function useLearnContext(projectId: string, section: Section, doc: ProjectDoc | null, failed: boolean, play: () => void): void {
  // The latest `play` without registering again on every render.
  const playRef = useRef(play)
  playRef.current = play
  useEffect(() => {
    learn.setContext({ projectId, section, doc, failed, play: () => playRef.current() })
  }, [projectId, section, doc, failed])
  useEffect(() => () => learn.clearContext(projectId), [projectId])
}

/** Says `saved` whenever the game's revision on disk changes after the first load. */
export function useSavedSignal(revision: string): void {
  const last = useRef(revision)
  useEffect(() => {
    if (last.current !== '' && revision !== last.current) emitSignal('saved')
    last.current = revision
  }, [revision])
}
