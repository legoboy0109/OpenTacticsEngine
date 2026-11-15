import { useSyncExternalStore } from 'react'
import { soundPreview, type PreviewState } from './preview'

/** What the shared preview is doing (which button is playing, and whether the browser refused). */
export function useSoundPreview(): PreviewState {
  return useSyncExternalStore(soundPreview.subscribe, soundPreview.getState)
}
