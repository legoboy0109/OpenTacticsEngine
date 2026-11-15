import { useCallback, useEffect, useState } from 'react'
import { api, type ProjectInfo } from '../../api/client'
import { FirstRunCard } from '../../learn/FirstRunCard'
import { LearnButton } from '../../learn/LearnButton'
import { Link, projectPath } from '../../router'
import { Banner } from '../../ui/Banner'
import { Icon } from '../../ui/Icon'
import { NewGameDialog } from './NewGameDialog'
import { T } from './text'

type Load = { state: 'loading' } | { state: 'failed'; error: string } | { state: 'ready'; projects: ProjectInfo[] }

export function PickerScreen() {
  const [load, setLoad] = useState<Load>({ state: 'loading' })
  const [creating, setCreating] = useState(false)

  const refresh = useCallback(async () => {
    setLoad({ state: 'loading' })
    const res = await api.listProjects()
    setLoad(res.ok ? { state: 'ready', projects: res.value } : { state: 'failed', error: res.error })
  }, [])
  useEffect(() => { void refresh(); document.title = T.appName }, [refresh])

  return (
    <div className="picker">
      <header className="picker-head">
        <div>
          <p className="picker-app">{T.appName}</p>
          <h1>{T.title}</h1>
          <p className="help">{T.intro}</p>
        </div>
        <div className="picker-head-actions">
          <LearnButton className="btn" compact={false} />
          <button type="button" className="btn primary" onClick={() => setCreating(true)}><Icon name="plus" />{T.newGame}</button>
        </div>
      </header>
      <FirstRunCard />

      {load.state === 'loading' && <p className="help" role="status">{T.loading}</p>}
      {load.state === 'failed' && (
        <Banner tone="error" title={T.loadFailed} actions={<button type="button" className="btn sm" onClick={() => void refresh()}>{T.retry}</button>}>
          {load.error}
        </Banner>
      )}
      {load.state === 'ready' && load.projects.length === 0 && <p className="picker-empty">{T.empty}</p>}
      {load.state === 'ready' && load.projects.length > 0 && (
        <ul className="game-grid" aria-label={T.title}>
          {load.projects.map((p) => (
            <li key={p.id}>
              <Link to={projectPath(p.id, 'game')} className="game-card" data-ok={p.ok}>
                <span className="game-card-top">
                  <span className="game-card-name">{p.name || p.id}</span>
                  {!p.ok && <span className="game-card-warn" title={p.error}><Icon name="warn" /><span className="sr-only">{T.problemMark}</span></span>}
                </span>
                <span className="game-card-desc">{p.description || T.noDescription}</span>
                {!p.ok && <span className="game-card-error">{T.problemHelp}</span>}
                <span className="game-card-meta">{T.maps(p.maps)}</span>
              </Link>
            </li>
          ))}
        </ul>
      )}

      <NewGameDialog open={creating} existing={load.state === 'ready' ? load.projects.map((p) => p.id) : []} onClose={() => setCreating(false)} />
    </div>
  )
}
