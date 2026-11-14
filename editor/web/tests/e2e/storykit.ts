// Helpers for driving the story panels (Events, Objectives) like a person would: through labels and pickers.
import type { Locator, Page } from '@playwright/test'
import { expect } from './fixtures'
import { clickCell, tab } from './mapkit'

export const eventsTab = (page: Page) => tab(page, 'Events')
export const editor = (page: Page): Locator => page.getByTestId('event-editor')

/** Adds an event (the editor opens on it) and gives it a name. */
export async function newEvent(page: Page, name: string): Promise<void> {
  if (await editor(page).isVisible()) await page.getByRole('button', { name: 'All events' }).click()
  await page.getByRole('button', { name: 'Add an event' }).click()
  await expect(editor(page)).toBeVisible()
  await editor(page).getByLabel('Name', { exact: true }).fill(name)
}

/** Back to the list, then open an event by its name. */
export async function openEvent(page: Page, name: string): Promise<void> {
  if (await editor(page).isVisible()) await page.getByRole('button', { name: 'All events' }).click()
  await page.getByRole('button', { name: `Open event ${name}`, exact: true }).click()
  await expect(editor(page)).toBeVisible()
}

export const when = (page: Page, kind: string) => editor(page).getByLabel('When', { exact: true }).selectOption({ label: kind })

/** Adds a command to a list ("Then", or "If 1: Then"...) and leaves its form open. */
export async function addCommand(page: Page, list: string, kind: string): Promise<void> {
  await editor(page).getByLabel(`${list}: Add a command…`, { exact: true }).selectOption({ label: kind })
}

export const form = (page: Page): Locator => editor(page).locator('.command-form')

export const shot = async (page: Page, name: string) => { if (process.env.OTE_SHOT_DIR) await page.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png`, fullPage: true }) }

export async function createItem(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New item' }).click()
  const dlg = page.getByRole('dialog', { name: 'New item' })
  await dlg.getByLabel('Name of the item').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

/** Gives the unit on a square its story name (typing it, or by the button that makes one from the unit's name). */
export async function nameUnit(page: Page, x: number, y: number, storyName: string | { unitName: string }): Promise<void> {
  await clickCell(page, x, y)
  const props = page.getByTestId('unit-props')
  if (typeof storyName === 'string') await props.getByLabel('Story name').fill(storyName)
  else {
    await props.getByLabel('Name', { exact: true }).fill(storyName.unitName)
    await props.getByRole('button', { name: 'Fill from the name' }).click()
  }
}

