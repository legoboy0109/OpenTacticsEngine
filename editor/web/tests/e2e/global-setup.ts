// Starts the real helper (with the web player when it is built) on a temp copy of the repo's games.
import { playerBuilt, playerDir, startHelper } from './helper'

export default async function globalSetup(): Promise<() => Promise<void>> {
  const helper = await startHelper(playerDir())
  process.env.OTE_E2E_URL = helper.url
  process.env.OTE_E2E_GAMES = helper.games
  // Play tests check this and FAIL (not skip) with a clear message when the player is not built.
  process.env.OTE_E2E_PLAYER = playerBuilt() ? playerDir() : ''
  return helper.stop
}
