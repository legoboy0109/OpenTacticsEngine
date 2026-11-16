// Starts the real ote-editor helper on a temp copy of the repo's games and templates.
// Used by global-setup (the main helper) and by the one test that needs a helper without a web player.
import { spawn, type ChildProcess } from 'node:child_process'
import { cpSync, existsSync, mkdirSync, mkdtempSync, rmSync, statSync, writeFileSync } from 'node:fs'
import { createServer } from 'node:net'
import { tmpdir } from 'node:os'
import path from 'node:path'

export const webDir = path.resolve(import.meta.dirname, '../..')
const repo = path.resolve(webDir, '../..')

function freePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const s = createServer()
    s.listen(0, '127.0.0.1', () => {
      const port = (s.address() as { port: number }).port
      s.close(() => resolve(port))
    })
    s.on('error', reject)
  })
}

async function waitUp(url: string, proc: ChildProcess): Promise<void> {
  for (let i = 0; i < 100; i++) {
    if (proc.exitCode !== null) throw new Error(`ote-editor exited early (code ${proc.exitCode})`)
    try { if ((await fetch(`${url}/api/info`)).ok) return } catch { /* not up yet */ }
    await new Promise((r) => setTimeout(r, 100))
  }
  throw new Error('ote-editor did not start in time')
}

export const editorBin = (): string => path.resolve(webDir, process.env.OTE_EDITOR_BIN ?? '../../build/ote-editor')
export const playerDir = (): string => path.resolve(webDir, process.env.OTE_PLAYER_DIR ?? '../../build-web/engine/player')
export const playerBuilt = (): boolean => existsSync(path.join(playerDir(), 'ote-player.wasm')) && existsSync(path.join(playerDir(), 'embed.html'))

export interface Helper { url: string; games: string; learnFile: string; stop: () => Promise<void> }

/** `player`: the --player folder (pass a folder that does not exist to simulate "web player not built").
 *  `learn`: the tutorial state the helper starts with. Always a temp file (--learn-file), never the real user data folder.
 *  'answered' (default) = the first-run offer was answered, so the game list looks as it always did; 'empty' = no state at all. */
export async function startHelper(player: string, learn: 'answered' | 'empty' = 'answered'): Promise<Helper> {
  const bin = editorBin()
  if (!existsSync(bin)) throw new Error(`ote-editor binary not found at ${bin}. Build it, or set OTE_EDITOR_BIN.`)
  if (!existsSync(path.join(webDir, 'dist/index.html'))) throw new Error('editor/web/dist is missing: run "npm run build" first.')
  const tmp = mkdtempSync(path.join(tmpdir(), 'ote-e2e-'))
  // Only the games the repository ships (demo-*) and its loose files: the maker's own games and practice games in games/ would
  // change what tests see (a "Practice: ..." name already taken, an extra row in the list).
  const gamesDir = path.join(repo, 'games')
  cpSync(gamesDir, path.join(tmp, 'games'), {
    recursive: true,
    filter: (src) => {
      const rel = path.relative(gamesDir, src)
      const top = rel.split(path.sep)[0]
      return rel === '' || top.startsWith('demo-') || (rel === top && !statSync(src).isDirectory())
    },
  })
  cpSync(path.join(repo, 'editor/templates'), path.join(tmp, 'templates'), { recursive: true })
  const learnFile = path.join(tmp, 'learn', 'learn.json')
  if (learn === 'answered') { mkdirSync(path.dirname(learnFile), { recursive: true }); writeFileSync(learnFile, JSON.stringify({ version: 1, offerAnswered: true, lessons: {} })) }
  const port = await freePort()
  const proc = spawn(bin, [
    '--games', path.join(tmp, 'games'), '--templates', path.join(tmp, 'templates'),
    '--assets', path.join(repo, 'assets'), '--web', path.join(webDir, 'dist'), '--player', player, '--learn-file', learnFile, '--port', String(port),
  ], { cwd: webDir, stdio: ['ignore', 'ignore', 'pipe'] })
  let log = ''
  proc.stderr?.on('data', (d) => { log += String(d); if (log.length > 20000) log = log.slice(-10000) })
  const url = `http://127.0.0.1:${port}`
  try { await waitUp(url, proc) } catch (e) { proc.kill(); throw new Error(`${(e as Error).message}\n${log}`) }
  return {
    url, games: path.join(tmp, 'games'), learnFile,
    stop: async () => { proc.kill(); rmSync(tmp, { recursive: true, force: true }) },
  }
}
