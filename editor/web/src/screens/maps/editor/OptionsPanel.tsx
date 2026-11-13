// Right-hand panel on wide screens, collapsible sheet under the canvas on phones.
import { useState } from 'react'
import type { MapFile, ProjectDoc } from '../../../model/types'
import { useStored } from '../../../ui/hooks'
import { Icon } from '../../../ui/Icon'
import { SettingsPanel } from './panels/SettingsPanel'
import { SidesPanel } from './panels/SidesPanel'
import { ToolPanel } from './panels/ToolPanel'
import { T } from './text'
import type { ToolState } from './tools'

type Tab = 'tool' | 'map' | 'sides'
const TABS: Tab[] = ['tool', 'map', 'sides']

interface Props {
  doc: ProjectDoc
  map: MapFile
  mapId: string
  state: ToolState
  selected: string | null
  onChange: (p: Partial<ToolState>) => void
  say: (m: string | null) => void
}

export function OptionsPanel({ doc, map, mapId, state, selected, onChange, say }: Props) {
  const [tab, setTab] = useState<Tab>('tool')
  const [open, setOpen] = useStored('ote.mapSheetOpen', true)
  const choose = (t: Tab) => { setTab(t); setOpen(true) }
  return (
    <aside className="map-panel" aria-label={T.panelLabel} data-open={open}>
      <div className="panel-bar">
        <div className="tabs" role="tablist" aria-label={T.panelLabel}>
          {TABS.map((t) => (
            <button key={t} type="button" role="tab" id={`map-tab-${t}`} aria-selected={tab === t} aria-controls="map-tabpanel" className="tab" onClick={() => choose(t)}>{T.tabs[t]}</button>
          ))}
        </div>
        <button type="button" className="btn ghost sm sheet-toggle" aria-expanded={open} aria-controls="map-tabpanel" onClick={() => setOpen(!open)}>
          <Icon name={open ? 'down' : 'up'} />{open ? T.sheetClose : T.sheetOpen}
        </button>
      </div>
      <div id="map-tabpanel" role="tabpanel" aria-labelledby={`map-tab-${tab}`} className="panel-body">
        {tab === 'tool' && <ToolPanel doc={doc} map={map} mapId={mapId} tool={state.tool} state={state} selected={selected} onChange={onChange} say={say} />}
        {tab === 'map' && <SettingsPanel doc={doc} map={map} mapId={mapId} />}
        {tab === 'sides' && <SidesPanel doc={doc} map={map} mapId={mapId} />}
      </div>
    </aside>
  )
}
