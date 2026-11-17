// The Campaign screen (docs/EVENTS.md "Campaign") through the real editor: a campaign made from nothing with two chapters,
// every scene command (a nested If and a "go to"), all three rules; the exact data/campaign.json that was written and that
// the real game loader accepts it (/problems is empty). Then the demo's own campaign, deleting a map the campaign uses,
// removing the campaign, and a phone.
import { existsSync, readFileSync } from 'node:fs'
import type { Page } from '@playwright/test'
import { diskPath, editorUrl, expect, expectNoHScroll, readDisk, requirePlayer, saved, test } from './fixtures'
import { addSceneCommand, copyDemo, openCampaign, openChapter, scene, sceneForm } from './campaignkit'
import { openMap, tool } from './mapkit'
import { nameUnit, shot } from './storykit'

async function campaignOf(page: Page, id: string): Promise<any> {
  const res = await page.request.get(`/api/projects/${id}`)
  return (await res.json()).campaign
}

test('@smoke a campaign from nothing: two chapters, every scene command, a nested If and a go-to, all three rules: the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await game.create('economy-tactics', 'Story')

  // --- what the scenes can point at: a switch, a variable, two named units of the player, a second map
  await page.goto(`/p/${id}/rules`)
  await page.getByLabel('Add a switch').fill('Village saved')
  await page.getByLabel('Add a switch').press('Enter')
  await page.getByLabel('Add a variable').fill('Gold found')
  await page.getByLabel('Add a variable').press('Enter')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 2, 2, 'eirik')
  await nameUnit(page, 3, 3, { unitName: 'Tanky' })
  await page.goto(editorUrl(id, 'maps'))
  await page.getByRole('button', { name: 'Show map list' }).click()
  await page.getByRole('button', { name: 'New map' }).click()
  await page.getByLabel('Name of the map').fill('North Road')
  await page.keyboard.press('Enter')
  await expect(page).toHaveURL(/maps\/north_road/)
  await saved(page)
  expect(await game.problems(id)).toEqual([])
  expect(existsSync(diskPath(id, 'data/campaign.json'))).toBe(false)

  // --- no campaign yet: an explanation and a button (nothing is written until it is pressed)
  await openCampaign(page)
  await expect(page.getByTestId('no-campaign')).toContainText('A campaign makes it a story')
  await shot(page, 'campaign-empty')
  await page.getByRole('button', { name: 'Create a campaign' }).click()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json')).toEqual({ name: 'Story', chapters: [{ id: 'chapter_1', name: 'Chapter 1', map: 'crossroads' }] })
  expect(await game.problems(id)).toEqual([])

  // --- the story and its three rules
  await page.getByLabel('Campaign name').fill('The Long Road')
  await page.getByLabel('When a unit is defeated').selectOption({ label: 'It comes back next chapter' })
  await page.getByLabel('When a chapter is lost').selectOption({ label: 'The story ends' })
  await page.getByLabel('Free battles', { exact: true }).selectOption({ label: 'Only the maps I choose' })
  await expect(page.getByTestId('free-maps')).toBeVisible()
  await shot(page, 'campaign-free-battles')
  await page.getByTestId('free-maps').getByLabel('North Road').check()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json')).toMatchObject({ name: 'The Long Road', rules: { fallen: 'return', onDefeat: 'end' }, freeBattles: ['north_road'] })

  // --- chapter 2
  await page.getByRole('button', { name: 'Add a chapter' }).click()
  await page.getByLabel('Name of the chapter').fill('The Pass')
  await page.keyboard.press('Enter')
  await expect(page.getByTestId('chapter-form')).toBeVisible()
  await page.getByLabel('Map', { exact: true }).selectOption({ label: 'North Road' })
  await expect(page.getByRole('link', { name: 'Open this map in the map editor' })).toHaveAttribute('href', `/p/${id}/maps/north_road`)
  await page.getByLabel('Next chapter').selectOption({ label: 'Chapter 1' })

  // --- chapter 1: the scene before the battle, one command of every kind
  await openChapter(page, 'Chapter 1')
  await page.getByLabel('Name', { exact: true }).fill('The Crossing')
  await expect(scene(page, 'before').getByRole('option', { name: 'Go to a chapter' })).toHaveCount(1) // both scenes can send the story elsewhere (docs/EVENTS.md "Dialogue choices")
  await expect(scene(page, 'before').getByRole('option', { name: 'Add a unit' })).toHaveCount(0)
  await addSceneCommand(page, 'before', 'Before the battle', 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'before').getByLabel('Name shown').fill('Narrator')
  await sceneForm(page, 'before').getByLabel('What is said').fill('Three days later.')
  await addSceneCommand(page, 'before', 'Before the battle', 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Infantry (eirik)' })
  await sceneForm(page, 'before').getByLabel('Portrait').fill('eirik')
  await sceneForm(page, 'before').getByLabel('What is said').fill('Forward!')
  await addSceneCommand(page, 'before', 'Before the battle', 'Turn a switch on or off')
  await sceneForm(page, 'before').getByLabel('Switch', { exact: true }).selectOption({ label: 'Village saved' })
  await addSceneCommand(page, 'before', 'Before the battle', 'Set a variable')
  await sceneForm(page, 'before').getByLabel('Set it to').fill('3')
  await addSceneCommand(page, 'before', 'Before the battle', 'Add to a variable')
  await sceneForm(page, 'before').getByLabel('Add', { exact: true }).fill('2')
  await addSceneCommand(page, 'before', 'Before the battle', 'Give resources')
  await sceneForm(page, 'before').getByLabel('Which side').selectOption({ label: 'Blue Army' })
  await sceneForm(page, 'before').getByLabel('Amount of Funds').fill('200')
  await addSceneCommand(page, 'before', 'Before the battle', 'If… then… otherwise…')
  const cond = (n: number) => sceneForm(page, 'before').getByRole('group', { name: `If 7: Condition ${n}` })
  await cond(1).getByLabel('Is', { exact: true }).selectOption({ label: 'On' })
  await sceneForm(page, 'before').getByLabel('If 7: Add a condition…').selectOption({ label: 'A variable compares to a number' })
  await cond(2).getByLabel('Comparison').selectOption({ label: 'is at least' })
  await cond(2).getByLabel('Number', { exact: true }).fill('5')
  await sceneForm(page, 'before').getByLabel('If 7: Add a condition…').selectOption({ label: 'A unit is alive' })
  await cond(3).getByLabel('Which unit').selectOption({ label: 'Infantry (eirik)' })
  await sceneForm(page, 'before').getByLabel('If 7: Add a condition…').selectOption({ label: 'A unit is defeated' })
  await cond(4).getByLabel('Which unit').selectOption({ label: 'Tanky (tanky)' })
  // the If's own lists: Then (a line and a nested If with both branches) and Otherwise
  const then = 'Before the battle, If 7: Then'
  await addSceneCommand(page, 'before', then, 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Infantry (eirik)' })
  await sceneForm(page, 'before').getByLabel('What is said').fill('Good.')
  await addSceneCommand(page, 'before', then, 'If… then… otherwise…')
  const inner = (n: number) => sceneForm(page, 'before').getByRole('group', { name: `If 2: Condition ${n}` })
  await inner(1).getByLabel('Is', { exact: true }).selectOption({ label: 'Off' })
  await addSceneCommand(page, 'before', `${then}, If 2: Then`, 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'before').getByLabel('Name shown').fill('Narrator')
  await sceneForm(page, 'before').getByLabel('What is said').fill('Nested yes.')
  await addSceneCommand(page, 'before', `${then}, If 2: Otherwise`, 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'before').getByLabel('Name shown').fill('Narrator')
  await sceneForm(page, 'before').getByLabel('What is said').fill('Nested no.')
  await addSceneCommand(page, 'before', 'Before the battle, If 7: Otherwise', 'Say something')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'before').getByLabel('Name shown').fill('Narrator')
  await sceneForm(page, 'before').getByLabel('What is said').fill('Not yet.')

  // --- chapter 1: the scene after the battle may send the story somewhere
  await addSceneCommand(page, 'after', 'After the battle', 'Say something')
  await sceneForm(page, 'after').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'after').getByLabel('Name shown').fill('Narrator')
  await sceneForm(page, 'after').getByLabel('What is said').fill('Well fought.')
  await addSceneCommand(page, 'after', 'After the battle', 'If… then… otherwise…')
  await sceneForm(page, 'after').getByRole('group', { name: 'If 2: Condition 1' }).getByLabel('Switch', { exact: true }).selectOption({ label: 'Village saved' })
  await addSceneCommand(page, 'after', 'After the battle, If 2: Then', 'Go to a chapter')
  await sceneForm(page, 'after').getByLabel('Next chapter').selectOption({ label: 'The Pass' })
  await shot(page, 'campaign-chapter-open')

  // --- chapter 2: only an after scene
  await openChapter(page, 'The Pass')
  await addSceneCommand(page, 'after', 'After the battle', 'Say something')
  await sceneForm(page, 'after').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'after').getByLabel('Name shown').fill('Narrator')
  await sceneForm(page, 'after').getByLabel('What is said').fill('The end.')

  await saved(page)
  const file = readDisk(id, 'data/campaign.json')
  expect(file).toEqual({
    name: 'The Long Road',
    chapters: [
      {
        id: 'chapter_1', name: 'The Crossing', map: 'crossroads',
        before: [
          { say: 'Three days later.', speakerName: 'Narrator' },
          { say: 'Forward!', speaker: 'eirik', portrait: 'eirik' },
          { setSwitch: 'village_saved', to: true },
          { setVariable: 'gold_found', to: 3 },
          { addVariable: 'gold_found', by: 2 },
          { giveResource: { funds: 200 }, side: 'blue' },
          {
            if: [{ switch: 'village_saved' }, { variable: 'gold_found', op: '>=', value: 5 }, { alive: 'eirik' }, { dead: 'tanky' }],
            then: [
              { say: 'Good.', speaker: 'eirik' },
              { if: [{ switch: 'village_saved', is: false }], then: [{ say: 'Nested yes.', speakerName: 'Narrator' }], else: [{ say: 'Nested no.', speakerName: 'Narrator' }] },
            ],
            else: [{ say: 'Not yet.', speakerName: 'Narrator' }],
          },
        ],
        after: [
          { say: 'Well fought.', speakerName: 'Narrator' },
          { if: [{ switch: 'village_saved' }], then: [{ goto: 'the_pass' }] },
        ],
      },
      { id: 'the_pass', name: 'The Pass', map: 'north_road', after: [{ say: 'The end.', speakerName: 'Narrator' }], next: 'chapter_1' },
    ],
    rules: { fallen: 'return', onDefeat: 'end' },
    freeBattles: ['north_road'],
  })
  expect(await game.problems(id)).toEqual([]) // the real game loader accepts all of it
  await expect(page.getByTestId('campaign-checks')).toHaveCount(0)

  // --- the flow preview and the one-line summaries
  await openCampaign(page)
  await expect(page.getByTestId('flow-chapter_1')).toContainText('The Pass')
  await expect(page.getByTestId('flow-the_pass')).toContainText('The Crossing')
  await expect(page.getByTestId('flow-the_pass')).not.toContainText('Nothing leads here')
  await expect(page.getByRole('button', { name: /^The Crossing/ }).first()).toContainText('Narrator says “Three days later.”, Infantry says “Forward!”')
  await shot(page, 'campaign-flow')

  // --- reorder: chapter 2 first (the order is the story); "Chapter 1" is no longer the start, so nothing leads to it
  await openChapter(page, 'The Pass')
  await page.getByRole('button', { name: 'Move chapter up' }).click()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters.map((c: any) => c.id)).toEqual(['the_pass', 'chapter_1'])
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the demo campaign shows without problems and is not rewritten when nothing is edited', async ({ page }) => {
  const id = copyDemo()
  const before = readFileSync(diskPath(id, 'data/campaign.json'), 'utf8')
  await page.goto(editorUrl(id, 'campaign'))
  await expect(page.getByLabel('Campaign name')).toHaveValue('The Ford War')
  await expect(page.getByLabel('When a unit is defeated')).toHaveValue('gone')
  await expect(page.getByLabel('When a chapter is lost')).toHaveValue('retry')
  await expect(page.getByLabel('Free battles', { exact: true })).toHaveValue('all')
  await expect(page.getByRole('list', { name: 'Chapters' }).getByRole('button')).toHaveCount(2)
  await expect(page.getByTestId('campaign-checks')).toHaveCount(0)
  await expect(page.getByTestId('campaign-hints')).toHaveCount(0)
  await openChapter(page, 'Skirmish at the Ford')
  await expect(page.getByLabel('Map', { exact: true })).toHaveValue('bridge')
  await expect(scene(page, 'before').getByRole('button', { name: /^1\. Narrator says/ })).toBeVisible()
  await expect(scene(page, 'after').getByRole('button', { name: /^1\. If the switch/ })).toBeVisible()
  await scene(page, 'after').getByRole('button', { name: /^3\. Gives side/ }).click()
  await expect(sceneForm(page, 'after').getByLabel('Amount of Gold')).toHaveValue('200')
  await shot(page, 'campaign-demo')
  await openChapter(page, 'The North Road')
  await expect(page.getByTestId('flow-ford')).toContainText('The North Road')
  await page.waitForTimeout(1200) // longer than the autosave pause: nothing may be written
  expect(readFileSync(diskPath(id, 'data/campaign.json'), 'utf8')).toBe(before)
  const probs = await page.request.get(`/api/projects/${id}/problems`)
  expect((await probs.json()).problems).toEqual([])
})

