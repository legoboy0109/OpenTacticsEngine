import { useCallback, useEffect, useState } from 'react'
import type { Route } from '../router'
import { Link } from '../router'
import { GameScreen } from '../screens/game/GameScreen'
import { MapsScreen } from '../screens/maps/MapsScreen'
import { RulesScreen } from '../screens/rules/RulesScreen'
import { TerrainScreen } from '../screens/terrain/TerrainScreen'
import { UnitsScreen } from '../screens/units/UnitsScreen'
import { WeaponsScreen } from '../screens/weapons/WeaponsScreen'
import { useProject, useStore } from '../store/context'
import { ProblemsPanel } from './ProblemsPanel'
import { SaveBanners } from './SaveBanners'
import { Sidebar } from './Sidebar'
import { TopBar } from './TopBar'

type ProjectRoute = Extract<Route, { kind: 'project' }>

/** Screens receive the item picked in the URL (if any). The map editor and
 *  Play plug in here: replace MapsScreen, and give TopBar a real Play button. */
function Screen({ route }: { route: ProjectRoute }) {
  switch (route.section) {
    case 'game': return <GameScreen />
    case 'rules': return <RulesScreen />
    case 'units': return <UnitsScreen itemId={route.itemId} />
    case 'weapons': return <WeaponsScreen itemId={route.itemId} />
    case 'terrain': return <TerrainScreen itemId={route.itemId} />
    case 'maps': return <MapsScreen itemId={route.itemId} />
  }
}

export function ProjectShell({ route }: { route: ProjectRoute }) {
  const store = useStore()
  const { phase, loadError, doc, canUndo, canRedo } = useProject()
  const [problemsOpen, setProblemsOpen] = useState(false)
  const closeProblems = useCallback(() => setProblemsOpen(false), [])

  // Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z act on the whole game, even while typing in a box.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (!(e.ctrlKey || e.metaKey) || e.altKey || e.key.toLowerCase() !== 'z') return
      e.preventDefault()
      if (e.shiftKey) store.redo(); else store.undo()
    }
    document.addEventListener('keydown', onKey)
    return () => document.removeEventListener('keydown', onKey)
  }, [store, canUndo, canRedo])

  useEffect(() => { document.title = `${doc.game?.name || store.id} - Game Editor` }, [doc.game?.name, store.id])

  if (phase === 'loading') return <div className="center-msg" role="status">Opening game&hellip;</div>
  if (phase === 'failed') {
    return (
      <div className="center-msg">
        <h1>Could not open this game</h1>
        <p>{loadError}</p>
        <Link to="/" className="btn primary">Back to all games</Link>
      </div>
    )
  }
  return (
    <div className="app">
      <a href="#main" className="skip-link">Skip to content</a>
      <TopBar problemsOpen={problemsOpen} onToggleProblems={() => setProblemsOpen((o) => !o)} />
      {problemsOpen && <ProblemsPanel onClose={closeProblems} />}
      <div className="workspace">
        <Sidebar current={route.section} />
        <main id="main" className="main" tabIndex={-1}>
          <SaveBanners />
          <Screen route={route} />
        </main>
      </div>
    </div>
  )
}
