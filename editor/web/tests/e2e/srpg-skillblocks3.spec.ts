// The third set of skill blocks (docs/SRPG.md "Skill blocks, third set", docs/EDITOR_UI.md "Skills", "Abilities", "Items"), built through the real
// editor: the aura's receiver tests, the strike condition, recoil, cancelEffective with tags, a steal condition that reads the item (and its reason),
// numbers on a usable item, a staff's bonus and a light that shrinks. The proof that matters: the exact JSON the forms wrote and that the real loader
// accepts the game (/problems is empty). Desktop and the 360 px phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { newSkill, openSkills, part, rule } from './skillskit'

const skills = (id: string) => readDisk(id, 'data/skills.json').skills as any[]
const abilities = (id: string) => readDisk(id, 'data/abilities.json').abilities as any[]
const items = (id: string) => readDisk(id, 'data/items.json').items as any[]

/** A weapon with a kind of damage and an effectiveness tag, and a usable item, so the forms have something to pick. */
async function prepare(page: Page, id: string): Promise<{ weapon: string; unit: string }> {
  const doc = await (await page.request.get(`/api/projects/${id}`)).json()
  const unit = doc.units[0].id as string
  const weapons = doc.weapons.map((w: any, i: number) => (i === 0 ? { ...w, damageKind: 'magic', versus: { [unit]: 2 }, versusDefault: 1 } : w))
  const list = [...(doc.items ?? []), { id: 'rope', name: 'Rope', kind: 'usable', heal: 3, uses: 2 }]
  const res = await page.request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, weapons, items: list } })
  expect(res.ok(), await res.text()).toBeTruthy()
  return { weapon: weapons[0].id, unit }
}