test('@smoke deleting a map the campaign uses asks with a "used by" list, clears the chapter (scenes kept) and one undo brings it all back', async ({ page }) => {
  const id = copyDemo()
  await page.goto(editorUrl(id, 'maps', 'bridge'))
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg.getByTestId('map-delete-usages')).toContainText('Campaign, chapter “Skirmish at the Ford”: its map is cleared (the scenes are kept; pick another map)')
  await shot(page, 'campaign-map-delete')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  const campaign = await campaignOf(page, id)
  expect(campaign.chapters[0].map).toBe('')
  expect(campaign.chapters[0].before).toHaveLength(2) // its scenes are kept
  expect(campaign.freeBattles).toBe('all')
  await openCampaign(page)
  await expect(page.getByTestId('campaign-checks')).toContainText('Chapter “Skirmish at the Ford”: pick a map.')
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems.length).toBeGreaterThan(0)
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect((await campaignOf(page, id)).chapters[0].map).toBe('bridge')
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems).toEqual([])
})

test('@smoke deleting a switch the scenes use cleans them up, and the Rules screen warns about a missing one', async ({ page }) => {
  const id = copyDemo()
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByTestId('campaign-checks')).toHaveCount(0)
  await page.getByRole('button', { name: /Delete .*Sera joined/ }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Campaign, chapter “Skirmish at the Ford”, “After the battle”, command 1')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(JSON.stringify(await campaignOf(page, id))).not.toContain('sera_joined')
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems).toEqual([])
})

