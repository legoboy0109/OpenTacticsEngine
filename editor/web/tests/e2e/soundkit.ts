// Helpers for the sound tests: sound files made on the fly (WAV) or taken from the repo's audio fixtures (Ogg), a game with
// no sound at all, driving the Sound screen like a person would, and running the REAL player on what the editor wrote.
import { execFileSync } from 'node:child_process'
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import path from 'node:path'
import type { APIRequestContext, Locator, Page } from '@playwright/test'
import { diskPath, editorUrl, expect, gamesDir } from './fixtures'

const repo = path.resolve(import.meta.dirname, '../../../..')

/** A 16-bit mono PCM WAV of `seconds` of a quiet tone (the pitch tells files apart in a debugger, nothing else). */
export function wav(seconds = 0.5, rate = 22050, hz = 440, channels = 1): Buffer {
  const frames = Math.round(seconds * rate)
  const data = Buffer.alloc(frames * 2 * channels)
  for (let i = 0; i < frames; i++) {
    const v = Math.round(Math.sin((2 * Math.PI * hz * i) / rate) * 3000)
    for (let c = 0; c < channels; c++) data.writeInt16LE(v, (i * channels + c) * 2)
  }
  const head = Buffer.alloc(44)
  head.write('RIFF', 0, 'latin1'); head.writeUInt32LE(36 + data.length, 4); head.write('WAVE', 8, 'latin1')
  head.write('fmt ', 12, 'latin1'); head.writeUInt32LE(16, 16); head.writeUInt16LE(1, 20); head.writeUInt16LE(channels, 22)
  head.writeUInt32LE(rate, 24); head.writeUInt32LE(rate * 2 * channels, 28); head.writeUInt16LE(2 * channels, 32); head.writeUInt16LE(16, 34)
  head.write('data', 36, 'latin1'); head.writeUInt32LE(data.length, 40)
  return Buffer.concat([head, data])
}

/** A real Ogg Vorbis file (0.5 s, mono, 22.05 kHz) from the repo's audio test fixtures. */
export const ogg = (): Buffer => readFileSync(path.join(repo, 'tests/fixtures/audio/tone_mono_22k.ogg'))

export const file = (name: string, buffer: Buffer, mimeType = name.endsWith('.ogg') ? 'audio/ogg' : 'audio/wav') => ({ name, mimeType, buffer })

/** A game from a template with no sound at all (no audio.json, nothing in audio/), whatever the template ships. */
export async function silentGame(request: APIRequestContext, game: { create(template: string, name?: string): Promise<string> }, template: string, name: string): Promise<string> {
  const id = await game.create(template, name)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  if (doc.audio) {
    // Clean the overrides and the music keys first (the loader would refuse names that no longer exist), then the file.
    const strip = <T extends { sounds?: unknown }>(l: T[] | null) => l?.map((x) => { const { sounds: _s, ...r } = x; return r })
    const maps: Record<string, unknown> = {}
    for (const [mid, m] of Object.entries<any>(doc.maps)) { const { music: _a, playerTurnMusic: _b, enemyTurnMusic: _c, ...r } = m; maps[mid] = r }
    const body: Record<string, unknown> = { baseRevision: doc.revision, audio: false, weapons: strip(doc.weapons), units: strip(doc.units), maps }
    if (doc.items) body.items = strip(doc.items)
    expect((await request.put(`/api/projects/${id}`, { data: body })).ok()).toBeTruthy()
  }
  for (const f of (await (await request.get(`/api/projects/${id}/audio`)).json()).audio) {
    expect((await request.delete(`/api/projects/${id}/${f.file}`)).ok(), f.file).toBeTruthy()
  }
  return id
}

export async function openSound(page: Page, game: string): Promise<void> {
  await page.goto(editorUrl(game, 'sound'))
  await expect(page.getByRole('heading', { name: 'Sound', level: 1 })).toBeVisible()
}

/** Uploads through "Add music" / "Add a sound" and names the result in the dialog that follows. */
export async function addTrackFromFile(page: Page, f: ReturnType<typeof file>, name: string): Promise<void> {
  await page.getByTestId('add-music-input').setInputFiles(f)
  const dlg = page.getByRole('dialog', { name: 'Name this music' })
  await dlg.getByLabel('Name', { exact: true }).fill(name)
  await dlg.getByRole('button', { name: 'Add', exact: true }).click()
}

export async function addSoundFromFile(page: Page, f: ReturnType<typeof file>, name: string): Promise<void> {
  await page.getByTestId('add-sound-input').setInputFiles(f)
  const dlg = page.getByRole('dialog', { name: 'Name this sound' })
  await dlg.getByLabel('Name', { exact: true }).fill(name)
  await dlg.getByRole('button', { name: 'Add', exact: true }).click()
}

