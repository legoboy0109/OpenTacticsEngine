import { useEffect, useRef } from 'react'
import { TOOL_KEY, TOOLS, type Tool } from './tools'

interface Handlers {
  onTool: (t: Tool) => void
  onFill: () => void
  onZoom: (factor: number) => void
  onFit: () => void
  onRemove: () => void
  onEscape: () => void
}

const typing = (el: EventTarget | null): boolean => {
  const e = el as HTMLElement | null
  return !!e && (e.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(e.tagName) || !!e.closest?.('dialog'))
}

/** Single-key shortcuts of the map editor (T U O S, F, + -, 0, Delete, Esc). Never while typing or in a dialog. */
export function useMapShortcuts(h: Handlers): void {
  const ref = useRef(h)
  ref.current = h
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey || e.altKey || typing(e.target)) return
      const k = e.key.toLowerCase()
      const tool = TOOLS.find((t) => TOOL_KEY[t] === k)
      const onCanvas = (e.target as HTMLElement).tagName === 'CANVAS'
      if (tool) ref.current.onTool(tool)
      else if (k === 'f') ref.current.onFill()
      else if (k === '+' || k === '=') ref.current.onZoom(1.25)
      else if (k === '-' || k === '_') ref.current.onZoom(1 / 1.25)
      else if (k === '0') ref.current.onFit()
      else if (k === 'delete' && !onCanvas) ref.current.onRemove()
      else if (k === 'escape' && !onCanvas) ref.current.onEscape()
      else return
      if (k !== 'delete' && k !== 'escape') e.preventDefault()
    }
    document.addEventListener('keydown', onKey)
    return () => document.removeEventListener('keydown', onKey)
  }, [])
}
