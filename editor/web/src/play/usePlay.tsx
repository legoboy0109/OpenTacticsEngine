// Play button logic: save, refuse while the game has problems, check the web player exists, open the panel.
import { useCallback, useRef, useState, type ReactNode, type RefObject } from 'react'
import { useStore } from '../store/context'
import { Dialog } from '../ui/Dialog'
import { PlayPanel } from './PlayPanel'
import { T } from './text'

/** True unless the helper says the web player files are not there (404). */
export async function playerAvailable(): Promise<boolean> {
  try {
    const res = await fetch('/player/ote-player.wasm', { method: 'HEAD' })
    if (res.status === 404) return false
    if (res.ok) return true
    const ranged = await fetch('/player/ote-player.wasm', { headers: { Range: 'bytes=0-0' } })
    return ranged.ok
  } catch {
    return false
  }
}

type Notice = { title: string; body: ReactNode }

export function usePlay(maps: Array<{ id: string; name: string }>, onProblems: () => void): {
  start: (mapId: string) => Promise<void>
  busy: boolean
  node: ReactNode
  buttonRef: RefObject<HTMLButtonElement>
} {
  const store = useStore()
  const [open, setOpen] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)
  const [notice, setNotice] = useState<Notice | null>(null)
  const buttonRef = useRef<HTMLButtonElement>(null)

  const start = useCallback(async (mapId: string) => {
    setBusy(true)
    try {
      // Play runs what is on disk, so write everything first.
      if (!(await store.flush())) { setNotice({ title: T.unsavedTitle, body: <p>{T.unsavedBody}</p> }); return }
      if (store.getState().problems.length > 0) { onProblems(); return }
      if (!(await playerAvailable())) {
        setNotice({
          title: T.noPlayerTitle,
          body: (<><p>{T.noPlayerBody}</p><pre className="command" data-testid="player-build-command">{T.noPlayerCommand}</pre><p className="help">{T.noPlayerHelp}</p></>),
        })
        return
      }
      setOpen(mapId)
    } finally {
      setBusy(false)
    }
  }, [store, onProblems])

  const close = useCallback(() => { setOpen(null); requestAnimationFrame(() => buttonRef.current?.focus()) }, [])
  const closeNotice = useCallback(() => { setNotice(null); requestAnimationFrame(() => buttonRef.current?.focus()) }, [])
  const node = (
    <>
      {open && <PlayPanel store={store} maps={maps} initialMap={open} onClose={close} />}
      <Dialog open={!!notice} title={notice?.title ?? ''} onClose={closeNotice}
        footer={<button type="button" className="btn primary" onClick={closeNotice}>{T.ok}</button>}>
        {notice?.body}
      </Dialog>
    </>
  )
  return { start, busy, node, buttonRef }
}
