// Campaign parties (docs/EVENTS.md "Parties") through the real editor: the parties card (add, rename, order, delete with the clean-up), the "Party"
// commands in a scene's picker, a chapter's own party, the checks and hints, the timeline's party sign, and the exact data/campaign.json that
// was written, which the real game loader accepts (/problems is empty). Then the same screen at 360 px.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { addSceneCommand, openCampaign, openChapter, sceneForm } from './campaignkit'
import { straightCampaign, timelineGame } from './timelinekit'

const addParty = async (page: Page, name: string) => {
  await page.getByRole('button', { name: 'Add a party' }).click()
  await page.getByLabel('Name of the party').fill(name)
  await page.keyboard.press('Enter')
}

/** The Campaign screen of a three-chapter game on its list view, at either width. */
async function open(page: Page, id: string) {
  await page.goto(editorUrl(id))
  await openCampaign(page)
  await page.getByRole('tab', { name: 'List' }).click()
  await expect(page.getByTestId('no-parties')).toBeVisible()
}

test('@smoke parties: add two, switch in chapter 2, merge in chapter 3, a chapter party, the sign on the timeline, delete with clean-up; the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = timelineGame(straightCampaign(3))
  await open(page, id)

  // --- the parties card: nothing yet, then two parties; the first is the starting one
  await expect(page.getByTestId('no-parties')).toContainText('No parties')
  await addParty(page, 'The company')
  await addParty(page, 'The northern road')
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').parties).toEqual([{ id: 'the_company', name: 'The company' }, { id: 'the_northern_road', name: 'The northern road' }])
  await expect(page.getByTestId('party-the_company')).toContainText('The story starts with this party')
  await expect(page.getByTestId('party-the_northern_road')).not.toContainText('The story starts with this party')

  // --- renaming changes the name only
  await page.getByTestId('party-the_northern_road').getByLabel('Party 2: name').fill('The north road')
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').parties[1]).toEqual({ id: 'the_northern_road', name: 'The north road' })
  await page.getByTestId('party-the_northern_road').getByLabel('Party 2: name').fill('The northern road')

  // --- chapter 2: the party commands are in the picker under "Party"
  await openChapter(page, 'Chapter 2')
  const before = page.getByTestId('scene-before')
  await expect(before.getByRole('option', { name: 'Switch to another party' })).toHaveCount(1)
  await expect(before.getByRole('option', { name: 'Merge parties' })).toHaveCount(1)
  await addSceneCommand(page, 'before', 'Before the battle', 'Switch to another party')
  await sceneForm(page, 'before').getByLabel('Party to switch to').selectOption({ label: 'The northern road' })
  await expect(before).toContainText('Switches to the party “The northern road”')

  // --- chapter 3: back to the first, then merge (the merge form starts on north into the first party)
  await openChapter(page, 'Chapter 3')
  await addSceneCommand(page, 'before', 'Before the battle', 'Switch to another party')
  await sceneForm(page, 'before').getByLabel('Party to switch to').selectOption({ label: 'The company' })
  await expect(page.getByTestId('campaign-hints')).toContainText('“The northern road” is switched to but never merged')
  await addSceneCommand(page, 'before', 'Before the battle', 'Merge parties')
  await expect(sceneForm(page, 'before').getByRole('checkbox', { name: 'The northern road' })).toBeChecked()
  await expect(sceneForm(page, 'before').getByLabel('Merge into')).toHaveValue('the_company')
  await expect(before).toContainText('Merges “The northern road” into “The company”')
  // merged: the "never merged" note is gone. (The game's maps all place Mara on the player's side, so the note that the north party starts her fresh from the map stays.)
  await expect(page.getByTestId('campaign-hints')).not.toContainText('never merged')
  await expect(page.getByTestId('campaign-hints')).toContainText('so far only “The company” has them, not “The northern road”')
  await expect(page.getByTestId('campaign-checks')).toHaveCount(0)

  // --- a chapter's own party (chapter 1 starts the first party)
  await openChapter(page, 'Chapter 1')
  await page.getByLabel('Starts with party').selectOption({ label: 'The company' })
  await saved(page)
  const c = readDisk(id, 'data/campaign.json')
  expect(c.chapters[0].party).toBe('the_company')
  expect(c.chapters[1].before).toEqual([{ switchParty: 'the_northern_road' }])
  expect(c.chapters[2].before).toEqual([{ switchParty: 'the_company' }, { mergeParties: ['the_northern_road'], into: 'the_company' }])
  expect(await game.problems(id)).toEqual([])

  // --- the timeline shows a party sign on the boxes that change party
  await page.goto(editorUrl(id, 'campaign'))  // (a chapter is open: the timeline of the whole story is the screen without one)
  await page.getByRole('tab', { name: 'Timeline' }).click()
  const sign = (box: string) => page.getByTestId(`box-${box}`).locator('[data-marker="party"]')
  await expect(sign('ch2:before')).toHaveAttribute('aria-label', '1 party switch or merge')
  await expect(sign('ch3:before')).toHaveAttribute('aria-label', '2 party switches or merges')
  await expect(sign('ch1:level')).toHaveAttribute('aria-label', '1 party switch or merge')  // chapter 1 has no scene: its own party shows on its level
  await page.getByRole('tab', { name: 'List' }).click()

  // --- deleting a party says what it takes with it, and cleans up in the same step
  await page.getByRole('button', { name: 'Delete the party The northern road' }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Switches to the party “The northern road”')
  await expect(dlg).toContainText('Merges “The northern road” into “The company”')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  const after = readDisk(id, 'data/campaign.json')
  expect(after.parties).toEqual([{ id: 'the_company', name: 'The company' }])
  expect(after.chapters[1].before ?? []).toEqual([])  // its only command was the switch
  expect(after.chapters[2].before).toEqual([{ switchParty: 'the_company' }])  // the merge had nothing left to merge
  expect(after.chapters[0].party).toBe('the_company')
  expect(await game.problems(id)).toEqual([])
  // ... and the last party goes: the key leaves the file and the commands naming it go too
  await page.getByRole('button', { name: 'Delete the party The company' }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  const none = readDisk(id, 'data/campaign.json')
  expect('parties' in none).toBe(false)
  expect(none.chapters[0].party).toBeUndefined()
  expect(none.chapters[2].before ?? []).toEqual([])
  expect(await game.problems(id)).toEqual([])
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('parties at 360 px: the card, a party command with its form, the chapter field and the hints leave no sideways scroll', async ({ page }) => {
    const id = timelineGame(straightCampaign(2))
    await open(page, id)
    await expectNoHScroll(page)
    await addParty(page, 'A party with quite a long name indeed')
    await addParty(page, 'The northern road')
    await expect(page.getByTestId('parties')).toBeVisible()
    await expectNoHScroll(page)
    await openChapter(page, 'Chapter 2')
    await addSceneCommand(page, 'before', 'Before the battle', 'Merge parties')
    await expect(sceneForm(page, 'before').getByLabel('Merge into')).toBeVisible()
    await expectNoHScroll(page)
    await expect(page.getByLabel('Starts with party')).toBeVisible()
    await saved(page)
    expect(readDisk(id, 'data/campaign.json').chapters[1].before).toEqual([{ mergeParties: ['the_northern_road'], into: 'a_party_with_quite_a_long_name_indeed' }])
  })
})
