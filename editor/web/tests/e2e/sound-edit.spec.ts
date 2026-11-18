// Changing sound that is already there: rename and delete with "used by" and one Undo, the upload refusals, deleting a sound
// FILE that is used, the amber checks, a broken audio.json, removing all sound, the unused files, and the sidebar entry.
import { existsSync, readdirSync, readFileSync, writeFileSync } from 'node:fs'
import { diskPath, editorUrl, expect, readDisk, saved, test } from './fixtures'
import { audioDisk, openSound, playerBin, playerClean, runPlayer, silentGame, soundGame, wav } from './soundkit'

const undo = (page: import('@playwright/test').Page) => page.getByRole('button', { name: 'Undo' }).click()

test('@smoke renaming a track follows every place that names it, in one Undo step', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Rename Sound')
  await openSound(page, id)
  // the sidebar: "Sound" sits right after "Art"
  const links = await page.getByRole('navigation', { name: 'Sections' }).getByRole('link').allInnerTexts()
  expect(links.map((l) => l.replace(/\s+\d+ problems?$/, '')).slice(links.findIndex((l) => l.startsWith('Art')), links.findIndex((l) => l.startsWith('Art')) + 2)).toEqual(['Art', 'Sound'])
  const before = { audio: audioDisk(id), map: readDisk(id, 'maps/ford.json'), campaign: readDisk(id, 'data/campaign.json') }

  await page.getByRole('button', { name: 'Rename battle' }).click()
  const dlg = page.getByRole('dialog', { name: 'Rename this music' })
  await dlg.getByLabel('Name', { exact: true }).fill('Fight Music')
  await dlg.getByRole('button', { name: 'Rename', exact: true }).click()
  await expect(page.getByTestId('track-fight_music')).toBeVisible()
  await saved(page)
  const audio = audioDisk(id)
  expect(Object.keys(audio.music)).toEqual(['fight_music', 'win'])
  expect(audio.use).toEqual({ titleMusic: 'fight_music', mapMusic: 'fight_music', victory: 'win', sounds: { hit: 'hit', step: 'step' } })
  const map = readDisk(id, 'maps/ford.json')
  expect(map.music).toBe('fight_music')
  expect(map.events[0].do[0]).toEqual({ playMusic: 'fight_music', fade: 1 })
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toEqual({ music: 'fight_music', commands: [{ playSound: 'hit' }] })
  expect(await game.problems(id)).toEqual([])

  await undo(page)
  await saved(page)
  expect({ audio: audioDisk(id), map: readDisk(id, 'maps/ford.json'), campaign: readDisk(id, 'data/campaign.json') }).toEqual(before)
})

test('@smoke deleting a track says where it is used, takes it out of all of them, and one Undo brings them back', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Delete Sound')
  await openSound(page, id)
  const before = { audio: audioDisk(id), map: readDisk(id, 'maps/ford.json'), campaign: readDisk(id, 'data/campaign.json'), weapons: readDisk(id, 'data/weapons.json') }

  await page.getByRole('button', { name: 'Delete battle' }).click()
  const dlg = page.getByRole('dialog', { name: 'Delete the music “battle”?' })
  await expect(dlg).toContainText('standard set')
  await expect(dlg).toContainText('Title screen')
  await expect(dlg).toContainText('Plays the music “battle”')
  await expect(dlg).toContainText('its music is taken off')
  await expect(dlg).toContainText('goes back to the game’s music')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await expect(page.getByTestId('track-battle')).toHaveCount(0)
  await saved(page)
  const audio = audioDisk(id)
  expect(Object.keys(audio.music)).toEqual(['win'])
  expect(audio.use).toEqual({ victory: 'win', sounds: { hit: 'hit', step: 'step' } })
  const map = readDisk(id, 'maps/ford.json')
  expect('music' in map).toBe(false)
  expect(map.enemyTurnMusic).toBe('win')
  expect(map.events[0].do).toEqual([{ playSound: 'hit' }])
  expect(readDisk(id, 'data/campaign.json').chapters[0].before).toEqual([{ playSound: 'hit' }])
  expect(await game.problems(id)).toEqual([])
  // the file is not part of this: it stays (now unused) until it is deleted on purpose
  expect(existsSync(diskPath(id, 'audio/battle.ogg'))).toBe(true)
  await expect(page.getByTestId('file-battle.ogg')).toContainText('Not used yet')

  await undo(page)
  await saved(page)
  expect({ audio: audioDisk(id), map: readDisk(id, 'maps/ford.json'), campaign: readDisk(id, 'data/campaign.json'), weapons: readDisk(id, 'data/weapons.json') }).toEqual(before)

  // a sound: the weapon's own sound and the standard set follow too
  await page.getByRole('button', { name: 'Delete hit', exact: true }).click()
  const sdlg = page.getByRole('dialog', { name: 'Delete the sound “hit”?' })
  await expect(sdlg).toContainText('Weapon')
  await sdlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  expect(audioDisk(id).sounds).toEqual({ step: { files: ['audio/step.wav', 'audio/hit.wav'] } })
  expect(audioDisk(id).use.sounds).toEqual({ step: 'step' })
  expect(readDisk(id, 'data/weapons.json').weapons[0].sounds).toEqual({ miss: 'step' })
  expect(readDisk(id, 'maps/ford.json').events[0].do).toEqual([{ playMusic: 'battle', fade: 1 }])
  expect(await game.problems(id)).toEqual([])
})

