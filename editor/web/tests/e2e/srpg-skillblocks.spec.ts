// The general skill blocks (docs/SRPG.md "More skill blocks", docs/EDITOR_UI.md "Skills"), built through the real editor: a weapon's kind of damage, an aura
// limited to a kind of unit, turn tests, formula amounts, follow-up control, the counterattack test, "move again but not after attacking" and cure. The proof
// that matters: the exact JSON the forms wrote and that the real loader accepts the game (/problems is empty). Desktop and the 360 px phone layout.
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { newSkill, openSkills, part, rule } from './skillskit'

const skills = (id: string) => readDisk(id, 'data/skills.json').skills as any[]
const weapon = (id: string, wid: string) => (readDisk(id, 'data/weapons.json').weapons as any[]).find((w) => w.id === wid)

test('@smoke the general skill blocks: a damage kind on a weapon, an aura for one kind of unit, turn tests, formula amounts, follow-up control, move again, cure', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill blocks', { srpgContent: true })

  // A weapon's kind of damage: a free word.
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await page.getByLabel('Kind of damage').fill('magic')
  await saved(page)
  expect(weapon(id, 'iron_sword').damageKind).toBe('magic')

  await openSkills(page, id)
  await newSkill(page, 'Rally Scouts')

  // Rule 1: an aura for cavalry only, in combat, on even turns; its hit bonus is a formula that reads the unit that receives it.
  const r1 = rule(page, 1)  // a new skill starts with one rule
  await r1.getByLabel('When', { exact: true }).selectOption('combat')
  await r1.getByLabel('Reaches', { exact: true }).selectOption('allies')
  await r1.getByLabel('Only units of this type or tag').selectOption('cavalier')
  await part(r1, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('hit')
  await part(r1, 'Effect 1').getByLabel('How it is given').selectOption('formula')
  await part(r1, 'Effect 1').getByLabel('Formula', { exact: true }).fill('turn * 2')
  await r1.getByLabel('Add a condition…').selectOption('turn')
  await part(r1, 'Condition 1').getByLabel('Which turns').selectOption('even')
  await saved(page)
  await expect(r1.getByTestId('skill-rule-summary')).toContainText('allies that are a Cavalier within 2 squares get hit from the formula turn * 2')

  // Rule 2: when it strikes and the foe cannot answer, +3 damage.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('strike')
  await r2.getByLabel('Add a condition…').selectOption('counterattack')
  await expect(part(r2, 'Condition 1').getByLabel('The foe')).toHaveValue('no')
  await saved(page)

  // Rule 3: in combat the foe never makes a follow-up strike, but only against a magic weapon (a test on the foe's weapon).
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r3 = rule(page, 3)
  await r3.getByLabel('When', { exact: true }).selectOption('combat')
  await part(r3, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('followUp')
  await part(r3, 'Effect 1').getByLabel('Follow-up strike').selectOption('forbid')
  await part(r3, 'Effect 1').getByLabel('Who gets it').selectOption('foe')
  await r3.getByLabel('Add a condition…').selectOption('weaponKind')
  await part(r3, 'Condition 1').getByLabel('Who it is about').selectOption('foe')
  await expect(part(r3, 'Condition 1').getByLabel('Kind of damage')).toHaveValue('magic')

  // Rule 4: move again, but not after attacking. Rule 5: cured of everything at the start of its turn.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r4 = rule(page, 4)
  await r4.getByLabel('When', { exact: true }).selectOption('always')
  await part(r4, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('moveAgain')
  await part(r4, 'Effect 1').getByRole('checkbox', { name: 'Not after attacking' }).check()
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r5 = rule(page, 5)
  await r5.getByLabel('When', { exact: true }).selectOption('turnStart')
  await part(r5, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('cure')
  await part(r5, 'Effect 1').getByLabel('Which conditions').selectOption('all')
  await saved(page)

  const s = skills(id).find((x) => x.id === 'rally_scouts')
  expect(s.blocks).toEqual([
    { when: 'combat', if: [{ turn: { even: true } }], aura: { to: 'allies', within: 2, match: 'cavalier' }, do: [{ hit: 'turn * 2' }] },
    { when: 'strike', if: [{ counterattack: false }], do: [{ damage: 3 }] },
    { when: 'combat', if: [{ weaponKind: 'magic', who: 'foe' }], do: [{ followUp: 'forbid', to: 'foe' }] },
    { when: 'always', do: [{ moveAgain: { notAfterAttack: true } }] },
    { when: 'turnStart', do: [{ cure: 'all' }] },
  ])
  // The real loader accepts the game.
  expect(await game.problems(id)).toEqual([])
})

test('the general skill blocks fit a 360 px phone', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill blocks phone', { srpgContent: true })
  await page.setViewportSize({ width: 360, height: 800 })
  await openSkills(page, id)
  await newSkill(page, 'Phone blocks')
  const r = rule(page, 1)
  await r.getByLabel('When', { exact: true }).selectOption('combat')
  await r.getByLabel('Reaches', { exact: true }).selectOption('allies')
  await r.getByLabel('Add a condition…').selectOption('turn')
  await part(r, 'Condition 1').getByLabel('Which turns').selectOption('every')
  await part(r, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('hit')
  await part(r, 'Effect 1').getByLabel('How it is given').selectOption('formula')
  await expectNoHScroll(page)
  await saved(page)
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await expect(page.getByLabel('Kind of damage')).toBeVisible()
  await expectNoHScroll(page)
})
