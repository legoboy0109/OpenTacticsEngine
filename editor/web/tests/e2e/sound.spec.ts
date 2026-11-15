// The Sound screen (docs/SOUND.md step 4) through the real editor and helper: files uploaded like a person would, a track and
// two sounds (one with two variants) made from them, the standard set filled in, a weapon's own sound, a map's music, a scene's
// music, the story commands; the exact files written, the uploaded bytes on disk, the helper's check (the player's loader) clean,
// the REAL player accepting the game, and the preview button really playing.
import { existsSync, readFileSync } from 'node:fs'
import { diskPath, editorUrl, expect, readDisk, saved, test } from './fixtures'
import { addSceneCommand, openCampaign, openChapter, scene } from './campaignkit'
import { openMap, tab } from './mapkit'
import { addCommand, form, newEvent, shot } from './storykit'
import { addSoundFromFile, addTrackFromFile, audioDisk, file, ogg, openSound, playerBin, playerClean, previewState, runPlayer, silentGame, slot, wav } from './soundkit'

test('@smoke a game gets music and sounds from uploads: files, names, the standard set, a weapon, a map, a scene and the story commands', async ({ page, request, game }) => {
  test.setTimeout(150_000)
  const id = await silentGame(request, game, 'strategy-rpg', 'Sound')
  const hitBytes = wav(2, 22050, 660)
  const stepBytes = wav(0.4, 22050, 330)
  const musicBytes = ogg()

  // --- no sound yet: what this is, and the two "Add" buttons (and nothing else offered)
  await openSound(page, id)
  await expect(page.getByRole('heading', { name: 'This game has no sound yet' })).toBeVisible()
  await expect(page.getByTestId('no-sound')).toContainText('Sound effects play for menus')
  await expect(page.getByRole('button', { name: 'Add music' })).toBeVisible()
  await expect(page.getByRole('button', { name: 'Add a sound' })).toBeVisible()
  await expect(page.getByRole('button', { name: /starter/i })).toHaveCount(0)
  await shot(page, 'sound-empty')
  // The pickers elsewhere stay hidden while there is nothing to pick.
  await page.goto(editorUrl(id, 'weapons'))
  await expect(page.getByTestId('sounds-weapons')).toHaveCount(0)
  await openSound(page, id)

  // --- two WAVs and one Ogg through the screen; the first creates audio.json
  await addTrackFromFile(page, file('Battle Theme.ogg', musicBytes), 'Battle')
  await expect(page.getByTestId('track-battle')).toBeVisible()
  await addSoundFromFile(page, file('Hit.wav', hitBytes), 'Hit')
  await expect(page.getByTestId('sound-hit')).toBeVisible()
  await addSoundFromFile(page, file('Step.wav', stepBytes), 'Step')
  await expect(page.getByTestId('sound-step')).toBeVisible()
  await saved(page)
  for (const [name, bytes] of [['battle_theme.ogg', musicBytes], ['hit.wav', hitBytes], ['step.wav', stepBytes]] as const) {
    expect(readFileSync(diskPath(id, `audio/${name}`)).equals(bytes), `audio/${name} is byte for byte what was uploaded`).toBe(true)
  }
  expect(audioDisk(id)).toEqual({ music: { battle: { file: 'audio/battle_theme.ogg' } }, sounds: { hit: { file: 'audio/hit.wav' }, step: { file: 'audio/step.wav' } } })
  await expect(page.getByTestId('sound-files')).toContainText('audio/battle_theme.ogg')
  await expect(page.getByTestId('sound-files')).toContainText('22050 Hz, mono')

  // --- one sound, two variants ("one is picked each time")
  await page.getByRole('button', { name: 'step: Add a variant' }).click()
  await expect(page.getByTestId('variant-step-2')).toBeVisible()
  await expect(page.getByLabel('step: Variant 2')).toHaveValue('audio/hit.wav')
  await saved(page)
  expect(audioDisk(id).sounds.step).toEqual({ files: ['audio/step.wav', 'audio/hit.wav'] })
  // ...and back to one variant: `file` again
  await page.getByRole('button', { name: 'step: Remove variant 2' }).click()
  await saved(page)
  expect(audioDisk(id).sounds.step).toEqual({ file: 'audio/step.wav' })
  await page.getByRole('button', { name: 'step: Add a variant' }).click()
  await saved(page)

  // --- the track: volume, repeats, repeat from
  const track = page.getByTestId('track-battle')
  await track.getByLabel('battle: Volume').fill('80')
  await track.getByLabel('battle: Repeat from').fill('0.25')
  await saved(page)
  expect(audioDisk(id).music.battle).toEqual({ file: 'audio/battle_theme.ogg', volume: 80, loopStart: 0.25 })
  await page.getByTestId('sound-hit').getByLabel('hit: Volume').fill('90')
  await saved(page)
  await shot(page, 'sound-music-sounds')

  // --- "When things happen": music slots and event slots, grouped and labelled
  await slot(page, 'titleMusic').selectOption('battle')
  await slot(page, 'mapMusic').selectOption('battle')
  await slot(page, 'cursor').selectOption('hit')
  await slot(page, 'step').selectOption('step')
  await slot(page, 'hit').selectOption('hit')
  await saved(page)
  await expect(page.getByRole('region', { name: 'Menus' })).toContainText('The cursor moves')
  await expect(page.getByRole('region', { name: 'Moving and fighting' })).toContainText('A unit takes a step')
  await expect(page.getByRole('region', { name: 'Economy' })).toContainText('Money comes in')
  await expect(page.getByRole('region', { name: 'Story' })).toContainText('Text appears in a conversation')
  await shot(page, 'sound-events')
  expect(audioDisk(id)).toEqual({
    music: { battle: { file: 'audio/battle_theme.ogg', volume: 80, loopStart: 0.25 } },
    sounds: { hit: { file: 'audio/hit.wav', volume: 90 }, step: { files: ['audio/step.wav', 'audio/hit.wav'] } },
    use: { titleMusic: 'battle', mapMusic: 'battle', sounds: { cursor: 'hit', step: 'step', hit: 'hit' } },
  })

  // --- the preview button really plays (a 2 second sound), and stops
  const play = page.getByTestId('play-sound:hit')
  await play.click()
  await expect.poll(async () => (await previewState(page))?.paused, { timeout: 8000 }).toBe(false)
  const t1 = (await previewState(page))!.time
  await expect.poll(async () => (await previewState(page))!.time, { timeout: 5000 }).toBeGreaterThan(t1)
  expect((await previewState(page))!.src).toContain('/file/audio/hit.wav')
  await expect(page.getByRole('button', { name: 'Stop hit' }).first()).toBeVisible()
  await page.getByRole('button', { name: 'Stop hit' }).first().click()
  await expect.poll(async () => (await previewState(page))?.paused).toBe(true)
  // a track repeats: after it ends it starts again (an Ogg file of half a second)
  await page.getByTestId('play-music:battle').click()
  await expect.poll(async () => (await previewState(page))?.paused, { timeout: 8000 }).toBe(false)
  await page.waitForTimeout(1300) // more than two times its length
  expect((await previewState(page))!.paused, 'a repeating track is still going').toBe(false)
  await page.getByTestId('play-music:battle').click()

  // --- the pickers elsewhere appear now: a weapon's own sound
  await page.goto(editorUrl(id, 'weapons'))
  const weapons = page.getByTestId('sounds-weapons')
  await expect(weapons).toBeVisible()
  await weapons.getByLabel('Sounds: When it hits').selectOption('hit')
  await weapons.getByLabel('Sounds: When it misses').selectOption({ label: 'Silent' })
  await saved(page)
  expect(readDisk(id, 'data/weapons.json').weapons[0].sounds).toEqual({ hit: 'hit', miss: '' })

  // --- a map's music (and "Same as the game" by default), and the story commands in an event
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  const mm = page.getByTestId('map-music')
  await expect(mm.getByLabel('Music', { exact: true })).toHaveValue('__none__')
  await expect(mm.getByLabel('Music', { exact: true }).locator('option:checked')).toHaveText('Same as the game')
  await mm.getByLabel('Music', { exact: true }).selectOption('battle')
  await mm.getByLabel('Music on the enemy’s turn').selectOption({ label: 'No music' })
  await tab(page, 'Events').click()
  await newEvent(page, 'intro')
  await addCommand(page, 'Then', 'Play music')
  await form(page).getByLabel('Music track').selectOption('battle')
  await form(page).getByLabel('Fade (seconds)').fill('1.5')
  await addCommand(page, 'Then', 'Play a sound')
  await form(page).getByLabel('Sound', { exact: true }).selectOption('hit')
  await addCommand(page, 'Then', 'Stop the music')
  await shot(page, 'sound-story-commands')
  await saved(page)
  const map = readDisk(id, 'maps/ford.json')
  expect(map.music).toBe('battle')
  expect(map.enemyTurnMusic).toBe('')
  expect('playerTurnMusic' in map).toBe(false)
  expect(map.events.at(-1).do).toEqual([{ playMusic: 'battle', fade: 1.5 }, { playSound: 'hit' }, { stopMusic: true }])
  await expect(page.getByTestId('event-editor')).toContainText('Plays the music “battle” (fades in over 1.5 seconds)')

  // --- a campaign scene's music: the object form only while it has music
  await openCampaign(page)
  await page.getByRole('button', { name: 'Create a campaign' }).click()
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toBeUndefined()
  await openChapter(page, 'Chapter 1')
  await scene(page, 'before').getByLabel('Before the battle: Music for this scene').selectOption('battle')
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toEqual({ music: 'battle' })
  await addSceneCommand(page, 'before', 'Before the battle', 'Play a sound')
  await scene(page, 'before').locator('.command-form').getByLabel('Sound', { exact: true }).selectOption('hit')
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toEqual({ music: 'battle', commands: [{ playSound: 'hit' }] })
  await shot(page, 'sound-scene')
  // taking the music off writes the plain list again
  await scene(page, 'before').getByLabel('Before the battle: Music for this scene').selectOption({ label: 'Keep what is playing' })
  await saved(page)
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toEqual([{ playSound: 'hit' }])
  await scene(page, 'before').getByLabel('Before the battle: Music for this scene').selectOption('battle')
  await saved(page)

  // --- the helper's check (the player's own sound loader) has nothing to say
  expect(await game.problems(id)).toEqual([])

  // --- and the REAL player accepts the game and asks for what we chose
  test.skip(!existsSync(playerBin()), `ote-player not found at ${playerBin()}: build it (cmake --build build) or set OTE_PLAYER_BIN; the rest of this test (the real player's check) was not run`)
  const run = runPlayer(id)
  expect(playerClean(run), run.output).toEqual([])
  expect(run.log).toContain('music play')
  const battle = runPlayer(id, ['--map', 'ford'])
  expect(playerClean(battle), battle.output).toEqual([])
  expect(battle.log, 'the map\'s music started in the real player').toMatch(/music play \S*audio\/battle_theme\.ogg fade=0\.5 loop=1/)
  expect(battle.log, 'the story\'s sound and stop ran').toMatch(/sound \S*audio\/hit\.wav vol=90[\s\S]*music stop/)
})
