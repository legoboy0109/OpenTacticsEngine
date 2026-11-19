import { Component, type ReactNode } from 'react'

/** Catches a section's code failing to load (offline, helper stopped, stale tab after a rebuild): a plain message, never a blank page. */
export class LazyBoundary extends Component<{ children: ReactNode }, { failed: boolean }> {
  state = { failed: false }
  static getDerivedStateFromError() { return { failed: true } }
  componentDidCatch(error: unknown) { console.error('Section failed to load:', error) }
  render() {
    if (!this.state.failed) return this.props.children
    return (
      <div className="center-msg" role="alert">
        <h1>Could not load this section</h1>
        <p>Its code could not be fetched, probably because the editor helper is unreachable or was updated. Check the connection and reload the page.</p>
        <button type="button" className="btn primary" onClick={() => window.location.reload()}>Reload</button>
      </div>
    )
  }
}
