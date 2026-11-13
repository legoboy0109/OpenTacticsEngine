import type { ReactNode } from 'react'
import { Icon } from '../../../ui/Icon'
import { TOOLS, type Tool } from './tools'
import { T } from './text'
import type { MapViewState } from './viewState'

const TOOL_ICON: Record<Tool, ReactNode> = {
  terrain: <path d="M4 20l5-9 4 6 3-4 4 7zM3 8h6M12 5h9" />,
  units: <path d="M12 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM5 21c0-4 3-6 7-6s7 2 7 6" />,
  owners: <path d="M5 21V4h11l-2 4 2 4H5" />,
  select: <path d="M5 4l13 6-6 2-2 6zM13 13l6 6" />,
}

interface Props { tool: Tool; onTool: (t: Tool) => void }

export function Toolbar({ tool, onTool }: Props) {
  return (
    <div className="map-toolbar">
      <div className="tool-group" role="toolbar" aria-label={T.toolsLabel}>
        {TOOLS.map((t) => (
          <button key={t} type="button" className="tool-btn" aria-pressed={t === tool} onClick={() => onTool(t)} title={`${T.tools[t].label} (${T.tools[t].key}): ${T.tools[t].help}`} data-tool={t}>
            <svg className="icon-svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">{TOOL_ICON[t]}</svg>
            <span className="tool-label">{T.tools[t].label}</span>
            <kbd className="tool-key" aria-hidden="true">{T.tools[t].key}</kbd>
          </button>
        ))}
      </div>
    </div>
  )
}

/** Floats over the corner of the canvas. */
export function ZoomControls({ vs, mapW, mapH }: { vs: MapViewState; mapW: number; mapH: number }) {
  return (
    <div className="tool-group zoom-controls" role="group" aria-label="Zoom">
      <button type="button" className="btn ghost sm icon" onClick={() => vs.zoomBy(1 / 1.25)} aria-label={T.zoomOut} title={`${T.zoomOut} (-)`}><Icon name="minus" /></button>
      <button type="button" className="btn ghost sm icon" onClick={() => vs.zoomBy(1.25)} aria-label={T.zoomIn} title={`${T.zoomIn} (+)`}><Icon name="plus" /></button>
      <button type="button" className="btn ghost sm" onClick={() => vs.fit(mapW, mapH)} title={T.fitHelp}>{T.fit}</button>
    </div>
  )
}
