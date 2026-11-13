// React wiring for the document store.
import { createContext, useContext, useEffect, useState, useSyncExternalStore, type ReactNode } from 'react'
import { api } from '../api/client'
import type { ProjectDoc } from '../model/types'
import { DocumentStore, type StoreState } from './documentStore'

const Ctx = createContext<DocumentStore | null>(null)

/** Opens project `id`: loads it, autosaves it, and flushes when it is closed. */
export function ProjectProvider({ id, children }: { id: string; children: ReactNode }) {
  const [store, setStore] = useState<DocumentStore | null>(null)
  useEffect(() => {
    const s = new DocumentStore(api, id)
    const detach = s.attachPageEvents()
    setStore(s)
    void s.load()
    return () => { detach(); s.dispose(); setStore(null) }
  }, [id])
  if (!store) return null
  return <Ctx.Provider value={store}>{children}</Ctx.Provider>
}

export function useStore(): DocumentStore {
  const s = useContext(Ctx)
  if (!s) throw new Error('useStore outside ProjectProvider')
  return s
}

export function useProject(): StoreState {
  const s = useStore()
  return useSyncExternalStore(s.subscribe, s.getState)
}

export function useDoc(): ProjectDoc {
  return useProject().doc
}

/** `edit(fn, key?)`: apply a pure edit; same `key` in quick succession = one undo step. */
export function useEdit(): DocumentStore['edit'] {
  return useStore().edit
}