test('@smoke removing the campaign asks first, deletes the file (the game goes back to single battles), and undo brings it back', async ({ page }) => {
  const id = copyDemo()
  const original = await (async () => { await page.goto(editorUrl(id, 'campaign')); return campaignOf(page, id) })()
  await expect(page.getByLabel('Campaign name')).toBeVisible()
  await page.getByRole('button', { name: 'Remove the campaign' }).click()
  const dlg = page.getByRole('dialog', { name: 'Remove the campaign?' })
  await expect(dlg).toContainText('single battles')
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  expect(existsSync(diskPath(id, 'data/campaign.json'))).toBe(true)
  await page.getByRole('button', { name: 'Remove the campaign' }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Remove' }).click()
  await expect(page.getByRole('button', { name: 'Create a campaign' })).toBeVisible()
  await saved(page)
  expect(existsSync(diskPath(id, 'data/campaign.json'))).toBe(false)
  expect(await campaignOf(page, id)).toBeNull()
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(await campaignOf(page, id)).toEqual(original)
})

test('a broken campaign file blocks only the Campaign screen and is not overwritten', async ({ page }) => {
  const id = copyDemo()
  const { writeFileSync } = await import('node:fs')
  writeFileSync(diskPath(id, 'data/campaign.json'), '{ not json')
  await page.goto(editorUrl(id, 'campaign'))
  await expect(page.getByText('The campaign can’t be opened')).toBeVisible()
  await expect(page.getByRole('button', { name: 'Create a campaign' })).toHaveCount(0)
  await page.goto(editorUrl(id, 'rules'))
  await expect(page.getByRole('heading', { name: 'Rules', level: 1 })).toBeVisible()
  expect(readFileSync(diskPath(id, 'data/campaign.json'), 'utf8')).toBe('{ not json')
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the Campaign screen at 360 px has no sideways scroll: empty, list, flow, an open chapter with scenes and a form', async ({ page, game }) => {
    const id = await game.create('economy-tactics', 'Phone')
    await page.goto(editorUrl(id, 'campaign'))
    await expect(page.getByRole('button', { name: 'Create a campaign' })).toBeVisible()
    await expectNoHScroll(page)
    await shot(page, 'campaign-phone-empty')
    await page.getByRole('button', { name: 'Create a campaign' }).click()
    await expect(page.getByTestId('flow')).toBeVisible()
    await page.getByLabel('Free battles', { exact: true }).selectOption({ label: 'Only the maps I choose' })
    await expectNoHScroll(page)
    await shot(page, 'campaign-phone-list')
    await page.getByRole('list', { name: 'Chapters' }).getByRole('button').first().click()
    await expect(page.getByTestId('chapter-form')).toBeVisible()
    await addSceneCommand(page, 'before', 'Before the battle', 'Say something')
    await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
    await sceneForm(page, 'before').getByLabel('Name shown').fill('A narrator with quite a long name indeed')
    await sceneForm(page, 'before').getByLabel('What is said').fill('A long line of text that has to wrap inside the narrow screen without making the page wider than the phone.')
    await addSceneCommand(page, 'after', 'After the battle', 'If… then… otherwise…')
    await expectNoHScroll(page)
    await shot(page, 'campaign-phone-chapter')
    await page.getByRole('button', { name: /Chapters/ }).click()
    await expectNoHScroll(page)
  })
})

test('Play on the Campaign screen starts the story from chapter 1; the picker can go back to a single map', async ({ page }) => {
  requirePlayer()
  const id = copyDemo()
  await page.goto(editorUrl(id))
  await openCampaign(page)
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const panel = page.getByRole('dialog', { name: 'Play', exact: true })
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&campaign=1`)
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  await expect(panel.getByLabel('Map')).toHaveValue('*campaign')
  await panel.getByLabel('Map').selectOption('bridge')
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=bridge`)
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})
