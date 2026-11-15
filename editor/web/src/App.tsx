import { LearnHost } from './learn/LearnHost'
import { PickerScreen } from './screens/picker/PickerScreen'
import { Link, useRoute } from './router'
import { ProjectShell } from './shell/ProjectShell'
import { ProjectProvider } from './store/context'

export function App() {
  return (
    <>
      <Screens />
      <LearnHost />
    </>
  )
}

function Screens() {
  const route = useRoute()
  if (route.kind === 'picker') return <PickerScreen />
  if (route.kind === 'project') {
    return (
      <ProjectProvider id={route.id} key={route.id}>
        <ProjectShell route={route} />
      </ProjectProvider>
    )
  }
  return (
    <div className="center-msg">
      <h1>Page not found</h1>
      <Link to="/" className="btn primary">Back to all games</Link>
    </div>
  )
}
