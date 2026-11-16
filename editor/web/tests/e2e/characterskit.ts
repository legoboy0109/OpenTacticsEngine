// Helpers for the Characters screen's tests.
import type { Page } from '@playwright/test'
import { editorUrl, section } from './fixtures'

export async function switchOn(page: Page, id: string): Promise<void> {
  await page.goto(editorUrl(id, 'rules'))
  await page.getByTestId('srpg-switches').getByRole('switch', { name: 'This game has characters' }).check()
  await section(page, 'Characters').click()
}

export async function newCharacter(page: Page, name: string): Promise<void> {
  // On a phone an open form hides the list: go back to it first.
  const back = page.getByRole('button', { name: /^Characters$/ })
  if (await back.isVisible()) await back.click()
  await page.getByRole('button', { name: 'New character' }).click()
  await page.getByLabel('Name of the character').fill(name)
  await page.getByRole('button', { name: 'Create' }).click()
}
