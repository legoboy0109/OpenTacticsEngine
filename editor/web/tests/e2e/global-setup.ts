// Starts the real helper on a temp copy of the repo's games and templates.
import { spawn, type ChildProcess } from 'node:child_process'
import { cpSync, existsSync, mkdtempSync, rmSync } from 'node:fs'
import { createServer } from 'node:net'
import { tmpdir } from 'node:os'
import path from 'node:path'

const webDir = path.resolve(import.meta.dirname, '../..')
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

export default async function globalSetup(): Promise<() => Promise<void>> {
  const bin = path.resolve(webDir, process.env.OTE_EDITOR_BIN ?? '../../build/ote-editor')
  if (!existsSync(bin)) throw new Error(`ote-editor binary not found at ${bin}. Build it, or set OTE_EDITOR_BIN.`)
  if (!existsSync(path.join(webDir, 'dist/index.html'))) throw new Error('editor/web/dist is missing: run "npm run build" first.')

  const tmp = mkdtempSync(path.join(tmpdir(), 'ote-e2e-'))
  cpSync(path.join(repo, 'games'), path.join(tmp, 'games'), { recursive: true })
  cpSync(path.join(repo, 'editor/templates'), path.join(tmp, 'templates'), { recursive: true })

  const port = await freePort()
  const proc = spawn(bin, [
    '--games', path.join(tmp, 'games'), '--templates', path.join(tmp, 'templates'),
    '--assets', path.join(repo, 'assets'), '--web', path.join(webDir, 'dist'), '--port', String(port),
  ], { cwd: webDir, stdio: ['ignore', 'ignore', 'pipe'] })
  let log = ''
  proc.stderr?.on('data', (d) => { log += String(d); if (log.length > 20000) log = log.slice(-10000) })

  const url = `http://127.0.0.1:${port}`
  try { await waitUp(url, proc) } catch (e) { proc.kill(); throw new Error(`${(e as Error).message}\n${log}`) }
  process.env.OTE_E2E_URL = url
  process.env.OTE_E2E_GAMES = path.join(tmp, 'games')

  return async () => {
    proc.kill()
    rmSync(tmp, { recursive: true, force: true })
  }
}
