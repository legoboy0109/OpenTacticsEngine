// E2 of docs/SRPG.md: the installer of the starter set, the Skills cards (unit type, character, accessory, weapon), the story
// forms (teach, forget, "has skill") and the 360 px phone. The proof: the exact JSON written, and the game loads.
import { spawnSync } from 'node:child_process'
import path from 'node:path'
import { newCharacter, switchOn } from './characterskit'
import { editorUrl, expect, expectNoHScroll, gamesDir, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'
import { emptySkills, newSkill, openSkills, rule, shot } from './skillskit'
import { addCommand, createItem, editor, eventsTab, form, newEvent } from './storykit'

test('@smoke the starter set: pick, point at stats, add, skip what exists; every skill loads and the game plays', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Starter')
  await emptySkills(page, id)
  await openSkills(page, id)
  await page.getByTestId('starter-open').click()
  const dlg = page.getByRole('dialog', { name: 'Add starter skills' })
  await expect(dlg.getByTestId('starter-list').getByRole('checkbox')).toHaveCount(30)
  await expect(dlg.getByLabel('Is attack?')).toHaveValue('str')
  await expect(dlg.getByLabel('Is skill?')).toHaveValue('skl')
  await shot(page, 'skills-starter-dialog')
  await dlg.getByRole('checkbox', { name: /^Tough/ }).uncheck()
  await dlg.getByLabel('Is skill?').selectOption('')
  await expect(dlg.getByRole('button', { name: 'Add 26 skills' })).toBeVisible()
  await dlg.getByRole('button', { name: 'Add 26 skills' }).click()
  await expect(page.getByTestId('starter-result')).toContainText('Added 26 skills.')
  await expect(page.getByTestId('starter-result')).toContainText('Gifted, Heavy Blow, Draining Blow')
  await page.getByRole('button', { name: 'Done' }).click()
  await saved(page)
  const skills = readDisk(id, 'data/skills.json').skills
  expect(skills).toHaveLength(26)
  expect(skills.find((s: any) => s.id === 'gifted')).toBeUndefined() // it needs the skill stat
  expect(skills.find((s: any) => s.id === 'hold_the_line').blocks[0]).toEqual({ when: 'combat', if: [{ initiating: false }], do: [{ damageTaken: -3 }] })
  expect(skills.find((s: any) => s.id === 'light_step').blocks[0].do[0]).toEqual({ stats: { mov: 1 } })
  // 26 skills nobody has yet: one line, not twenty-six.
  await expect(page.getByTestId('skill-checks')).toContainText('26 skills are not given to anyone yet')
  expect(await game.problems(id)).toEqual([])

  // Again: what exists is skipped and listed; the four that needed the skill stat or Tough are added now it is chosen.
  await page.getByTestId('starter-open').click()
  await page.getByTestId('starter-open').isVisible()
  await page.getByRole('dialog').getByRole('button', { name: /^Add \d+ skills?$/ }).click()
  await expect(page.getByTestId('starter-result')).toContainText('Added 4 skills.')
  await page.getByRole('button', { name: 'Done' }).click()
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills).toHaveLength(30)
  expect(await game.problems(id)).toEqual([])
})

test('every kind of rule the editor can write loads in the game: a skill of each "when", the whole set, and the sim plays', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Every kind')
  await emptySkills(page, id)
  await openSkills(page, id)
  await page.getByTestId('starter-open').click()
  await page.getByRole('button', { name: /^Add 30 skills$/ }).click()
  await page.getByRole('button', { name: 'Done' }).click()
  await saved(page)
  // Give them out so the unit types carry real skills into the battle.
  const all = (await (await page.request.get(`/api/projects/${id}`)).json())
  all.units.forEach((u: any) => { u.skills = all.skills.slice(0, 12).map((s: any) => s.id) })
  all.units[0].learns = [{ level: 2, skill: 'caring_presence' }]
  expect((await page.request.put(`/api/projects/${id}`, { data: { baseRevision: all.revision, units: all.units } })).ok()).toBeTruthy()
  expect(await game.problems(id)).toEqual([])
  const sim = path.resolve(import.meta.dirname, '../../../../build/ote-sim')
  const r = spawnSync(sim, [path.join(gamesDir(), id), 'ford', '--runs', '1'], { encoding: 'utf8', timeout: 60_000 })
  expect(r.status, r.stderr).toBe(0)
})