test('@smoke a file that is not a sound, a sound that is really another kind, and a file that is too big are refused plainly', async ({ page, request, game }) => {
  const id = await silentGame(request, game, 'strategy-rpg', 'Refuse Sound')
  await openSound(page, id)
  const input = page.getByTestId('add-music-input')
  await input.setInputFiles({ name: 'notes.wav', mimeType: 'audio/wav', buffer: Buffer.from('this is just a text file, not a sound at all') })
  await expect(page.getByText('“notes.wav” is not a sound file')).toBeVisible()
  await input.setInputFiles({ name: 'really-a-wav.ogg', mimeType: 'audio/ogg', buffer: wav(0.2) })
  await expect(page.getByText('“really-a-wav.ogg” is really a WAV file but its name ends in .ogg')).toBeVisible()
  const big = Buffer.concat([wav(0.1), Buffer.alloc(8 * 1024 * 1024)])
  await input.setInputFiles({ name: 'huge.wav', mimeType: 'audio/wav', buffer: big })
  await expect(page.getByText('“huge.wav” is larger than 8 MB')).toBeVisible()
  // nothing was named, uploaded or saved
  await expect(page.getByRole('dialog')).toHaveCount(0)
  // (a game made from a template that ships starter sounds keeps its emptied audio folder)
  expect(existsSync(diskPath(id, 'audio')) ? readdirSync(diskPath(id, 'audio')) : []).toEqual([])
  expect(existsSync(diskPath(id, 'data/audio.json'))).toBe(false)
  await expect(page.getByTestId('no-sound')).toBeVisible()
  // the helper refuses the same things on its own (a client that skipped the checks)
  const raw = await request.put(`/api/projects/${id}/audio/sneaky.wav`, { data: Buffer.from('not a sound') })
  expect(raw.status()).toBe(400)
  expect((await request.put(`/api/projects/${id}/audio/huge.wav`, { data: big })).status()).toBe(413)
})

test('deleting a sound FILE that tracks and sounds use asks first, takes them out in one Undo step, and deletes the file for good', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'File Sound')
  await openSound(page, id)
  await page.getByRole('button', { name: 'Delete the file audio/hit.wav' }).click()
  const dlg = page.getByRole('dialog', { name: 'Delete audio/hit.wav?' })
  await expect(dlg).toContainText('Sound “hit”')
  await expect(dlg).toContainText('Sound “step”')
  await expect(dlg).toContainText('Undo brings them back, but not the file')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await expect(page.getByText('audio/hit.wav was deleted.')).toBeVisible()
  await saved(page)
  expect(existsSync(diskPath(id, 'audio/hit.wav'))).toBe(false)
  const audio = audioDisk(id)
  expect(audio.sounds).toEqual({ step: { file: 'audio/step.wav' } })
  expect(audio.use.sounds).toEqual({ step: 'step' })
  expect(readDisk(id, 'data/weapons.json').weapons[0].sounds).toEqual({ miss: 'step' })
  expect(readDisk(id, 'maps/ford.json').events[0].do).toEqual([{ playMusic: 'battle', fade: 1 }])
  expect(await game.problems(id)).toEqual([])

  // an unused file goes straight after one question
  await page.getByRole('button', { name: 'Delete the file audio/spare.wav' }).click()
  const d2 = page.getByRole('dialog', { name: 'Delete audio/spare.wav?' })
  await expect(d2).toContainText('deleted from the game folder for good')
  await d2.getByRole('button', { name: 'Delete', exact: true }).click()
  await expect(page.getByTestId('file-spare.wav')).toHaveCount(0)
  expect(existsSync(diskPath(id, 'audio/spare.wav'))).toBe(false)
})

