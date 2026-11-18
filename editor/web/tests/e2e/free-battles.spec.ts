// Free battles that unlock after the story is complete (docs/EVENTS.md "Campaign"): the Campaign screen's "When" choice and
// note, the exact file written (the object form only when needed, the old forms untouched), the game loader accepting it,
// and a phone.
import { readFileSync } from 'node:fs'
import { diskPath, editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { copyDemo } from './campaignkit'

const free = (page: import('@playwright/test').Page) => page.getByLabel('Free battles', { exact: true })
const when = (page: import('@playwright/test').Page) => page.getByLabel('When free battles can be played', { exact: true })
const note = (page: import('@playwright/test').Page) => page.getByLabel('Line shown while they are locked', { exact: true })

test('the demo campaign (the plain "all") is not rewritten, and "after the story" writes the object form the loader accepts', async ({ page }) => {
  const id = copyDemo()
  const before = readFileSync(diskPath(id, 'data/campaign.json'), 'utf8')
  await page.goto(editorUrl(id, 'campaign'))
  await expect(free(page)).toHaveValue('all')
  await expect(when(page)).toHaveValue('always')
  await expect(note(page)).toHaveCount(0) // only while the list is locked
  await page.waitForTimeout(1200)
  expect(readFileSync(diskPath(id, 'data/campaign.json'), 'utf8')).toBe(before)

  await when(page).selectOption({ label: 'After the story is complete' })
  await expect(note(page)).toBeVisible()
  await note(page).fill('Finish the story to unlock free battles.')
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').freeBattles).toEqual({ maps: 'all', when: 'storyComplete', lockedNote: 'Finish the story to unlock free battles.' })
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems).toEqual([])

  // Reopened: the choice shows; a chosen list keeps the object; back to "Always" with no note goes back to the plain form.
  await page.reload()
  await expect(when(page)).toHaveValue('storyComplete')
  await expect(note(page)).toHaveValue('Finish the story to unlock free battles.')
  await free(page).selectOption({ label: 'Only the maps I choose' })
  await page.getByTestId('free-maps').getByLabel('Skirmish at the Ford').check()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').freeBattles).toEqual({ maps: ['bridge'], when: 'storyComplete', lockedNote: 'Finish the story to unlock free battles.' })
  await note(page).fill('')
  await when(page).selectOption({ label: 'Always' })
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').freeBattles).toEqual(['bridge'])
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems).toEqual([])
})

test('a hand-written object form with nothing chosen shows a hint, and a bad "when" an error', async ({ page }) => {
  const id = copyDemo()
  const { writeFileSync } = await import('node:fs')
  const path = diskPath(id, 'data/campaign.json')
  const text = readFileSync(path, 'utf8')
  expect(text).toContain('"freeBattles": "all"')
  writeFileSync(path, text.replace('"freeBattles": "all"', '"freeBattles": { "maps": [], "when": "storyComplete" }'))
  await page.goto(editorUrl(id, 'campaign'))
  await expect(page.getByTestId('campaign-hints')).toContainText('no map is chosen, so nothing will ever unlock')
  writeFileSync(path, text.replace('"freeBattles": "all"', '"freeBattles": { "maps": "all", "when": "later" }'))
  await page.reload()
  await expect(page.getByTestId('campaign-checks')).toContainText('“when” must be “always” or “storyComplete”')
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the free battles controls at 360 px have no sideways scroll', async ({ page }) => {
    const id = copyDemo()
    await page.goto(editorUrl(id, 'campaign'))
    await when(page).selectOption({ label: 'After the story is complete' })
    await note(page).fill('A rather long line that says to finish the whole story before the free battles open.')
    await expect(note(page)).toBeVisible()
    await expectNoHScroll(page)
  })
})
