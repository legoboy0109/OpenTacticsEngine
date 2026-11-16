// Helpers for the Skills screen's tests (E2 of docs/SRPG.md): switching skills on, making a skill, a rule's group, screenshots.
import type { Locator, Page } from '@playwright/test'
import { editorUrl, expect, section } from './fixtures'

/** Resizes the window to the page's height first: a full-page capture would smear the sticky sidebar over the content. */
export const shot = async (page: Page, name: string): Promise<void> => {
  if (!process.env.OTE_SHOT_DIR) return
  const w = page.viewportSize()!.width
  await page.setViewportSize({ width: w, height: 900 })
  const h = await page.evaluate(() => document.documentElement.scrollHeight)
  await page.setViewportSize({ width: w, height: Math.min(h, 3200) })
  await page.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png` })
  await page.setViewportSize({ width: w, height: 900 })
}

/** New strategy-RPG games ship with the starter skills; these tests start from "skills on, none made yet". */
export async function emptySkills(page: Page, id: string): Promise<void> {
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  const put = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, skills: [] } })
  expect(put.ok(), await put.text()).toBeTruthy()
}

export async function openSkills(page: Page, id: string, skill?: string): Promise<void> {
  await page.goto(editorUrl(id, 'skills', skill))
  await expect(page.getByRole('heading', { name: 'Skills', level: 1 })).toBeVisible()
}

export async function newSkill(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New skill' }).click()
  const dlg = page.getByRole('dialog', { name: 'New skill' })
  await dlg.getByLabel('Name of the skill').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

export const rule = (page: Page, n: number): Locator => page.getByRole('group', { name: `Rule ${n}`, exact: true })
export const part = (parent: Locator, name: string): Locator => parent.getByRole('group', { name, exact: true })

export { section }
