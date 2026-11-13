import { test as base, expect, type APIRequestContext, type Page } from '@playwright/test'
import { readFileSync, writeFileSync } from 'node:fs'
import path from 'node:path'

export { expect }

/** Every test creates its own game from a template, so tests never touch each other. */
export const test = base.extend<{ game: GameFactory }>({
  baseURL: async ({}, use) => { await use(process.env.OTE_E2E_URL!) },
  game: async ({ request }, use) => { await use(new GameFactory(request)) },
})

let counter = 0

export class GameFactory {
  constructor(private request: APIRequestContext) {}

  /** POST /api/projects with a unique id; returns the id. */
  async create(template: string, name = 'Test Game'): Promise<string> {
    const id = `t${Date.now().toString(36)}${(counter++).toString(36)}${Math.random().toString(36).slice(2, 6)}`
    const res = await this.request.post('/api/projects', { data: { id, name, template } })
    expect(res.status(), await res.text()).toBe(201)
    return id
  }

  async problems(id: string): Promise<Array<{ file: string; message: string }>> {
    const res = await this.request.get(`/api/projects/${id}/problems`)
    return (await res.json()).problems
  }
}

export const gamesDir = (): string => process.env.OTE_E2E_GAMES!
export const diskPath = (id: string, rel: string): string => path.join(gamesDir(), id, rel)
export const readDisk = (id: string, rel: string): any => JSON.parse(readFileSync(diskPath(id, rel), 'utf8'))
export const writeDisk = (id: string, rel: string, text: string): void => writeFileSync(diskPath(id, rel), text)

export const editorUrl = (id: string, section = 'game', item?: string): string => `/p/${id}/${section}${item ? `/${item}` : ''}`

/** Wait until the top bar says everything is written. */
export async function saved(page: Page): Promise<void> {
  await expect(page.getByRole('status').filter({ hasText: /^Saved$/ })).toBeVisible()
}

/** Sidebar / section switcher link (the game name link in the top bar can share a word). */
export const section = (page: Page, name: string) => page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name, exact: true })

/** No horizontal scrolling: the page is never wider than the window. */
export async function expectNoHScroll(page: Page): Promise<void> {
  const wide = await page.evaluate(() => document.scrollingElement!.scrollWidth - window.innerWidth)
  expect(wide, 'page is wider than the viewport').toBeLessThanOrEqual(0)
}

/** Play tests need the web player build. Missing = a FAILURE with the fix spelled out, never a silent skip. */
export function requirePlayer(): void {
  if (!process.env.OTE_E2E_PLAYER) {
    throw new Error(
      'The web player is not built, so the Play tests cannot run. Build it:\n' +
      '  PATH="$HOME/.local/share/emsdk:$HOME/.local/share/emsdk/upstream/emscripten:$PATH" EM_CONFIG="$HOME/.local/share/emsdk/.emscripten" \\\n' +
      '    emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web\n' +
      'then run the tests with OTE_PLAYER_DIR=../../build-web/engine/player (the default, relative to editor/web).')
  }
}

/** Adds a second map ("second", a copy of the first) through the API, like a second author would. */
export async function addSecondMap(request: APIRequestContext, id: string): Promise<string> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const first = doc.game.maps[0]
  doc.game.maps.push({ id: 'second', name: 'Second Map', file: 'maps/second.json' })
  doc.maps.second = { ...doc.maps[first.id], name: 'Second Map' }
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game, maps: doc.maps } })
  expect(res.ok()).toBeTruthy()
  return 'second'
}
