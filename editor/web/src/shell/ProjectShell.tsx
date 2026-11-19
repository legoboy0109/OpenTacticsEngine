import { Suspense, lazy, useCallback, useEffect, useState } from 'react'
import type { Route } from '../router'
import { Link } from '../router'
import { useLearnContext, useSavedSignal } from '../learn/useLearn'
import { PLAY_CAMPAIGN } from '../play/PlayPanel'
import { usePlay } from '../play/usePlay'
import { useProject, useStore } from '../store/context'
import { LazyBoundary } from './LazyBoundary'
import { onOpenProblems } from './openProblems'
import { ProblemsPanel } from './ProblemsPanel'
import { SaveBanners } from './SaveBanners'
import { Sidebar } from './Sidebar'
import { TopBar } from './TopBar'

// Each section's screen is its own chunk, fetched the first time the section opens (keeps the first load small).
const screenLoaders = {
  GameScreen: () => import('../screens/game/GameScreen').then((m) => ({ default: m.GameScreen })),
  CampaignScreen: () => import('../screens/campaign/CampaignScreen').then((m) => ({ default: m.CampaignScreen })),
  AbilitiesScreen: () => import('../screens/abilities/AbilitiesScreen').then((m) => ({ default: m.AbilitiesScreen })),
  ConditionsScreen: () => import('../screens/conditions/ConditionsScreen').then((m) => ({ default: m.ConditionsScreen })),
  CharactersScreen: () => import('../screens/characters/CharactersScreen').then((m) => ({ default: m.CharactersScreen })),
  SkillsScreen: () => import('../screens/skills/SkillsScreen').then((m) => ({ default: m.SkillsScreen })),
  SupportsScreen: () => import('../screens/supports/SupportsScreen').then((m) => ({ default: m.SupportsScreen })),
  ArtScreen: () => import('../screens/art/ArtScreen').then((m) => ({ default: m.ArtScreen })),
  SoundScreen: () => import('../screens/sound/SoundScreen').then((m) => ({ default: m.SoundScreen })),
  ItemsScreen: () => import('../screens/items/ItemsScreen').then((m) => ({ default: m.ItemsScreen })),
  MapsScreen: () => import('../screens/maps/MapsScreen').then((m) => ({ default: m.MapsScreen })),
  ResearchScreen: () => import('../screens/research/ResearchScreen').then((m) => ({ default: m.ResearchScreen })),
  RulesScreen: () => import('../screens/rules/RulesScreen').then((m) => ({ default: m.RulesScreen })),
  StructuresScreen: () => import('../screens/structures/StructuresScreen').then((m) => ({ default: m.StructuresScreen })),
  TerrainScreen: () => import('../screens/terrain/TerrainScreen').then((m) => ({ default: m.TerrainScreen })),
  UnitsScreen: () => import('../screens/units/UnitsScreen').then((m) => ({ default: m.UnitsScreen })),
  WeaponsScreen: () => import('../screens/weapons/WeaponsScreen').then((m) => ({ default: m.WeaponsScreen })),
}
const GameScreen = lazy(screenLoaders.GameScreen)
const CampaignScreen = lazy(screenLoaders.CampaignScreen)
const AbilitiesScreen = lazy(screenLoaders.AbilitiesScreen)
const ConditionsScreen = lazy(screenLoaders.ConditionsScreen)
const CharactersScreen = lazy(screenLoaders.CharactersScreen)
const SkillsScreen = lazy(screenLoaders.SkillsScreen)
const SupportsScreen = lazy(screenLoaders.SupportsScreen)
const ArtScreen = lazy(screenLoaders.ArtScreen)
const SoundScreen = lazy(screenLoaders.SoundScreen)
const ItemsScreen = lazy(screenLoaders.ItemsScreen)
const MapsScreen = lazy(screenLoaders.MapsScreen)
const ResearchScreen = lazy(screenLoaders.ResearchScreen)
const RulesScreen = lazy(screenLoaders.RulesScreen)
const StructuresScreen = lazy(screenLoaders.StructuresScreen)
const TerrainScreen = lazy(screenLoaders.TerrainScreen)
const UnitsScreen = lazy(screenLoaders.UnitsScreen)
const WeaponsScreen = lazy(screenLoaders.WeaponsScreen)

