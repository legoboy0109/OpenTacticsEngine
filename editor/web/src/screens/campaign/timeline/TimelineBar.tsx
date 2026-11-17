// The buttons above the timeline: zoom, add a chapter at the end, the key list; and, for the selected chapter, move it, add a
// chapter next to it, add or remove its scenes, open its settings, remove it. (Moving by button is the way to reorder without
// dragging: on a phone and from the keyboard.)
import type { SceneKey } from '../../../model/campaignTypes'
import type { TimelineChapter } from '../../../model/timelineTypes'
import { Icon } from '../../../ui/Icon'
import { TL } from './text'

export interface SelectedActions {
  earlier: () => void
  later: () => void
  addBefore: () => void
  addAfter: () => void
  settings: () => void
  addScene: (which: SceneKey) => void
  removeScene: (which: SceneKey) => void
  remove: () => void
  clear: () => void
}

interface Props {
  scale: number
  onZoom: (factor: number) => void
  onFit: () => void
  onAddEnd: () => void
  helpOpen: boolean
  onHelp: () => void
  selected: TimelineChapter | null
  count: number
  actions: SelectedActions
}

export function TimelineBar({ scale, onZoom, onFit, onAddEnd, helpOpen, onHelp, selected, count, actions }: Props) {
  return (
    <div className="tl-bars">
      <div className="tl-bar" role="toolbar" aria-label={TL.region}>
        <button type="button" className="btn sm" onClick={() => onZoom(1 / 1.25)} aria-label={TL.zoomOut} title={TL.zoomOut}><Icon name="minus" /></button>
        <span className="tl-zoom" aria-live="off" data-testid="timeline-zoom">{Math.round(scale * 100)}%</span>
        <button type="button" className="btn sm" onClick={() => onZoom(1.25)} aria-label={TL.zoomIn} title={TL.zoomIn}><Icon name="plus" /></button>
        <button type="button" className="btn sm" onClick={onFit}>{TL.fit}</button>
        <button type="button" className="btn sm primary" onClick={onAddEnd}><Icon name="plus" />{TL.addEnd}</button>
        <button type="button" className="btn sm ghost" aria-expanded={helpOpen} aria-controls="timeline-keys" onClick={onHelp}><Icon name="info" />{TL.help}</button>
      </div>
      {helpOpen && (
        <div id="timeline-keys" className="tl-keys" role="note" aria-label={TL.helpTitle}>
          <strong>{TL.helpTitle}</strong>
          <ul>{TL.helpLines.map((l) => <li key={l}>{l}</li>)}</ul>
        </div>
      )}
      <div className="tl-selbar" role="toolbar" aria-label={TL.selectedBar} data-testid="timeline-selected">
        {!selected && <p className="help">{TL.noSelection}</p>}
        {selected && (
          <>
            <strong className="tl-selname">{TL.selectedName(selected.name)}</strong>
            <button type="button" className="btn sm" disabled={selected.index === 0} onClick={actions.earlier}><Icon name="back" />{TL.earlier}</button>
            <button type="button" className="btn sm" disabled={selected.index === count - 1} onClick={actions.later}>{TL.later}<Icon name="back" className="tl-flip" /></button>
            <button type="button" className="btn sm" onClick={actions.addBefore}><Icon name="plus" />{TL.addBefore}</button>
            <button type="button" className="btn sm" onClick={actions.addAfter}><Icon name="plus" />{TL.addAfter}</button>
            {selected.hasBefore
              ? <button type="button" className="btn sm" onClick={() => actions.removeScene('before')}>{TL.removeBeforeScene}</button>
              : <button type="button" className="btn sm" onClick={() => actions.addScene('before')}>{TL.addBeforeScene}</button>}
            {selected.hasAfter
              ? <button type="button" className="btn sm" onClick={() => actions.removeScene('after')}>{TL.removeAfterScene}</button>
              : <button type="button" className="btn sm" onClick={() => actions.addScene('after')}>{TL.addAfterScene}</button>}
            <button type="button" className="btn sm" onClick={actions.settings}>{TL.settings}</button>
            <button type="button" className="btn sm danger" onClick={actions.remove}><Icon name="trash" />{TL.removeChapter}</button>
            <button type="button" className="btn sm ghost" onClick={actions.clear}>{TL.deselect}</button>
          </>
        )}
      </div>
    </div>
  )
}