/** The picker of one row of "When things happen" (by the key of the moment, e.g. "titleMusic", "hit"). */
export const slot = (page: Page, key: string): Locator => page.getByTestId(`slot-${key}`).getByRole('combobox')

/** What the shared preview element is doing right now. */
export const previewState = (page: Page) => page.evaluate(() => {
  const a = document.querySelector('audio[data-testid="sound-preview"]') as HTMLAudioElement | null
  return a ? { paused: a.paused, time: a.currentTime, src: a.currentSrc, ended: a.ended } : null
})

// ---- the real player ----

export const playerBin = (): string => path.resolve(import.meta.dirname, '../..', process.env.OTE_PLAYER_BIN ?? '../../build/engine/player/ote-player')

export interface PlayerRun { output: string; log: string; status: number }

/** Runs the native player headless on a game of the e2e games folder, writing its audio log. Throws when the binary is missing
 *  (use `playerAvailable()` first to skip with a clear message). */
export function runPlayer(gameId: string, extra: string[] = []): PlayerRun {
  const tmp = mkdtempSync(path.join(tmpdir(), 'ote-player-'))
  const logFile = path.join(tmp, 'audio.log')
  try {
    let output = ''
    let status = 0
    try {
      output = execFileSync(playerBin(), ['--headless', '--no-settings', '--no-saving', '--games', gamesDir(), '--game', gameId, '--frames', '5', '--audio-log', logFile, ...extra],
        { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], timeout: 60_000 })
    } catch (e) {
      const err = e as { stdout?: string; stderr?: string; status?: number }
      output = `${err.stdout ?? ''}${err.stderr ?? ''}`
      status = err.status ?? 1
    }
    return { output, log: existsSync(logFile) ? readFileSync(logFile, 'utf8') : '', status }
  } finally {
    rmSync(tmp, { recursive: true, force: true })
  }
}

/** The player's output has no load error and no audio warning (a missing file or a wrong name would show here). */
export const playerClean = (run: PlayerRun): string[] => run.output.split('\n').filter((l) => /\[ERROR\]|Cannot load|\[warn\] audio|audio:/.test(l))

export const audioDisk = (id: string): any => JSON.parse(readFileSync(diskPath(id, 'data/audio.json'), 'utf8'))

// ---- seeding through the API (fast set-up for tests that are not about adding files) ----

/** Puts sound files into audio/ and writes data/audio.json (plus any other sections in `extra`), like another program would. */
export async function seedSound(request: APIRequestContext, id: string, files: Record<string, Buffer>, audio: object, extra: Record<string, unknown> = {}): Promise<void> {
  for (const [name, data] of Object.entries(files)) {
    const res = await request.put(`/api/projects/${id}/audio/${name}`, { data })
    expect(res.status(), await res.text()).toBe(201)
  }
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, audio, ...extra } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

/** A small sound game on the strategy-rpg template: two tracks (one a jingle), two sounds, the standard set, a weapon's own sound,
 *  a map's music, a story event and a campaign scene, all naming them. Returns the game id. */
export async function soundGame(request: APIRequestContext, game: { create(template: string, name?: string): Promise<string> }, name = 'Sound Game'): Promise<string> {
  const id = await silentGame(request, game, 'strategy-rpg', name)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const mapId = doc.game.maps[0].id
  const weapons = doc.weapons.map((w: any, i: number) => (i === 0 ? { ...w, sounds: { hit: 'hit', miss: 'step' } } : w))
  const map = { ...doc.maps[mapId], music: 'battle', enemyTurnMusic: 'win',
    events: [{ id: 'intro', when: { battle: 'start' }, do: [{ playMusic: 'battle', fade: 1 }, { playSound: 'hit' }] }] }
  await seedSound(request, id,
    { 'battle.ogg': ogg(), 'win.wav': wav(0.3, 22050, 880), 'hit.wav': wav(0.5), 'step.wav': wav(0.4, 22050, 330), 'spare.wav': wav(0.2, 22050, 220) },
    {
      music: { battle: { file: 'audio/battle.ogg' }, win: { file: 'audio/win.wav', loop: false } },
      sounds: { hit: { file: 'audio/hit.wav' }, step: { files: ['audio/step.wav', 'audio/hit.wav'] } },
      use: { titleMusic: 'battle', mapMusic: 'battle', victory: 'win', sounds: { hit: 'hit', step: 'step' } },
    },
    { weapons, maps: { [mapId]: map },
      campaign: { name: 'Story', chapters: [{ id: 'c1', name: 'One', map: mapId, before: { music: 'battle', commands: [{ playSound: 'hit' }] }, after: [{ playMusic: 'win' }] }] } })
  return id
}
