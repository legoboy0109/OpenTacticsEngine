// The one controller of the running editor, wired to the real helper, the shared signal bus and the router.
import { api, learnApi } from '../api/client'
import { navigate, projectPath } from '../router'
import { LearnController } from './controller'
import { signalBus } from './signals'

export const learn = new LearnController({
  api: {
    getState: learnApi.get,
    putState: learnApi.put,
    listProjects: api.listProjects,
    createProject: api.createProject,
  },
  bus: signalBus,
  goTo: (game, section, item) => navigate(projectPath(game, section, item)),
})