test('@smoke the Skills cards: unit type (always and learned), character, accessory and weapon', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Cards')
  await emptySkills(page, id)
  await openSkills(page, id)
  await newSkill(page, 'Sharp Eye')
  await newSkill(page, 'Calm')

  await page.goto(editorUrl(id, 'units', 'lord'))
  const unit = page.getByTestId('unit-skills')
  await expect(unit).toContainText('No skills.')
  await unit.getByLabel('Skills: Add a skill…').selectOption('sharp_eye')
  await unit.getByRole('button', { name: 'Add a learned skill' }).click()
  await unit.getByLabel('At level').fill('5')
  await unit.getByRole('group', { name: 'Learned skill 1' }).getByLabel('Skill', { exact: true }).selectOption('calm')
  await saved(page)
  const lord = () => readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord')
  expect(lord().skills).toEqual(['sharp_eye'])
  expect(lord().learns).toEqual([{ level: 5, skill: 'calm' }])
  await shot(page, 'unit-skills-card')
  await unit.getByRole('button', { name: 'Remove learned skill 1' }).click()
  await unit.getByRole('button', { name: 'Remove Sharp Eye' }).click()
  await saved(page)
  expect(lord()).not.toHaveProperty('skills')
  expect(lord()).not.toHaveProperty('learns')

  await switchOn(page, id)
  await newCharacter(page, 'Aldric')
  const ch = page.getByTestId('character-skills')
  await ch.getByLabel('Skills: Add a skill…').selectOption('calm')
  await ch.getByRole('button', { name: 'Add a learned skill' }).click()
  await ch.getByLabel('At level').fill('3')
  await saved(page)
  const c = readDisk(id, 'data/characters.json').characters[0]
  expect(c.skills).toEqual(['calm'])
  expect(c.learns).toEqual([{ level: 3, skill: 'sharp_eye' }])
  await shot(page, 'character-skills-card')

  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Lucky Ring')
  await expect(page.getByTestId('item-skills')).toHaveCount(0) // not an accessory yet
  await page.getByLabel('Kind').selectOption('accessory')
  // An accessory with no bonus yet says so; a skill is enough (the wording of K1).
  await expect(page.getByTestId('item-checks')).toContainText('a bonus to at least one stat, or at least one skill')
  await page.getByTestId('item-skills').getByLabel('Skills: Add a skill…').selectOption('sharp_eye')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items.find((i: any) => i.id === 'lucky_ring')).toEqual({ id: 'lucky_ring', name: 'Lucky Ring', kind: 'accessory', skills: ['sharp_eye'] })
  await expect(page.getByTestId('item-checks')).toHaveCount(0)
  await shot(page, 'item-skills-card')
  await page.getByLabel('Kind').selectOption('usable') // skills belong to accessories only
  await saved(page)
  expect(readDisk(id, 'data/items.json').items.find((i: any) => i.id === 'lucky_ring')).not.toHaveProperty('skills')
  await page.getByRole('button', { name: 'Undo' }).click()

  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await page.getByTestId('weapon-skills').getByLabel('Skills: Add a skill…').selectOption('calm')
  await saved(page)
  expect(readDisk(id, 'data/weapons.json').weapons.find((w: any) => w.id === 'iron_sword').skills).toEqual(['calm'])
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the story: teach a skill, forget a skill and the "has a skill" condition', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Story skills')
  await emptySkills(page, id)
  await openSkills(page, id)
  await newSkill(page, 'Calm')
  await saved(page)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.ford.units[0].id = 'eirik'
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })).ok()).toBeTruthy()

  await openMap(page, id, 'ford')
  await eventsTab(page).click()
  await newEvent(page, 'lesson')
  await addCommand(page, 'Then', 'Teach a skill')
  await form(page).getByLabel('Skill to teach').selectOption('calm')
  await form(page).getByLabel('Which units learn it', { exact: true }).selectOption('unit')
  await form(page).getByLabel('Which units learn it: unit').selectOption('eirik')
  await addCommand(page, 'Then', 'Make units forget a skill')
  await editor(page).getByLabel('Skill to forget').selectOption('calm')
  await editor(page).getByLabel('Only if: Add a condition…').selectOption({ label: 'A unit has a skill' })
  await editor(page).getByRole('group', { name: 'Only if: Condition 1' }).getByLabel('Skill', { exact: true }).selectOption('calm')
  await saved(page)
  const ev = readDisk(id, 'maps/ford.json').events[0]
  expect(ev.do[0]).toEqual({ teachSkill: 'calm', to: { unit: 'eirik' } })
  expect(ev.do[1]).toMatchObject({ forgetSkill: 'calm', from: expect.any(Object) }) // the filter is always written (the loader needs it)
  expect(ev.if).toEqual([{ hasSkill: 'calm' }])
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'story-teach-skill')
})

test('@smoke on a 360 px phone: the screen, a rule, the installer and the cards fit without sideways scrolling', async ({ page, game }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Phone skills')
  await emptySkills(page, id)
  await openSkills(page, id)
  await expectNoHScroll(page)
  await shot(page, 'skills-empty-phone')
  await page.getByTestId('starter-open').click()
  await expectNoHScroll(page)
  await shot(page, 'skills-starter-phone')
  await page.getByRole('button', { name: 'Cancel' }).click()
  await newSkill(page, 'Heavy Blow')
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('combat')
  await r1.getByLabel('Add a condition…').selectOption('near')
  await r1.getByLabel('Reaches').selectOption('allies')
  await r1.getByLabel('Add an effect…').selectOption('stats')
  await expectNoHScroll(page)
  await shot(page, 'skills-rule-phone')
  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByTestId('unit-skills').getByLabel('Skills: Add a skill…').selectOption('heavy_blow')
  await page.getByRole('button', { name: 'Add a learned skill' }).click()
  await expectNoHScroll(page)
  await shot(page, 'unit-skills-phone')
})
