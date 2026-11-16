// The lesson "Characters and skills" played through the real editor by a script that does what each step says. Each step is current
// (not ticked) before its action and ticked after it; near misses must not tick. Ends with the practice game loading in the real
// engine. With LEARN_SHOTS=<folder> pictures are saved for looking at the steps by eye (the window is made 360 px wide for the
// "phone-" ones and put back).
import type { Page } from '@playwright/test'
import { clickCell, pickSide, tool } from './mapkit'
import { requirePlayer, saved } from './fixtures'
import { answered, expect, expectCurrent, expectRingOn, expectTicked, learnButton, learnList, learnState, panel, readGame, ring, sectionLink, test } from './learnkit'
import { expectRingOnOneOf } from './learnstart'
import { finish, playAndClose, practiceOf, shot as desktopShot, startLesson } from './learntopic'

/** A picture of the window as it is, and (when pictures are on) another one at phone width: the ring must be on the right thing there too. */
async function shot(page: Page, name: string): Promise<void> {
  await desktopShot(page, name)
  const dir = process.env.LEARN_SHOTS
  if (!dir) return
  const wide = page.viewportSize()!
  await page.setViewportSize({ width: 360, height: 780 })
  await page.waitForTimeout(1200) // the layout settles and the ring scrolls its control into view
  await page.screenshot({ path: `${dir}/phone-${name}.png` })
  await page.setViewportSize(wide)
  await page.waitForTimeout(400)
}

test.use({ viewport: { width: 1920, height: 1080 } })
const { id: PRACTICE, name: PRACTICE_NAME } = practiceOf('characters-and-skills', 'Characters and skills')
const switchFor = (page: Page, name: string) => page.getByTestId('srpg-switches').getByRole('switch', { name })

