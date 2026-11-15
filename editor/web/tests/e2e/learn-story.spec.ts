// The lesson "Story events" played through the real editor by a script that does what each step asks. For every step: before the
// action the step is the current one (not ticked); after it, it is ticked and the next one is current. Ends with the practice game
// on disk loading in the real engine.
import { requirePlayer, saved } from './fixtures'
import {
  expect, expectCurrent, expectLoadsInEngine, expectRingOn, expectTicked, learnState, panel, readGame, ring, stepTitle, test,
} from './learnkit'
import { shot, startLesson, expectRingOnOneOf } from './learnstart'
import { canvas, clickCell, dragThrough, tab } from './mapkit'
import { addCommand, editor, form, when } from './storykit'

const PRACTICE = 'practice-story-events'
test.use({ viewport: { width: 1920, height: 1080 } })

test('@smoke the whole lesson: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, lh, 'story', 'Add an opening event', PRACTICE)
  await expect(panel(page).getByText('Step 1 of 9')).toBeVisible()
  expect(await learnState(lh)).toMatchObject({ lessons: { story: { state: 'running', step: 0, game: PRACTICE } } })

  // 1. Opening event. A game with one map opens it straight away.
  await expect(page).toHaveURL(/\/maps(\/start)?$/)
  await expect(canvas(page)).toBeVisible()

  // The ring is on the Events tab, then on "Add an event" once the tab is open. Opening the tab is not the work.
  await expectRingOnOneOf(page, 'add-event|map-tab-events|list-item', 'map-tab-events')
  await shot(page, 'story-1a')
  await tab(page, 'Events').click()
  await expectRingOnOneOf(page, 'add-event|map-tab-events|list-item', 'add-event')
  await shot(page, 'story-1b')
  await expectCurrent(page, 'Add an opening event')
  await page.getByRole('button', { name: 'Add an event' }).click()
  await expectTicked(page, 'Add an opening event', 'Make it say something')

  // 2. Say something: the new event is open; the ring is on "Add a command", then on the text box. Blank text does not count.
  await expectRingOnOneOf(page, 'say-text|add-command|event-open|list-item', 'add-command')
  await shot(page, 'story-2a')
  await addCommand(page, 'Then', 'Say something')
  await expectRingOnOneOf(page, 'say-text|add-command|event-open|list-item', 'say-text')
  await shot(page, 'story-2b')
  await form(page).getByLabel('What is said').fill('   ')
  await page.waitForTimeout(1600)
  await expectCurrent(page, 'Make it say something')
  await form(page).getByLabel('What is said').fill('Hold')
  await form(page).getByLabel('What is said').fill('Hold the bridge!')
  await expectCurrent(page, 'Make it say something') // waits for a pause in typing
  await expectTicked(page, 'Make it say something', 'Add a second event')

  // 3. Second event: back to the list, then add. Going back is not the work.
  await expectRingOnOneOf(page, 'add-event|events-back|list-item', 'events-back')
  await page.getByRole('button', { name: 'All events' }).click()
  await expectRingOnOneOf(page, 'add-event|events-back|list-item', 'add-event')
  await expectCurrent(page, 'Add a second event')
  await page.getByRole('button', { name: 'Add an event' }).click()
  await expectTicked(page, 'Add a second event', 'Wait for a unit to arrive')

  // 4. When: the new event still waits for the battle to start, which is not the work.
  await expectRingOnOneOf(page, 'event-when|list-item', 'event-when')
  await shot(page, 'story-4')
  await expectCurrent(page, 'Wait for a unit to arrive')
  await when(page, 'A unit reaches an area')
  await expectTicked(page, 'Wait for a unit to arrive', 'Pick the place')

  // 5. The place: the area starts as the top-left square. Pressing "Pick on the map" and clicking that same square is not a pick.
  await expectRingOnOneOf(page, 'pick-area|list-item', 'pick-area')
  await shot(page, 'story-5a')
  await editor(page).getByRole('button', { name: 'When: area: Pick on the map' }).click()
  await expectCurrent(page, 'Pick the place')
  await clickCell(page, 0, 0)
  await expectCurrent(page, 'Pick the place')
  await editor(page).getByRole('button', { name: 'When: area: Pick on the map' }).click()
  await dragThrough(page, [[7, 0], [7, 5]])
  await shot(page, 'story-5b')
  await expectTicked(page, 'Pick the place', 'Say something there')

  // 6. Say something there: the ring is on "Add a command" (the opening event's line does not count for this event).
  await expectRingOnOneOf(page, 'say-text|add-command|list-item', 'add-command')
  await addCommand(page, 'Then', 'Say something')
  await expectRingOnOneOf(page, 'say-text|add-command|list-item', 'say-text')
  await expectCurrent(page, 'Say something there')
  await form(page).getByLabel('What is said').fill('You')
  await form(page).getByLabel('What is said').fill('You made it across!')
  await expectTicked(page, 'Say something there', 'Tell the player the goal')

  // 7. Objective text: opening the tab is not the work.
  await expectRingOnOneOf(page, 'objective-text|map-tab-objectives|list-item', 'map-tab-objectives')
  await tab(page, 'Objectives').click()
  await expectRingOnOneOf(page, 'objective-text|map-tab-objectives|list-item', 'objective-text')
  await shot(page, 'story-7')
  await expectCurrent(page, 'Tell the player the goal')
  await page.getByTestId('objectives-panel').getByLabel('Objective text').fill('Survive 5 rounds')
  await expectTicked(page, 'Tell the player the goal', 'Add a way to win')

  // 8. A way to win: a way to LOSE is not the work.
  await expectRingOnOneOf(page, 'add-win|map-tab-objectives|list-item', 'add-win')
  await shot(page, 'story-8')
  await page.getByLabel('map: Add a way to lose…').selectOption({ label: 'A round begins' })
  await expectCurrent(page, 'Add a way to win')
  await page.getByLabel('map: Add a way to win…').selectOption({ label: 'Survive some rounds' })
  await expectTicked(page, 'Add a way to win', 'Play it')

  // 9. Play: the step ticks when the player says it is ready.
  await saved(page)
  await expectRingOn(page, 'play')
  await expectCurrent(page, 'Play it')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('learn-end')).toBeVisible()
  await expect(stepTitle(page)).toHaveCount(0)
  await expect(page.getByTestId('learn-end')).toContainText('You finished the lesson')
  await expect(page.getByTestId('learn-end')).toContainText('An objective that tells the player how to win.')
  await expect(ring(page)).toHaveCount(0)
  await shot(page, 'story-end')
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()

  // What was made, on disk, loaded by the real engine.
  await saved(page)
  const map = readGame(lh, PRACTICE, 'maps/start.json')
  expect(map.events).toEqual([
    { id: 'event_1', when: { battle: 'start' }, do: [{ say: 'Hold the bridge!' }] },
    { id: 'event_2', when: { reach: { x: 7, y: 0, w: 1, h: 6 } }, do: [{ say: 'You made it across!' }] },
  ])
  expect(map.objectives).toMatchObject({ text: 'Survive 5 rounds', win: [{ survive: 5 }] })
  await expectLoadsInEngine(lh, PRACTICE)
})
