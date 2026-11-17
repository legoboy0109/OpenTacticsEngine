// The second set of skill and ability blocks (docs/SRPG.md "Skill and ability blocks, second set", docs/EDITOR_UI.md "Skills", "Abilities"), built through
// the real editor, plus the controls for the keys the first set left hand-edit only (character tags, a map unit's skills, class skills on promotion).
// The proof that matters: the exact JSON the forms wrote and that the real loader accepts the game (/problems is empty). Desktop and the 360 px phone.
import { editorUrl, expect, expectNoHScroll, readDisk, saved, section, test } from './fixtures'
import { clickCell, openMap, tool } from './mapkit'
import { newSkill, openSkills, part, rule } from './skillskit'

const skills = (id: string) => readDisk(id, 'data/skills.json').skills as any[]
const abilities = (id: string) => readDisk(id, 'data/abilities.json').abilities as any[]

async function newAbility(page: import('@playwright/test').Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New ability' }).click()
  const dlg = page.getByRole('dialog', { name: 'New ability' })
  await dlg.getByLabel('Name of the ability').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

test('@smoke skills: nullify the foe’s skills by tag, a staff-used rule, a staff range bonus, a terrain list, a skill’s tags', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill blocks two', { srpgContent: true })

  // A tag on an existing skill (the field of the skill's form).
  await openSkills(page, id, 'tough')
  await page.getByLabel('Tags').fill('bonus')
  await page.getByLabel('Tags').press('Enter')
  await saved(page)
  expect(skills(id).find((s) => s.id === 'tough').tags).toEqual(['bonus'])

  await newSkill(page, 'Silencer')
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('combat')
  await part(r1, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('nullifySkills')
  await part(r1, 'Effect 1').getByLabel('Which skills').selectOption('tag')
  await part(r1, 'Effect 1').getByLabel('Tag', { exact: true }).selectOption('bonus')
  await saved(page)
  await expect(r1.getByTestId('skill-rule-summary')).toContainText('the foe’s skills tagged “bonus” do not work in this fight')

  // Rule 2: after using a staff, heal itself.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('staffUse')
  await expect(part(r2, 'Effect 1').getByLabel('Effect 1', { exact: true })).toHaveValue('heal')
  await expect(part(r2, 'Effect 1').getByLabel('Heals whom')).toHaveCount(0)  // a staff's heal goes to the user: no choice of whom
  await saved(page)

  // Rule 3: staves reach 2 further. Rule 4: in combat, standing on forest or mountain, +damage (the terrain list).
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r3 = rule(page, 3)
  await r3.getByLabel('When', { exact: true }).selectOption('always')
  await part(r3, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('staffRange')
  await part(r3, 'Effect 1').getByLabel('Extra squares of reach').fill('2')
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r4 = rule(page, 4)
  await r4.getByLabel('When', { exact: true }).selectOption('combat')
  await r4.getByLabel('Add a condition…').selectOption('terrain')
  await part(r4, 'Condition 1').getByLabel('Match').selectOption('many')
  await part(r4, 'Condition 1').getByRole('checkbox', { name: 'Plain' }).uncheck()  // the list starts from the one kind it had
  await part(r4, 'Condition 1').getByRole('checkbox', { name: 'Forest' }).check()
  await part(r4, 'Condition 1').getByRole('checkbox', { name: 'Mountain' }).check()
  await saved(page)

  expect(skills(id).find((s) => s.id === 'silencer').blocks).toEqual([
    { when: 'combat', do: [{ nullifySkills: { tag: 'bonus' } }] },
    { when: 'staffUse', do: [{ heal: 5 }] },
    { when: 'always', do: [{ staffRange: 2 }] },
    { when: 'combat', if: [{ terrain: ['forest', 'mountain'] }], do: [{ damage: 3 }] },
  ])
  expect(await game.problems(id)).toEqual([])
})

test('@smoke abilities: steal, refresh, move past and cure; an item that cannot be stolen', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Ability blocks two', { srpgContent: true })
  await page.goto(editorUrl(id, 'abilities'))
  await expect(page.getByRole('heading', { name: 'Abilities', level: 1 })).toBeVisible()

  await newAbility(page, 'Pickpocket')
  await page.getByLabel('What it does', { exact: true }).selectOption('steal')
  await expect(page.getByLabel('Who or what it is used on')).toHaveValue('enemy')
  await page.getByLabel('Only if (formula)').fill('a.spd > b.spd')
  await page.getByLabel('Weapons can be taken').check()
  await saved(page)
  await expect(page.getByTestId('ability-summary')).toContainText('weapons too')
  expect(abilities(id).find((a) => a.id === 'pickpocket')).toEqual({ id: 'pickpocket', name: 'Pickpocket', target: 'enemy', effect: { steal: { if: 'a.spd > b.spd', weapons: true } } })
  await page.getByLabel('The weapon in hand can be taken too').check()
  await saved(page)
  expect(abilities(id).find((a) => a.id === 'pickpocket').effect).toEqual({ steal: { if: 'a.spd > b.spd', weapons: true, equipped: true } })

  await newAbility(page, 'Encore')
  await page.getByLabel('What it does', { exact: true }).selectOption('refresh')
  await page.getByLabel('Times a unit can be refreshed in a turn').fill('2')
  await newAbility(page, 'Leap Past')
  await page.getByLabel('What it does', { exact: true }).selectOption('movePast')
  await newAbility(page, 'Cleanse')
  await page.getByLabel('What it does', { exact: true }).selectOption('cure')
  await page.getByLabel('Which conditions').selectOption('all')
  await saved(page)
  expect(abilities(id).find((a) => a.id === 'encore')).toEqual({ id: 'encore', name: 'Encore', target: 'ally', effect: { refresh: { perTurn: 2 } } })
  expect(abilities(id).find((a) => a.id === 'leap_past')).toEqual({ id: 'leap_past', name: 'Leap Past', target: 'unit', effect: { movePast: true } })
  expect(abilities(id).find((a) => a.id === 'cleanse')).toEqual({ id: 'cleanse', name: 'Cleanse', target: 'self', effect: { cure: 'all' } })

  // Now that the game has a steal ability the Items screen offers the switch.
  const first = ((await (await page.request.get(`/api/projects/${id}`)).json()).items[0].id) as string  // (the file starts with a comment line)
  await page.goto(editorUrl(id, 'items', first))
  await page.getByRole('switch', { name: 'Can be stolen' }).uncheck()
  await saved(page)
  const itemsOnDisk = async () => ((await (await page.request.get(`/api/projects/${id}`)).json()).items as any[]).find((i) => i.id === first)
  expect((await itemsOnDisk()).stealable).toBe(false)
  await page.getByRole('switch', { name: 'Can be stolen' }).check()
  await saved(page)
  expect('stealable' in (await itemsOnDisk())).toBe(false)
  expect(await game.problems(id)).toEqual([])
})

test('@smoke the keys that were hand-edit only: character tags, a map unit’s skills, skills of the old class', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Hand edit keys', { srpgContent: true })

  // A character's tags: chips, like a unit type's.
  await page.goto(editorUrl(id, 'characters', 'eirik'))
  await page.getByTestId('character-tags').getByRole('textbox').fill('scout')
  await page.getByTestId('character-tags').getByRole('textbox').press('Enter')
  await saved(page)
  expect(readDisk(id, 'data/characters.json').characters.find((c: any) => c.id === 'eirik').tags).toEqual(['scout'])

  // A map unit's own skills.
  const unit = readDisk(id, 'maps/ford.json').units[0]
  await openMap(page, id, 'ford')
  await tool(page, 'Select').click()
  await clickCell(page, unit.x, unit.y)
  const own = page.getByTestId('unit-skills')
  await own.getByRole('checkbox', { name: 'Tough' }).check()
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').units[0].skills).toEqual(['tough'])
  await own.getByRole('checkbox', { name: 'Tough' }).uncheck()
  await saved(page)
  expect('skills' in readDisk(id, 'maps/ford.json').units[0]).toBe(false)
  await own.getByRole('checkbox', { name: 'Tough' }).check()
  await saved(page)

  // The promotion rule: skills of the old class go on promotion.
  await page.goto(editorUrl(id, 'rules'))
  await page.getByTestId('promotion-rules').getByLabel('Skills of the old class').selectOption('replace')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.promotion.classSkills).toBe('replace')
  expect(await game.problems(id)).toEqual([])
  await section(page, 'Skills').click()
})

test('the second set fits a 360 px phone', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill blocks two phone', { srpgContent: true })
  await page.setViewportSize({ width: 360, height: 800 })
  await openSkills(page, id)
  await newSkill(page, 'Phone silencer')
  const r = rule(page, 1)
  await r.getByLabel('When', { exact: true }).selectOption('combat')
  await part(r, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('nullifySkills')
  await r.getByLabel('Add a condition…').selectOption('terrain')
  await part(r, 'Condition 1').getByLabel('Match').selectOption('many')
  await expectNoHScroll(page)
  await saved(page)
  await page.goto(editorUrl(id, 'abilities'))
  await newAbility(page, 'Phone steal')
  await page.getByLabel('What it does', { exact: true }).selectOption('steal')
  await expectNoHScroll(page)
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})
