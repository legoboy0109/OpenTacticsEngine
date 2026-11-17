// Helpers for the campaign timeline specs: a small neutral game (a copy of tests/fixtures/choice-story with a campaign of its own) and
// the few things a person does on the timeline.
import { cpSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import path from 'node:path'
import type { Locator, Page } from '@playwright/test'
import { diskPath, expect } from './fixtures'

export const SHOTS = '/tmp/claude-1000/-home-kadenm-Projects-OpenTacticsEngine/b3fc0737-752e-4599-9638-e7b4b8cfd654/scratchpad/timeline-shots'
const fixture = path.resolve(import.meta.dirname, '../../../../tests/fixtures/choice-story')

const say = (text: string) => ({ say: text, speakerName: 'Narrator' })

/** The campaign of the test game: chapter 1 asks "Play the tutorial?" (an answer jumps to the tutorial chapter) and, after the battle,
 *  "Which road?" (every answer jumps, so no plain arrow); the tutorial and both roads lead on to the last chapter. */
export function branchingCampaign(): any {
  return {
    name: 'The Crossroads',
    rules: { fallen: 'gone', onDefeat: 'retry' },
    chapters: [
      { id: 'crossing', name: 'The Crossing', map: 'ridge', next: 'valley',
        before: [
          say('We reach the ridge at dawn.'),
          { choice: 'Play the tutorial?', speakerName: 'Mara', answers: [{ text: 'Yes', do: [{ goto: 'tutorial' }] }, { text: 'No' }] },
          { setSwitch: 'crossed', to: true }],
        after: [
          { choice: 'Which road next?', speakerName: 'Mara', store: 'road', answers: [
            { text: 'The valley road', do: [{ goto: 'valley' }] }, { text: 'The summit path', do: [{ goto: 'summit' }] }] }] },
      { id: 'tutorial', name: 'Learning the Ropes', map: 'vale', before: [say('Let us practise first.')], next: 'valley' },
      { id: 'valley', name: 'The Valley', map: 'vale', next: 'home', before: [say('A green valley.')] },
      { id: 'summit', name: 'The Summit', map: 'summit', next: 'home', before: [say('Thin air up here.')] },
      { id: 'home', name: 'Home', map: 'vale', after: [say('Home at last.')] },
    ],
  }
}

/** A straight campaign of `n` chapters on the three maps in turn. */
export function straightCampaign(n: number, withScenes = false): any {
  const maps = ['ridge', 'vale', 'summit']
  return {
    name: 'A Long Road', rules: { fallen: 'gone', onDefeat: 'retry' },
    chapters: Array.from({ length: n }, (_, i) => ({ id: `ch${i + 1}`, name: `Chapter ${i + 1}`, map: maps[i % 3], ...(withScenes && i % 2 === 0 ? { before: [say(`Chapter ${i + 1} begins.`)] } : {}) })),
  }
}

/** Copies the fixture game under a fresh id, with `campaign` as its data/campaign.json. Returns the id. */
export function timelineGame(campaign: any = branchingCampaign()): string {
  const id = `tl${Date.now().toString(36)}${Math.random().toString(36).slice(2, 6)}`
  mkdirSync(diskPath(id, ''), { recursive: true })
  cpSync(fixture, diskPath(id, ''), { recursive: true })
  const g = diskPath(id, 'game.json')
  writeFileSync(g, readFileSync(g, 'utf8').replace('Crossroads Tale', `Timeline ${id}`))
  writeFileSync(diskPath(id, 'data/campaign.json'), JSON.stringify(campaign, null, 2))
  return id
}

export const timelineTab = (page: Page): Locator => page.getByRole('tab', { name: 'Timeline' })
export const view = (page: Page): Locator => page.getByTestId('timeline-view')
export const box = (page: Page, id: string): Locator => page.getByTestId(`box-${id}`)
export const chip = (page: Page, chapter: string): Locator => page.getByTestId(`chip-${chapter}`)
export const edge = (page: Page, from: string, to: string): Locator => page.getByTestId(`edge-${from}>${to}`)

export async function openTimeline(page: Page, id: string): Promise<void> {
  await page.goto(`/p/${id}/campaign`)
  await timelineTab(page).click()
  await expect(page.getByTestId('timeline')).toBeVisible()
}

export async function shotTo(page: Page, name: string): Promise<void> {
  mkdirSync(SHOTS, { recursive: true })
  await page.screenshot({ path: path.join(SHOTS, `${name}.png`) })
}
