// Helpers for driving the Campaign screen like a person would: through labels, pickers and the sidebar.
import { cpSync, readFileSync, writeFileSync } from 'node:fs'
import type { Locator, Page } from '@playwright/test'
import { diskPath, expect } from './fixtures'

export const scene = (page: Page, which: 'before' | 'after'): Locator => page.getByTestId(`scene-${which}`)
/** The open command's form in a scene. */
export const sceneForm = (page: Page, which: 'before' | 'after'): Locator => scene(page, which).locator('.command-form')

/** Adds a command to one of a scene's lists (`list` is "Before the battle", "Before the battle, If 7: Then"...). */
export async function addSceneCommand(page: Page, which: 'before' | 'after', list: string, kind: string): Promise<void> {
  await scene(page, which).getByLabel(`${list}: Add a command…`, { exact: true }).selectOption({ label: kind })
}

export async function openCampaign(page: Page): Promise<void> {
  // (the link gains a "2 problems" note when the campaign has problems, so match the start of its name)
  await page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name: /^Campaign/ }).click()
  await expect(page.getByRole('heading', { name: 'Campaign', level: 1 })).toBeVisible()
}

/** Opens a chapter of the list by its name. */
export async function openChapter(page: Page, name: string): Promise<void> {
  await page.getByRole('list', { name: 'Chapters' }).getByRole('button', { name: new RegExp(`^${name}`) }).click()
  await expect(page.getByTestId('chapter-form')).toBeVisible()
}

/** A copy of the demo game under a fresh project id (the demo has a two-chapter campaign). Returns the id. */
export function copyDemo(): string {
  const id = `demo${Date.now().toString(36)}${Math.random().toString(36).slice(2, 6)}`
  cpSync(diskPath('demo-srpg', ''), diskPath(id, ''), { recursive: true })
  // Another game called "Emblem Demo" would confuse tests that look the demo up by name in the picker.
  const g = diskPath(id, 'game.json')
  writeFileSync(g, readFileSync(g, 'utf8').replace('Emblem Demo', `Copy ${id}`))
  return id
}
