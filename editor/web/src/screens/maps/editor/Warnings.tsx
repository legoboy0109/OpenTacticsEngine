// Friendly checks above the canvas. Clicking one selects and shows the square it is about.
import type { MapWarning } from '../../../model/mapChecks'
import { Icon } from '../../../ui/Icon'
import { T } from './text'

export function Warnings({ warnings, onFocus }: { warnings: MapWarning[]; onFocus: (c: { x: number; y: number }) => void }) {
  if (warnings.length === 0) return null
  return (
    <section className="map-warnings" aria-label={T.warningsTitle(warnings.length)} data-testid="map-warnings">
      <header><Icon name="warn" /><strong>{T.warningsTitle(warnings.length)}</strong><span className="help">{T.warningsHelp}</span></header>
      <ul>
        {warnings.map((w) => (
          <li key={w.key}>
            {w.cell
              ? <button type="button" className="warning-link" onClick={() => onFocus(w.cell!)}>{w.text}</button>
              : <span>{w.text}</span>}
          </li>
        ))}
      </ul>
    </section>
  )
}