test('@smoke Characters and skills: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, 'characters', 'Switch on characters')
  await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/rules$`))
  await expect(panel(page).getByText('Step 1 of 10')).toBeVisible()
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe(PRACTICE_NAME)
  expect(await learnState(lh)).toMatchObject({ lessons: { characters: { state: 'running', step: 0, game: PRACTICE } } })

  // 1. Characters on. Trap: switching on skills is not it.
  await expectRingOn(page, 'srpg-characters')
  await shot(page, 'characters-1')
  await switchFor(page, 'This game has skills').check()
  await expectCurrent(page, 'Switch on characters')
  await switchFor(page, 'This game has skills').uncheck()
  await switchFor(page, 'This game has characters').check()
  await expectTicked(page, 'Switch on characters', 'Make a character')

  // 2. A character with a personal bonus: a name alone is not enough; a typed number ticks after a pause.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/characters$/)
  await expectRingOnOneOf(page, 'character-stats|new-item', 'new-item')
  await shot(page, 'characters-2')
  await page.getByRole('button', { name: 'New character' }).click()
  await page.getByRole('dialog').getByLabel('Name of the character').fill('Mira')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expect(page).toHaveURL(/\/characters\/mira$/)
  await expectCurrent(page, 'Make a character') // named, but no personal bonus yet
  await expectRingOnOneOf(page, 'character-stats|new-item', 'character-stats')
  await shot(page, 'characters-2b')
  await page.getByLabel('HP personal bonus').fill('1')
  await page.getByLabel('HP personal bonus').fill('5')
  await expectCurrent(page, 'Make a character') // the tick waits for a pause in typing
  await expectTicked(page, 'Make a character', 'Put them on the map')

  // 3. On the map. Trap: a plain soldier from the Classes tab is not the character.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expectRingOnOneOf(page, 'unit-characters-tab|tool-units', 'tool-units')
  await tool(page, 'Units').click()
  await pickSide(page, 'Blue').click()
  await page.getByRole('radiogroup', { name: 'Classes' }).getByRole('radio', { name: 'Soldier' }).click()
  await clickCell(page, 2, 4)
  await expectCurrent(page, 'Put them on the map')
  await expectRingOnOneOf(page, 'unit-characters-tab|tool-units', 'unit-characters-tab')
  await shot(page, 'characters-3')
  await page.getByRole('tab', { name: 'Characters', exact: true }).click()
  await page.getByRole('radiogroup', { name: 'Characters' }).getByRole('radio', { name: /Mira/ }).click()
  await clickCell(page, 2, 3)
  await expectTicked(page, 'Put them on the map', 'Switch on skills')

  // 4. Skills on.
  await sectionLink(page, 'Rules').click()
  await expectRingOn(page, 'srpg-skills')
  await shot(page, 'characters-4')
  await switchFor(page, 'This game has skills').check()
  await expectTicked(page, 'Switch on skills', 'Add the starter skills')

  // 5. The starter set: opening the installer is not adding.
  await sectionLink(page, 'Skills').click()
  await expectRingOn(page, 'starter-open')
  await page.getByTestId('starter-open').click()
  await expectCurrent(page, 'Add the starter skills')
  await page.getByRole('dialog').getByRole('button', { name: /^Add \d+ skills$/ }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Done' }).click()
  await expectTicked(page, 'Add the starter skills', 'Give them a skill')

  // 6. A personal skill. Trap: the learned list needs levelling up and is not this step.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/characters\/mira$/)
  await expectRingOn(page, 'character-skills-add')
  await shot(page, 'characters-6')
  await page.getByLabel('Skills: Add a skill…').selectOption('tough')
  await expectTicked(page, 'Give them a skill', 'Switch on levelling up')

  // 7. Levelling up (the game's own switch, on the Rules screen).
  await sectionLink(page, 'Rules').click()
  await expectRingOn(page, 'progression')
  await shot(page, 'characters-7')
  await page.getByRole('switch', { name: 'Experience and levels' }).check()
  await expectTicked(page, 'Switch on levelling up', 'A skill learned at a level')

  // 8. A learned skill.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expectRingOn(page, 'character-skills-learns')
  await shot(page, 'characters-8')
  await page.getByRole('button', { name: 'Add a learned skill' }).click()
  await expectTicked(page, 'A skill learned at a level', 'Make a skill of your own')

  // 9. A skill of your own: a new skill is valid at once, but its rule is still the one it came with.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expectRingOnOneOf(page, 'skill-rule|new-item', 'new-item')
  await shot(page, 'characters-9')
  await page.getByRole('button', { name: 'New skill' }).click()
  await page.getByRole('dialog').getByLabel('Name of the skill').fill('Sharp Eyes')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expect(page).toHaveURL(/\/skills\/sharp_eyes$/)
  await expectCurrent(page, 'Make a skill of your own')
  await expectRingOnOneOf(page, 'skill-rule|new-item', 'skill-rule')
  await shot(page, 'characters-9b')
  const rule = page.getByTestId('skill-rule').first()
  await rule.getByLabel('When', { exact: true }).selectOption('combat')
  await rule.getByRole('group', { name: 'Effect 1' }).getByLabel('Effect 1', { exact: true }).selectOption('damage')
  await expectTicked(page, 'Make a skill of your own', 'Play it')

  // 10. Play: the step ticks when the player says it is ready.
  await saved(page)
  await expectRingOn(page, 'play')
  await shot(page, 'characters-10')
  await expectCurrent(page, 'Play it')
  await playAndClose(page, () => expect(page.getByTestId('learn-end')).toBeVisible())
  await expect(ring(page)).toHaveCount(0)
  await shot(page, 'characters-end')

  await finish(page, lh, 'characters', 'A game that you played, with the character in it.', PRACTICE)
  // What was made, on disk (and loaded by the real engine inside `finish`).
  const mira = readGame(lh, PRACTICE, 'data/characters.json').characters[0]
  expect(mira).toMatchObject({ id: 'mira', name: 'Mira', type: 'soldier', stats: { hp: 5 }, skills: ['tough'] })
  expect(mira.learns).toHaveLength(1)
  expect(readGame(lh, PRACTICE, 'data/skills.json').skills.find((s: any) => s.id === 'sharp_eyes').blocks).toEqual([{ when: 'combat', do: [{ damage: 3 }] }])
  expect(readGame(lh, PRACTICE, 'maps/start.json').units).toContainEqual({ character: 'mira', side: 'blue', x: 2, y: 3 })
  expect(readGame(lh, PRACTICE, 'game.json').rules.progression.enabled).toBe(true)
})

test('the Learn list shows the lesson with its length and description', async ({ page, lh }) => {
  await answered(lh)
  await page.goto('/')
  await learnButton(page).click()
  const row = learnList(page).locator('[data-lesson="characters"]')
  await expect(row).toContainText('Characters and skills')
  await expect(row).toContainText('Make a named character with its own strengths, place it on the map, and give it skills.')
  await expect(row).toContainText('9 min')
  await row.scrollIntoViewIfNeeded()
  await shot(page, 'characters-list')
})
