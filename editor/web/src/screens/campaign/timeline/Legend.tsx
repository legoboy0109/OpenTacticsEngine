// A small legend: what each sign on a box means, and the kinds of arrow.
import { MARKER_KEYS } from '../../../model/timelineTypes'
import { MarkerIcon } from './Markers'
import { TL } from './text'

export function Legend() {
  return (
    <details className="tl-legend" data-testid="timeline-legend">
      <summary>{TL.legendTitle}</summary>
      <div className="tl-legend-body">
        <p>{TL.legendMarkers}</p>
        <ul className="tl-legend-list">
          {MARKER_KEYS.map((k) => <li key={k}><span className="tl-marker"><MarkerIcon kind={k} /></span>{TL.legendMarkerNames[k]}</li>)}
        </ul>
        <p>{TL.legendArrows}</p>
        <ul className="tl-legend-list">
          <li><svg width="34" height="10" aria-hidden="true"><path d="M1 5h32" className="tl-edge-order" /></svg>{TL.legendOrder}</li>
          <li><svg width="34" height="10" aria-hidden="true"><path d="M1 5h32" className="tl-edge-next" /></svg>{TL.legendNext}</li>
          <li><svg width="34" height="10" aria-hidden="true"><path d="M1 5h32" className="tl-edge-goto" /></svg>{TL.legendGoto}</li>
          <li>{TL.legendLoop}</li>
          <li>{TL.legendUnreached}</li>
        </ul>
      </div>
    </details>
  )
}
