import { createRoot } from 'react-dom/client'
import { App } from './App'
import './styles/tokens.css'
import './styles/base.css'
import './styles/shell.css'
import './styles/components.css'
import './styles/screens.css'
import './styles/map.css'
import './styles/play.css'

createRoot(document.getElementById('root')!).render(<App />)