/** Fetch every screen chunk in the background so later section switches (and offline use) need no network. */
export function preloadScreens(): void {
  for (const load of Object.values(screenLoaders)) void load().catch(() => {})
}

type ProjectRoute = Extract<Route, { kind: 'project' }>

/** Screens receive the item picked in the URL (if any). */
function Screen({ route }: { route: ProjectRoute }) {
  switch (route.section) {
    case 'game': return <GameScreen />
    case 'rules': return <RulesScreen />
    case 'units': return <UnitsScreen itemId={route.itemId} />
    case 'characters': return <CharactersScreen itemId={route.itemId} />
    case 'skills': return <SkillsScreen itemId={route.itemId} />
    case 'abilities': return <AbilitiesScreen itemId={route.itemId} />
    case 'conditions': return <ConditionsScreen itemId={route.itemId} />
    case 'supports': return <SupportsScreen itemId={route.itemId} />
    case 'weapons': return <WeaponsScreen itemId={route.itemId} />
    case 'items': return <ItemsScreen itemId={route.itemId} />
    case 'terrain': return <TerrainScreen itemId={route.itemId} />
    case 'structures': return <StructuresScreen itemId={route.itemId} />
    case 'research': return <ResearchScreen itemId={route.itemId} />
    case 'art': return <ArtScreen itemId={route.itemId} />
    case 'sound': return <SoundScreen />
    case 'maps': return <MapsScreen itemId={route.itemId} />
    case 'campaign': return <CampaignScreen itemId={route.itemId} />
  }
}

export function ProjectShell({ route }: { route: ProjectRoute }) {
  const store = useStore()
  const { phase, loadError, doc, canUndo, canRedo, revision } = useProject()
  const [problemsOpen, setProblemsOpen] = useState(false)
  const [fixFirst, setFixFirst] = useState(false)
  const closeProblems = useCallback(() => { setProblemsOpen(false); setFixFirst(false) }, [])
  // Play runs the map being edited, the story on the Campaign screen, or the first map anywhere else.
  const maps = doc.game?.maps ?? []
  const playMap = route.section === 'campaign' && doc.campaign && maps.length > 0 ? PLAY_CAMPAIGN
    : (route.section === 'maps' && maps.some((m) => m.id === route.itemId) ? route.itemId : maps[0]?.id) ?? ''
  const play = usePlay(maps, useCallback(() => { setFixFirst(true); setProblemsOpen(true) }, []))

  // The lesson runner (docs/TUTORIAL.md) follows the open game, its screen, and the saves.
  useLearnContext(store.id, route.section, phase === 'ready' ? doc : null, phase === 'failed', () => void play.start(playMap))
  useSavedSignal(revision)

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

  // Once the opened section is showing, fetch the other sections' code quietly in the background.
  useEffect(() => {
    const t = window.setTimeout(preloadScreens, 1500)
    return () => window.clearTimeout(t)
  }, [])

  // A screen (the campaign timeline's problem badges) can ask for the Problems list.
  useEffect(() => onOpenProblems(() => setProblemsOpen(true)), [])
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
      <TopBar problemsOpen={problemsOpen} onToggleProblems={() => { if (problemsOpen) closeProblems(); else setProblemsOpen(true) }}
        onPlay={() => void play.start(playMap)} playBusy={play.busy} playDisabled={!playMap} playRef={play.buttonRef} />
      {problemsOpen && <ProblemsPanel onClose={closeProblems} fixFirst={fixFirst} />}
      {play.node}
      <div className="workspace">
        <Sidebar current={route.section} />
        <main id="main" className="main" tabIndex={-1}>
          <SaveBanners />
          <LazyBoundary key={route.section}>
            <Suspense fallback={<p role="status">Loading&hellip;</p>}>
              <Screen route={route} />
            </Suspense>
          </LazyBoundary>
        </main>
      </div>
    </div>
  )
}
