// E2 of docs/SRPG.md: the Skills screen and its rule builder through the real editor. The proof that matters: the exact JSON the
// editor wrote, and that the game loader accepts it (/problems is empty).
import { editorUrl, expect, readDisk, saved, test } from './fixtures'
import { emptySkills, newSkill, openSkills, part, rule, shot } from './skillskit'

test('@smoke an empty Skills screen, then a skill with three rules: the file is exactly that, the sentences read well, the game loads', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skills')
  await emptySkills(page, id)
  await openSkills(page, id)
  await expect(page.getByText('No skills yet').first()).toBeVisible()
  await shot(page, 'skills-empty')

  await newSkill(page, 'Heavy Blow')
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills).toEqual([{ id: 'heavy_blow', name: 'Heavy Blow', blocks: [{ when: 'always', do: [{ stats: { str: 1 } }] }] }])
  await expect(page.getByTestId('skill-summary')).toHaveText('Always: Strength +1.')

  // Rule 1: in combat, only if it started the fight, damage +3.
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('combat')
  await r1.getByLabel('Add a condition…').selectOption('initiating')
  await part(r1, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('damage')
  await expect(r1.getByTestId('skill-rule-summary')).toHaveText('In combat, if it started the fight: damage +3.')

  // Rule 2: when it strikes, a chance from the Skill stat times 2, double damage.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('strike')
  await r2.getByLabel('Chance', { exact: true }).selectOption('stat')
  await r2.getByLabel('Stat', { exact: true }).selectOption('skl')
  await r2.getByLabel('Times').fill('2')
  await part(r2, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('damagePercent')
  await part(r2, 'Effect 1').getByLabel('Amount').fill('200')
  await expect(r2.getByTestId('skill-rule-summary')).toHaveText('When it strikes, with a chance of its Skill times 2 percent: damage dealt 200%.')

  // Rule 3: a timed rule with a chance: heals allies at the start of its turn.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r3 = rule(page, 3)
  await r3.getByLabel('When', { exact: true }).selectOption('turnStart')
  await r3.getByLabel('Chance', { exact: true }).selectOption('percent')
  await r3.getByLabel('Percent', { exact: true }).fill('50')
  const fx = part(r3, 'Effect 1')
  await fx.getByLabel('Heals', { exact: true }).selectOption('percent')
  await fx.getByLabel('Amount').fill('20')
  await fx.getByLabel('Who is healed').selectOption('allies')
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills[0].blocks).toEqual([
    { when: 'combat', if: [{ initiating: true }], do: [{ damage: 3 }] },
    { when: 'strike', chance: 'a.skl * 2', do: [{ damagePercent: 200 }] },
    { when: 'turnStart', chance: 50, do: [{ heal: { percent: 20 }, to: 'allies', within: 1 }] },
  ])
  await expect(page.getByTestId('skill-summary')).toHaveText(
    'In combat, if it started the fight: damage +3. When it strikes, with a chance of its Skill times 2 percent: damage dealt 200%. ' +
    'At the start of its turn, with a 50% chance: heals allies within 1 square by 20% of their HP.')
  await shot(page, 'skills-three-rules')

  // An aura: allies within 2 squares, and the "foe" choices.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r4 = rule(page, 4)
  await r4.getByLabel('Reaches').selectOption('allies')
  await expect(r4.getByTestId('skill-rule-summary')).toHaveText('In combat: allies within 2 squares get damage +3.')
  await part(r4, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('hit')
  await r4.getByLabel('Add an effect…').selectOption('avoid')
  await shot(page, 'skills-aura-rule')

  // The unit type gets the skill (always), so no "nobody has it" check is left; the game loads and plays.
  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByTestId('unit-skills').getByLabel('Skills: Add a skill…').selectOption('heavy_blow')
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord').skills).toEqual(['heavy_blow'])
  expect(await game.problems(id)).toEqual([])
})

test('the pickers offer only what the "when" allows; changing it repairs the rule', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill pickers')
  await emptySkills(page, id)
  await openSkills(page, id)
  await newSkill(page, 'Picky')
  const r1 = rule(page, 1)
  const effects = async () => r1.getByLabel('Add an effect…').locator('option').allTextContents()
  expect(await effects()).toEqual(expect.arrayContaining(['Squares cost at most', 'Move through enemy units', 'Experience gained']))
  expect(await effects()).not.toContain('Strikes once more')
  await r1.getByLabel('When', { exact: true }).selectOption('strike')
  expect(await effects()).toEqual(expect.arrayContaining(['Strikes once more', 'Heals itself from the damage']))
  expect(await effects()).not.toContain('Squares cost at most')
  // A strike rule gets a chance picker, a combat rule a "Reaches" picker, an "always" rule neither.
  await expect(r1.getByTestId('skill-chance')).toBeVisible()
  await r1.getByLabel('When', { exact: true }).selectOption('always')
  await expect(r1.getByTestId('skill-chance')).toHaveCount(0)
  await expect(r1.getByTestId('skill-reaches')).toHaveCount(0)
  await r1.getByLabel('When', { exact: true }).selectOption('combat')
  await expect(r1.getByTestId('skill-reaches')).toBeVisible()
  // "Always" has no foe conditions; a rule that gives growths cannot have any condition.
  await r1.getByLabel('When', { exact: true }).selectOption('always')
  const conds = await r1.getByLabel('Add a condition…').locator('option').allTextContents()
  expect(conds).not.toContain('Who started the fight')
  await r1.getByLabel('Add an effect…').selectOption('growths')
  await expect(r1.getByTestId('skill-no-conditions')).toContainText('growths cannot have conditions')
  expect(await game.problems(id)).toEqual([])
})

test('duplicate, reorder, undo, and delete (it names who has it and takes it from them in one undo step)', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill delete')
  await emptySkills(page, id)
  await openSkills(page, id)
  await newSkill(page, 'Keen Eye')
  await page.getByRole('button', { name: 'Duplicate' }).click()
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills.map((s: any) => s.id)).toEqual(['keen_eye', 'keen_eye_copy'])
  await page.getByRole('button', { name: 'Move skill up' }).click()
  await saved(page)
  expect(readDisk(id, 'data/skills.json').skills.map((s: any) => s.id)).toEqual(['keen_eye_copy', 'keen_eye'])

  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByTestId('unit-skills').getByLabel('Skills: Add a skill…').selectOption('keen_eye')
  await saved(page)
  await openSkills(page, id, 'keen_eye')
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog')
  await expect(dlg).toContainText('Unit “Lord” has it')
  await dlg.getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord')).not.toHaveProperty('skills')
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'lord').skills).toEqual(['keen_eye'])
  expect(readDisk(id, 'data/skills.json').skills).toHaveLength(2)
})

test('a rule the file has in a refused shape is shown with a plain message, and the loader names it too (Problems)', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill problem')
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  const skills = [{ id: 'bad', name: 'Bad', blocks: [{ when: 'always', if: [{ hp: { op: '<=', percent: 50 } }], do: [{ growths: { str: 5 } }] }] }]
  expect((await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, skills } })).ok()).toBeTruthy()
  await openSkills(page, id, 'bad')
  await expect(rule(page, 1).getByTestId('skill-rule-issues')).toContainText('Growths cannot have a condition')
  await expect(page.getByTestId('skill-checks')).toContainText('Bad, rule 1: Growths cannot have a condition.')
  await expect(page.getByTestId('skill-checks')).toContainText('nobody has this skill yet')
  const problems = await game.problems(id)
  expect(problems.map((p) => p.message).join(' ')).toContain('"growths" cannot have a condition')
  await shot(page, 'skills-problem')
})
