// The small "what changes" signs of a box: an icon and a count. They carry a text alternative (title and aria-label).
import { MARKER_KEYS, type Markers } from '../../../model/timelineTypes'
import { TL } from './text'

// Plain stroke icons on a 24 x 24 grid (no emoji, no icon library).
const ICONS: Record<keyof Markers, string> = {
  join: 'M9 11a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7zM3 20c0-3.5 2.7-6 6-6s6 2.5 6 6M19 8v6M16 11h6',
  leave: 'M9 11a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7zM3 20c0-3.5 2.7-6 6-6s6 2.5 6 6M16 11h6',
  items: 'M4 8h16v12H4zM4 8l2-4h12l2 4M10 12h4',
  shop: 'M3 9l2-5h14l2 5M3 9h18M5 9v11h14V9M10 20v-6h4v6',
  flags: 'M6 21V4M6 5h12l-3 4 3 4H6',
  questions: 'M4 5h16v11H11l-4 4v-4H4zM10 9.5a2 2 0 1 1 3 1.7c-.7.4-1 .8-1 1.5M12 14.8v.01',
}

export function MarkerIcon({ kind }: { kind: keyof Markers }) {
  return (
    <svg className="tl-marker-icon" viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" strokeWidth="2"
      strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" focusable="false"><path d={ICONS[kind]} /></svg>
  )
}

/** The words for a box's markers, for its accessible name ("2 questions asked, 1 unit joins"). */
export function markersText(m: Markers): string {
  return MARKER_KEYS.filter((k) => m[k] > 0).map((k) => TL.marker[k](m[k])).join(', ')
}

export function MarkerRow({ markers, only }: { markers: Markers; only?: Array<keyof Markers> }) {
  const shown = MARKER_KEYS.filter((k) => markers[k] > 0 && (!only || only.includes(k)))
  if (shown.length === 0) return null
  return (
    <span className="tl-markers">
      {shown.map((k) => (
        <span key={k} className="tl-marker" data-marker={k} title={TL.marker[k](markers[k])} aria-label={TL.marker[k](markers[k])} role="img">
          <MarkerIcon kind={k} /><span aria-hidden="true">{markers[k]}</span>
        </span>
      ))}
    </span>
  )
}