test('files nobody uses are marked, and can become music or a sound with a click', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Unused Sound')
  await openSound(page, id)
  await expect(page.getByTestId('file-spare.wav')).toContainText('Not used yet')
  await expect(page.getByTestId('file-hit.wav')).toContainText('Used by hit, step')
  await expect(page.getByTestId('file-hit.wav')).toContainText('22050 Hz, mono')
  await page.getByRole('button', { name: 'Use as a sound: audio/spare.wav' }).click()
  const dlg = page.getByRole('dialog', { name: 'Name this sound' })
  await expect(dlg.getByLabel('Name', { exact: true })).toHaveValue('spare')
  await dlg.getByRole('button', { name: 'Add', exact: true }).click()
  await expect(page.getByTestId('sound-spare')).toBeVisible()
  await saved(page)
  expect(audioDisk(id).sounds.spare).toEqual({ file: 'audio/spare.wav' })
  await expect(page.getByTestId('file-spare.wav')).toContainText('Used by spare')
})

test('what the player would refuse is shown in amber on the Sound screen and next to the thing it is about', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Check Sound')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  // a looping jingle, an unknown sound in a weapon, a missing file; the helper saves it all (work in progress is never refused)
  const audio = { ...doc.audio, music: { ...doc.audio.music, win: { file: 'audio/win.wav' } }, sounds: { ...doc.audio.sounds, ghost: { file: 'audio/ghost.wav' } } }
  const weapons = doc.weapons.map((w: any, i: number) => (i === 0 ? { ...w, sounds: { hit: 'nope' } } : w))
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, audio, weapons } })).ok()).toBeTruthy()
  await openSound(page, id)
  await expect(page.getByTestId('audio-checks')).toContainText('Standard set, “You win”: “win” repeats, but this plays once')
  await expect(page.getByTestId('audio-hints')).toContainText('audio/ghost.wav is not in the game, so this sound stays silent')
  await page.goto(editorUrl(id, 'weapons'))
  await expect(page.getByTestId('audio-checks')).toContainText('“nope”, which is not a sound of this game')
  // the helper agrees about the error (the loader stops at the first one) and not about the missing file, which the game only warns about
  const problems = await game.problems(id)
  expect(problems).toHaveLength(1)
  expect(problems[0].file).toBe('data/audio.json')
  expect(problems[0].message).toContain('win')
  // a story command naming a track that is not there shows on the map
  const map = { ...doc.maps.ford, events: [{ id: 'intro', when: { battle: 'start' }, do: [{ playMusic: 'nowhere' }] }] }
  const d2 = await (await request.get(`/api/projects/${id}`)).json()
  await request.put(`/api/projects/${id}`, { data: { baseRevision: d2.revision, maps: { ford: map } } })
  await page.goto(editorUrl(id, 'maps', 'ford'))
  await expect(page.getByTestId('map-warnings')).toContainText('“nowhere” is not a music track of this game')
})

test('a damaged audio.json blocks only the Sound screen and is never overwritten', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Broken Sound')
  const path = diskPath(id, 'data/audio.json')
  writeFileSync(path, '{ "music": ')
  await page.goto(editorUrl(id, 'sound'))
  await expect(page.getByText('Sound can’t be opened: data/audio.json could not be read.')).toBeVisible()
  await expect(page.getByRole('button', { name: 'Add music' })).toHaveCount(0)
  // the other screens work, and an edit there does not rewrite the file
  await page.goto(editorUrl(id, 'weapons'))
  await expect(page.getByRole('heading', { name: 'Weapons', level: 1 })).toBeVisible()
  await page.getByLabel('Name', { exact: true }).first().fill('Renamed sword')
  await saved(page)
  expect(readFileSync(path, 'utf8')).toBe('{ "music": ')
})

test('removing all sound asks with the list of what is taken out, deletes audio.json, keeps the files, and one Undo writes it all back', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Remove Sound')
  await openSound(page, id)
  const before = { audio: readFileSync(diskPath(id, 'data/audio.json'), 'utf8'), weapons: readDisk(id, 'data/weapons.json'), map: readDisk(id, 'maps/ford.json'), campaign: readDisk(id, 'data/campaign.json') }
  await page.getByRole('button', { name: 'Remove all sound…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Delete all the sound in this game?' })
  await expect(dlg).toContainText('Weapon')
  await expect(dlg).toContainText('Plays the sound “hit”')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await expect(page.getByTestId('no-sound')).toBeVisible()
  await saved(page)
  expect(existsSync(diskPath(id, 'data/audio.json'))).toBe(false)
  expect(existsSync(diskPath(id, 'audio/hit.wav'))).toBe(true)
  expect('sounds' in readDisk(id, 'data/weapons.json').weapons[0]).toBe(false)
  expect(readDisk(id, 'maps/ford.json').events[0].do).toEqual([])
  expect(await game.problems(id)).toEqual([])
  await undo(page)
  await saved(page)
  expect(JSON.parse(readFileSync(diskPath(id, 'data/audio.json'), 'utf8'))).toEqual(JSON.parse(before.audio))
  expect({ weapons: readDisk(id, 'data/weapons.json'), map: readDisk(id, 'maps/ford.json'), campaign: readDisk(id, 'data/campaign.json') }).toEqual({ weapons: before.weapons, map: before.map, campaign: before.campaign })
})

