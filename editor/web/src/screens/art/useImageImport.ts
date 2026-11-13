// Adding images: prepare (convert, check size), upload, then add the image to the document with a first guess for
// how it is divided. A failed step shows a plain message and leaves the document exactly as it was.
import { useCallback, useRef, useState } from 'react'
import type { ApiError } from '../../api/client'
import { prepareImage } from '../../art/importImage'
import { artNameFor, displayStem } from '../../model/artFiles'
import { addSheet, newSheetId } from '../../model/graphics'
import { guessDivision } from '../../model/gridGuess'
import { useStore } from '../../store/context'
import { T } from './text'

export interface ArtNotice { id: number; tone: 'info' | 'error' | 'success' | 'warn'; text: string }

/** The helper's own wording is already plain language; an unreachable helper gets its own sentence. */
export const uploadFailure = (name: string, res: ApiError): string => (res.status === 0 ? T.helperDown(name, res.error) : T.uploadFailed(name, res.error))

export function useImageImport(onAdded: (sheetId: string) => void) {
  const store = useStore()
  const [busy, setBusy] = useState(0)
  const [notices, setNotices] = useState<ArtNotice[]>([])
  const counter = useRef(0)
  const say = useCallback((tone: ArtNotice['tone'], text: string) => setNotices((n) => [...n, { id: counter.current++, tone, text }]), [])
  const dismiss = useCallback((id: number) => setNotices((n) => n.filter((x) => x.id !== id)), [])

  const addFiles = useCallback(async (files: File[]) => {
    const list = files.filter((f) => f.size > 0 || f.type.startsWith('image/'))
    if (!list.length) return
    setBusy((b) => b + list.length)
    let added = 0
    let last: string | null = null
    try {
      for (const f of list) {
        try {
          const prep = await prepareImage(f)
          if (!prep.ok) { say('error', prep.message); continue }
          // The name is unique among the files that exist now; if another upload took it meanwhile, look again.
          let res = await store.art.upload(artNameFor(f.name, store.art.getState().files.map((x) => x.file)), prep.blob, false)
          for (let i = 0; !res.ok && res.status === 409 && i < 3; i++) {
            await store.art.refresh()
            res = await store.art.upload(artNameFor(f.name, store.art.getState().files.map((x) => x.file)), prep.blob, false)
          }
          if (!res.ok) { say('error', uploadFailure(f.name, res)); continue }
          const doc = store.getState().doc
          const tile = doc.game?.view?.tileSize ?? 24
          const div = guessDivision(res.value.width, res.value.height, tile)
          const stem = displayStem(f.name)
          const id = newSheetId(doc, stem || res.value.file)
          store.edit((d) => addSheet(d, { file: res.value.file, name: stem || undefined, cellWidth: div.cellWidth, cellHeight: div.cellHeight }, newSheetId(d, stem || res.value.file)))
          added++; last = id
          if (prep.converted) say('info', T.converted(f.name))
        } finally {
          setBusy((b) => b - 1)
        }
      }
    } finally {
      if (last) onAdded(last)
      if (added > 1) say('success', T.added(added))
    }
  }, [store, say, onAdded])

  return { addFiles, busy, notices, say, dismiss }
}