async function newItem(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New item' }).click()
  const dlg = page.getByRole('dialog', { name: 'New item' })
  await dlg.getByLabel('Name of the item').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

async function newAbility(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New ability' }).click()
  const dlg = page.getByRole('dialog', { name: 'New ability' })
  await dlg.getByLabel('Name of the ability').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

async function buildSkills(page: Page, id: string, weapon: string, unit: string): Promise<void> {
  await openSkills(page, id)
  await newSkill(page, 'Third banner')
  // Rule 1: an aura that asks about the receiver's weapon, its kind of damage and a formula.
  const r1 = rule(page, 1)
  await r1.getByLabel('When', { exact: true }).selectOption('combat')
  await r1.getByLabel('Reaches', { exact: true }).selectOption('allies')
  await r1.getByLabel('Only units holding this weapon or group').selectOption(weapon)
  await r1.getByLabel('Only units whose weapon deals').selectOption('magic')
  await r1.getByText('Advanced', { exact: true }).click()
  await r1.getByLabel('Only units that pass this formula').fill('b.level >= 2')
  await saved(page)
  await expect(r1.getByTestId('skill-rule-summary')).toContainText('have a weapon dealing magic damage and pass the formula b.level >= 2')

  // Rule 2: the first strike deals more.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('strike')
  await r2.getByLabel('Add a condition…').selectOption('strike')
  await expect(part(r2, 'Condition 1').getByLabel('Which strike')).toHaveValue('first')
  await saved(page)
  await expect(r2.getByTestId('skill-rule-summary')).toContainText('it is the first strike of the fight')

  // Rule 3: after a follow-up strike the wielder takes half the damage dealt (recoil by formula), turned on the wielder.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r3 = rule(page, 3)
  await r3.getByLabel('When', { exact: true }).selectOption('strike')
  await r3.getByLabel('Add a condition…').selectOption('strike')
  await part(r3, 'Condition 1').getByLabel('Which strike').selectOption('followUp')
  await part(r3, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('recoil')
  await part(r3, 'Effect 1').getByLabel('How it is given').selectOption('formula')
  await part(r3, 'Effect 1').getByLabel('Formula', { exact: true }).fill('dealt / 2')
  await part(r3, 'Effect 1').getByRole('checkbox', { name: 'Turn the strike on the wielder' }).check()
  await saved(page)
  await expect(r3.getByTestId('skill-rule-summary')).toContainText('turns on its wielder')

  // Rule 4: not hurt extra by the effectiveness tag.
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r4 = rule(page, 4)
  await r4.getByLabel('When', { exact: true }).selectOption('always')
  await part(r4, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('cancelEffective')
  await part(r4, 'Effect 1').getByLabel('Which effectiveness').selectOption('some')
  await part(r4, 'Effect 1').getByRole('checkbox', { name: unit, exact: true }).check()
  await saved(page)

  // Rule 5: after a staff, heal half of what it healed (the formula help names healed).
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r5 = rule(page, 5)
  await r5.getByLabel('When', { exact: true }).selectOption('staffUse')
  await part(r5, 'Effect 1').getByLabel('Heals', { exact: true }).selectOption('formula')
  await part(r5, 'Effect 1').getByLabel('Formula for the hit points restored').fill('healed / 2')
  await expect(part(r5, 'Effect 1')).toContainText('healed is the hit points the staff just restored')
  await saved(page)

  const blocks = skills(id).find((s) => s.id === 'third_banner').blocks
  expect(blocks).toEqual([
    { when: 'combat', aura: { to: 'allies', within: 2, weapon, weaponKind: 'magic', test: 'b.level >= 2' }, do: [{ stats: { str: 1 } }] },
    { when: 'strike', if: [{ strike: 'first' }], do: [{ damage: 3 }] },
    { when: 'strike', if: [{ strike: 'followUp' }], do: [{ recoil: { amount: 'dealt / 2', instead: true } }] },
    { when: 'always', do: [{ cancelEffective: [unit] }] },
    { when: 'staffUse', do: [{ heal: 'healed / 2' }] },
  ])
}

test('@smoke skills: the aura receiver tests, the strike number, recoil, cancelEffective tags and what a staff healed', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill blocks three', { srpgContent: true })
  const { weapon, unit } = await prepare(page, id)
  await buildSkills(page, id, weapon, unit)
  expect(await game.problems(id)).toEqual([])
})

test('@smoke abilities and items: a steal condition that reads the item, numbers on an item, a staff bonus, a light that shrinks', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Item blocks three', { srpgContent: true })
  await prepare(page, id)

  // A steal ability whose formula reads the item: the reason field appears with it.
  await page.goto(editorUrl(id, 'abilities'))
  await newAbility(page, 'Light Hands')
  await page.getByLabel('What it does', { exact: true }).selectOption('steal')
  await expect(page.getByLabel('Why an item cannot be taken')).toHaveCount(0)
  await page.getByLabel('Only if (formula)').fill('it.weight < a.str')
  await page.getByLabel('Why an item cannot be taken').fill('too heavy')
  await saved(page)
  await expect(page.getByTestId('ability-summary')).toContainText('otherwise “too heavy”')
  expect(abilities(id).find((a) => a.id === 'light_hands')).toEqual({ id: 'light_hands', name: 'Light Hands', target: 'enemy', effect: { steal: { if: 'it.weight < a.str', reason: 'too heavy' } } })

  // The game steals now, so a usable item can say how heavy it is.
  await page.goto(editorUrl(id, 'items', 'rope'))
  await page.getByLabel('Add a number (for example weight)').fill('weight')
  await page.getByLabel('Add a number (for example weight)').press('Enter')
  await page.getByLabel('Weight Number').fill('3')
  await saved(page)
  expect(items(id).find((i) => i.id === 'rope').props).toEqual({ weight: 3 })

  // A staff gives stats while carried, and a light staff can shrink.
  await page.goto(editorUrl(id, 'items'))
  await newItem(page, 'Torch')
  await page.getByLabel('Kind', { exact: true }).selectOption('staff')
  await expect(page.getByTestId('staff-use')).toBeVisible()
  const stat = (await (await page.request.get(`/api/projects/${id}`)).json()).game.stats[1] as { id: string; name: string }
  await page.getByLabel(`${stat.name} Bonus`).fill('2')
  await page.getByRole('checkbox', { name: 'It lights up an area' }).check()
  await page.getByLabel('Radius of the light (squares)').fill('4')
  await page.getByLabel('Shrinks by (squares each turn)').fill('2')
  await saved(page)
  const torch = items(id).find((i) => i.id === 'torch')
  expect(torch.bonus).toEqual({ [stat.id]: 2 })
  expect(torch.effects).toEqual([{ light: { radius: 4, turns: 1, shrink: 2 } }])
  // A shrink as big as the radius cannot be written: the field keeps what it had.
  await page.getByLabel('Shrinks by (squares each turn)').fill('9')
  await saved(page)
  expect(items(id).find((i) => i.id === 'torch').effects[0].light.shrink).toBeLessThan(4)
  await page.getByLabel('Shrinks by (squares each turn)').fill('')
  await saved(page)
  expect('shrink' in items(id).find((i) => i.id === 'torch').effects[0].light).toBe(false)
  // (This game has no fogged map, so the light staff is worth a look but loads; the problems list is empty.)
  expect(await game.problems(id)).toEqual([])
})

test('the third set fits a 360 px phone', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Skill blocks three phone', { srpgContent: true })
  const { weapon } = await prepare(page, id)
  await page.setViewportSize({ width: 360, height: 800 })
  await openSkills(page, id)
  await newSkill(page, 'Phone banner')
  const r = rule(page, 1)
  await r.getByLabel('When', { exact: true }).selectOption('combat')
  await r.getByLabel('Reaches', { exact: true }).selectOption('allies')
  await r.getByLabel('Only units holding this weapon or group').selectOption(weapon)
  await r.getByText('Advanced', { exact: true }).click()
  await expectNoHScroll(page)
  await page.getByRole('button', { name: 'Add a rule' }).click()
  const r2 = rule(page, 2)
  await r2.getByLabel('When', { exact: true }).selectOption('strike')
  await r2.getByLabel('Add a condition…').selectOption('strike')
  await part(r2, 'Condition 1').getByLabel('Which strike').selectOption('compare')
  await part(r2, 'Effect 1').getByLabel('Effect 1', { exact: true }).selectOption('recoil')
  await expectNoHScroll(page)
  await saved(page)
  await page.goto(editorUrl(id, 'abilities'))
  await newAbility(page, 'Phone steal')
  await page.getByLabel('What it does', { exact: true }).selectOption('steal')
  await page.getByLabel('Only if (formula)').fill('it.weight < 3')
  await expect(page.getByLabel('Why an item cannot be taken')).toBeVisible()
  await expectNoHScroll(page)
  await saved(page)
  await page.goto(editorUrl(id, 'items', 'rope'))
  await expectNoHScroll(page)
  expect(await game.problems(id)).toEqual([])
})