test('the standard weapon, unit and item sounds: "Same as the game" by default, "Silent" written as an empty name, units fold away', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Overrides Sound')
  await page.goto(editorUrl(id, 'units'))
  const fold = page.getByText('Sounds', { exact: true }).first()
  await expect(page.getByTestId('sounds-units')).toBeHidden() // collapsed by default
  await fold.click()
  const units = page.getByTestId('sounds-units')
  await expect(units).toBeVisible()
  for (const label of ['Walking', 'Its attack hits', 'Its attack misses', 'Its critical hit', 'Being defeated', 'Being picked', 'Being healed']) {
    await expect(units.getByLabel(`Sounds: ${label}`)).toHaveValue('__none__')
  }
  await units.getByLabel('Sounds: Walking').selectOption('step')
  await units.getByLabel('Sounds: Being defeated').selectOption({ label: 'Silent' })
  await saved(page)
  expect(readDisk(id, 'data/units.json').units[0].sounds).toEqual({ step: 'step', defeated: '' })
  await units.getByLabel('Sounds: Walking').selectOption({ label: 'Same as the game' })
  await saved(page)
  expect(readDisk(id, 'data/units.json').units[0].sounds).toEqual({ defeated: '' })
  // an item
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, items: [{ id: 'potion', name: 'Potion', kind: 'usable', heal: 5 }] } })
  await page.goto(editorUrl(id, 'items', 'potion'))
  await page.getByTestId('sounds-items').getByLabel('Sounds: When it is used').selectOption('hit')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0].sounds).toEqual({ use: 'hit' })
  expect(await game.problems(id)).toEqual([])
  if (existsSync(playerBin())) {
    const run = runPlayer(id)
    expect(playerClean(run), run.output).toEqual([])
  }
})

test('a staff has the same Sounds card as other items, and the key it writes is one the loader accepts', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Staff Sound')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, items: [{ id: 'mend', name: 'Mend', kind: 'staff', group: 'staff', range: 2, target: 'ally', effects: [{ heal: 10 }] }] } })
  await page.goto(editorUrl(id, 'items', 'mend'))
  await page.getByTestId('sounds-items').getByLabel('Sounds: When it is used').selectOption('hit')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0].sounds).toEqual({ use: 'hit' })
  expect(await game.problems(id)).toEqual([]) // the core loader no longer refuses "sounds" on a staff
  await page.getByTestId('sounds-items').getByLabel('Sounds: When it is used').selectOption({ label: 'Silent' })
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0].sounds).toEqual({ use: '' })
  expect(await game.problems(id)).toEqual([])
  // The staff's own heal sound (docs/SOUND.md "A staff's own heal sound"): the game's "+N" cue replaced, or silenced with "Silent".
  await page.getByTestId('sounds-items').getByLabel('Sounds: The “+N” when it heals').selectOption({ label: 'Silent' })
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0].sounds).toEqual({ use: '', heal: '' })
  await page.getByTestId('sounds-items').getByLabel('Sounds: The “+N” when it heals').selectOption('hit')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0].sounds).toEqual({ use: '', heal: 'hit' })
  expect(await game.problems(id)).toEqual([])
})

test('a game with audio saves and reloads without a byte changing', async ({ page, request, game }) => {
  const id = await soundGame(request, game, 'Same Bytes')
  const files = ['data/audio.json', 'data/weapons.json', 'data/units.json', 'maps/ford.json', 'data/campaign.json']
  // the helper writes its own layout on the first save: take that as the baseline
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, audio: doc.audio, weapons: doc.weapons, units: doc.units, maps: doc.maps, campaign: doc.campaign } })
  const before = files.map((f) => readFileSync(diskPath(id, f), 'utf8'))
  await openSound(page, id)
  await page.getByTestId('track-battle').getByLabel('battle: Volume').fill('70')
  await page.getByTestId('track-battle').getByLabel('battle: Volume').fill('100')
  await saved(page)
  expect(files.map((f) => readFileSync(diskPath(id, f), 'utf8'))).toEqual(before)
})
