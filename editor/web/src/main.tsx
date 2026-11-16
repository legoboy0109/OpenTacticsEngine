import { createRoot } from 'react-dom/client'
import { loadHexViews } from './api/hexViews'
import { App } from './App'
import './styles/tokens.css'
import './styles/base.css'
import './styles/shell.css'
import './styles/components.css'
import './styles/screens.css'
import './styles/map.css'
import './styles/art.css'
import './styles/story.css'
import './styles/sound.css'
import './styles/campaign.css'
import './styles/play.css'
import './styles/learn.css'

void loadHexViews() // ask the helper which kinds of view hexagon games may use, before the Game screen needs it
createRoot(document.getElementById('root')!).render(<App />)
