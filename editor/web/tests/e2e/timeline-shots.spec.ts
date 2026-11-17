// Screenshots of the campaign timeline (run with SHOTS=1; they are written to the folder in timelinekit.ts's SHOTS).
import { expect, test } from './fixtures'
import { branchingCampaign, box, openTimeline, SHOTS, shotTo, straightCampaign, timelineGame } from './timelinekit'

test.skip(!process.env.SHOTS, 'screenshots are only taken on request (SHOTS=1)')
test.use({ viewport: { width: 1440, height: 1000 }, deviceScaleFactor: 2 })

const fit = async (page: import('@playwright/test').Page) => { await page.getByRole('button', { name: 'Fit all' }).click(); await page.waitForTimeout(400) }

test('shots: desktop straight campaign', async ({ page }) => {
  const id = timelineGame(straightCampaign(5, true))
  await openTimeline(page, id)
  await fit(page)
  await shotTo(page, 'desktop-straight')
})

test('shots: desktop with branches, a loop and an unreached chapter (light and dark)', async ({ page }) => {
  const camp = branchingCampaign()
  camp.chapters.push({ id: 'lost', name: 'The Lost Chapter', map: 'summit' })
  camp.chapters.find((c: any) => c.id === 'home').next = 'crossing'
  camp.chapters.find((c: any) => c.id === 'home').after = [{ choice: 'Again?', answers: [{ text: 'Once more', do: [{ goto: 'crossing' }] }, { text: 'Enough' }] }]
  const id = timelineGame(camp)
  await openTimeline(page, id)
  await fit(page)
  await shotTo(page, 'desktop-branches')
  await page.getByTestId('timeline-view').screenshot({ path: SHOTS + '/desktop-branches-view.png' })
  await page.emulateMedia({ colorScheme: 'dark' })
  await shotTo(page, 'desktop-branches-dark')
})

test('shots: expanded level with a selected chapter and a problem badge', async ({ page }) => {
  const camp = branchingCampaign()
  camp.chapters.find((c: any) => c.id === 'valley').before = [{ say: '', speakerName: 'Narrator' }]
  const id = timelineGame(camp)
  await openTimeline(page, id)
  await fit(page)
  await page.getByTestId('expand-crossing').click()
  await page.getByTestId('chip-crossing').click()
  for (let i = 0; i < 3; i++) await page.getByRole('button', { name: 'Zoom in' }).click()
  await page.locator('[data-node-open="crossing:level"]').focus()
  await page.keyboard.press('ArrowLeft')
  await page.keyboard.press('ArrowRight')
  await expect(box(page, 'crossing:level')).toBeVisible()
  await page.waitForTimeout(400)
  await shotTo(page, 'desktop-expanded')
  await page.getByTestId('timeline-view').screenshot({ path: SHOTS + '/desktop-expanded-view.png' })
})

test('shots: 40 chapters, zoomed out', async ({ page }) => {
  const camp = straightCampaign(40, true)
  camp.chapters[0].before = [{ choice: 'Tutorial?', speakerName: 'M', answers: [{ text: 'Yes', do: [{ goto: 'ch40' }] }, { text: 'No' }] }]
  for (const i of [5, 10, 15, 20, 25, 30]) camp.chapters[i - 1].after = [{ choice: 'Which way?', speakerName: 'M', answers: [{ text: 'Left', do: [{ goto: `ch${i + 3}` }] }, { text: 'Right' }] }]
  camp.chapters[34].after = [{ goto: 'ch10' }]
  const id = timelineGame(camp)
  await openTimeline(page, id)
  await fit(page)
  await shotTo(page, 'desktop-40-fit')
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
  test('shots: phone', async ({ page }) => {
    const id = timelineGame()
    await openTimeline(page, id)
    await page.getByTestId('chip-crossing').tap()
    await shotTo(page, 'phone-top')
    await page.getByTestId('timeline-view').scrollIntoViewIfNeeded()
    await shotTo(page, 'phone-view')
  })
})
