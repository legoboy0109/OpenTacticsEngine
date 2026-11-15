// Adding a sound file: check it here first (size, first bytes), upload it, and say plainly why not when it fails. The
// helper checks the same things again, so nothing relies on this. A failed step leaves the game exactly as it was.
import { createContext, useCallback, useContext, useRef, useState, type ReactNode } from 'react'
import type { ApiError } from '../../api/client'
import { MAX_AUDIO_BYTES, audioExt, audioNameFor, sniffAudio } from '../../model/audioFiles'
import { useStore } from '../../store/context'
import { T } from './text'

export interface AudioNotice { id: number; tone: 'info' | 'error' | 'success'; text: string }

export const uploadFailure = (name: string, res: ApiError): string => (res.status === 0 ? T.helperDown(name, res.error) : T.uploadFailed(name, res.error))

export function useAudioUpload() {
  const store = useStore()
  const [busy, setBusy] = useState(0)
  const [notices, setNotices] = useState<AudioNotice[]>([])
  const counter = useRef(0)
  const say = useCallback((tone: AudioNotice['tone'], text: string) => setNotices((n) => [...n, { id: counter.current++, tone, text }]), [])
  const dismiss = useCallback((id: number) => setNotices((n) => n.filter((x) => x.id !== id)), [])

  /** Uploads one file; the path in audio/ it now has, or null (after saying why). */
  const upload = useCallback(async (f: File): Promise<string | null> => {
    if (f.size > MAX_AUDIO_BYTES) { say('error', T.tooBig(f.name)); return null }
    const head = new Uint8Array(await f.slice(0, 12).arrayBuffer())
    const kind = sniffAudio(head)
    if (!kind) { say('error', T.notSoundFile(f.name)); return null }
    const says = audioExt(f.name)
    if (says && says !== kind) { say('error', T.wrongKind(f.name, kind, says)); return null }
    setBusy((b) => b + 1)
    try {
      const names = () => store.audioFiles.getState().files.map((x) => x.file)
      let res = await store.audioFiles.upload(audioNameFor(f.name, kind, names()), f, false)
      // Another upload took the name meanwhile: look again.
      for (let i = 0; !res.ok && res.status === 409 && i < 3; i++) {
        await store.audioFiles.refresh()
        res = await store.audioFiles.upload(audioNameFor(f.name, kind, names()), f, false)
      }
      if (!res.ok) { say('error', uploadFailure(f.name, res)); return null }
      return res.value.file
    } finally {
      setBusy((b) => b - 1)
    }
  }, [store, say])

  return { upload, busy, notices, say, dismiss }
}

/** The screen's one upload, for the file pickers in its rows. */
const Ctx = createContext<((f: File) => Promise<string | null>) | null>(null)
export const UploadProvider = ({ upload, children }: { upload: (f: File) => Promise<string | null>; children: ReactNode }) => <Ctx.Provider value={upload}>{children}</Ctx.Provider>
export function useUpload(): (f: File) => Promise<string | null> {
  const u = useContext(Ctx)
  if (!u) throw new Error('useUpload outside UploadProvider')
  return u
}
