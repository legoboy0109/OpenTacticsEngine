// The lesson "Sound" played through the real editor by a script that does what each step asks. For every step: before the action
// the step is the current one (not ticked); after it, it is ticked and the next one is current. It needs no sound file of its own:
// everything is chosen among the music and sounds the blank template comes with. Ends with the practice game loading in the
// real engine. (Headless browsers play no real audio: a play button is pressed, and the lesson only cares that it was.)
import { requirePlayer, saved } from './fixtures'
import {
  expect, expectCurrent, expectLoadsInEngine, expectRingOn, expectTicked, learnState, panel, readGame, ring, sectionLink, stepTitle, test,
} from './learnkit'
import { shot, startLesson, expectRingOnOneOf } from './learnstart'
import { canvas, tab } from './mapkit'

const PRACTICE = 'practice-sound'
test.use({ viewport: { width: 1920, height: 1080 } })

test('@smoke the whole lesson: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, lh, 'sound', 'Listen to the music', PRACTICE)
  await expect(page).toHaveURL(/\/sound$/)
  await expect(panel(page).getByText('Step 1 of 6')).toBeVisible()
  expect(await learnState(lh)).toMatchObject({ lessons: { sound: { state: 'running', step: 0, game: PRACTICE } } })

  // 1. Listen: the ring is on a play button; being on the Sound screen is not the work.
  await expectRingOn(page, 'sound-play')
  await shot(page, 'sound-1')
  await expectCurrent(page, 'Listen to the music')
  await page.getByTestId('play-music:title').click()
  await expectTicked(page, 'Listen to the music', 'Turn a track down')

  // 2. Volume: a track turned down but still louder than the template's own setting (the battle music begins at 80) is no change.
  await expectRingOn(page, 'track-volume')
  await shot(page, 'sound-2')
  await page.getByLabel('battle: Volume').fill('90')
  await page.waitForTimeout(1600)
  await expectCurrent(page, 'Turn a track down')
  await page.getByLabel('battle: Volume').fill('5')
  await page.getByLabel('battle: Volume').fill('50')
  await expectCurrent(page, 'Turn a track down') // waits for a pause in typing
  await expectTicked(page, 'Turn a track down', 'Pick music for a map')

  // 3. Map music: the ring goes to the sidebar link, then the Map tab, then the Music box. A turn music is not the map's music.
  await expectRingOn(page, 'nav-maps')
  await sectionLink(page, 'Maps').click()
  await expect(canvas(page)).toBeVisible()
  await expectRingOnOneOf(page, 'map-music|map-tab-map|list-item', 'map-tab-map')
  await tab(page, 'Map').click()
  await expectRingOnOneOf(page, 'map-music|map-tab-map|list-item', 'map-music')
  await shot(page, 'sound-3')
  const mapMusic = page.getByTestId('map-music')
  await mapMusic.getByLabel('Music on the enemy’s turn').selectOption({ label: 'enemy' })
  await expectCurrent(page, 'Pick music for a map')
  await mapMusic.getByLabel('Music', { exact: true }).selectOption({ label: 'No music' })
  await expectCurrent(page, 'Pick music for a map') // "No music" is not a track
  await mapMusic.getByLabel('Music', { exact: true }).selectOption({ label: 'enemy' })
  await expectTicked(page, 'Pick music for a map', 'Change what a hit sounds like')

  // 4. Event sound: silence for a hit is not a different sound.
  await expectRingOn(page, 'nav-sound')
  await sectionLink(page, 'Sound').click()
  await expectRingOn(page, 'sound-event-hit')
  await shot(page, 'sound-4')
  await expectCurrent(page, 'Change what a hit sounds like')
  const hit = page.getByTestId('slot-hit').getByRole('combobox')
  await hit.selectOption({ label: 'Nothing' })
  await expectCurrent(page, 'Change what a hit sounds like')
  await hit.selectOption({ label: 'critical' })
  await page.getByTestId('play-slot:hit').click() // the step says to press its play button, which changes nothing
  await expectTicked(page, 'Change what a hit sounds like', 'Give a weapon its own sound')

  // 5. Weapon sound: "Take me there" opens the sword. Silence for one event is not a sound.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/weapons\/sword$/)
  await expectRingOn(page, 'own-sound-hit')
  await shot(page, 'sound-5')
  await expectCurrent(page, 'Give a weapon its own sound')
  await page.getByLabel('Sounds: When it misses').selectOption({ label: 'Silent' })
  await expectCurrent(page, 'Give a weapon its own sound')
  await page.getByLabel('Sounds: When it hits').selectOption({ label: 'critical' })
  await expectTicked(page, 'Give a weapon its own sound', 'Play it and listen')

  // 6. Play: the step ticks when the player says it is ready.
  await saved(page)
  await expectRingOn(page, 'play')
  await expectCurrent(page, 'Play it and listen')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('learn-end')).toBeVisible()
  await expect(stepTitle(page)).toHaveCount(0)
  await expect(page.getByTestId('learn-end')).toContainText('You finished the lesson')
  await expect(page.getByTestId('learn-end')).toContainText('A music track you turned down.')
  await expect(ring(page)).toHaveCount(0)
  await shot(page, 'sound-end')
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()

  // What was made, on disk, loaded by the real engine. No sound file was added.
  await saved(page)
  const audio = readGame(lh, PRACTICE, 'data/audio.json')
  expect(audio.music.battle.volume).toBe(50)
  expect(audio.use.sounds.hit).toBe('critical')
  const map = readGame(lh, PRACTICE, 'maps/start.json')
  expect(map).toMatchObject({ music: 'enemy', enemyTurnMusic: 'enemy' })
  expect(readGame(lh, PRACTICE, 'data/weapons.json').weapons[0].sounds).toEqual({ miss: '', hit: 'critical' })
  await expectLoadsInEngine(lh, PRACTICE)
})
