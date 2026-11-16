// E0 of docs/SRPG.md: the strategy-RPG switches on Rules, the sidebar entries that follow them, and the rule that a
// feature switched on is remembered (an empty file) and one switched off leaves no file. (The screens themselves are empty frames until steps E1 to E4.)
import { readFileSync } from 'node:fs'
import { diskPath, editorUrl, expect, saved, section, test, writeDisk } from './fixtures'

test('switches add and remove the screens; a switch stays on after a reload, and off removes the file', async ({ page, game }) => {
  const id = await game.create('blank', 'Skeleton')
  await page.goto(editorUrl(id, 'rules'))
  const switches = page.getByTestId('srpg-switches')
  await expect(switches).toBeVisible()
  for (const name of ['Characters', 'Skills', 'Abilities', 'Supports']) await expect(section(page, name)).toHaveCount(0)

  // Supports need characters.
  await expect(switches.getByRole('switch', { name: 'This game has supports' })).toBeDisabled()
  await switches.getByRole('switch', { name: 'This game has characters' }).check()
  await expect(section(page, 'Characters')).toBeVisible()
  await expect(switches.getByRole('switch', { name: 'This game has supports' })).toBeEnabled()
  await switches.getByRole('switch', { name: 'This game has skills' }).check()
  await expect(section(page, 'Skills')).toBeVisible()
  await saved(page)
  // The switch is remembered as an empty file (the engine treats it as no file); features left off have none.
  expect(JSON.parse(readFileSync(diskPath(id, 'data/characters.json'), 'utf8'))).toEqual({ characters: [] })
  expect(JSON.parse(readFileSync(diskPath(id, 'data/skills.json'), 'utf8'))).toEqual({ skills: [] })
  for (const f of ['abilities', 'supports']) expect(() => readFileSync(diskPath(id, `data/${f}.json`))).toThrow()
  expect(await game.problems(id)).toEqual([])
  await page.reload()
  await expect(switches.getByRole('switch', { name: 'This game has skills' })).toBeChecked()
  await expect(switches.getByRole('switch', { name: 'This game has characters' })).toBeChecked()
  await expect(section(page, 'Skills')).toBeVisible()

  // The empty frame of a screen, and the way back.
  await section(page, 'Skills').click()
  await expect(page.getByRole('heading', { name: 'Skills', level: 1 })).toBeVisible()
  await expect(page.getByText('No skills yet').first()).toBeVisible() // E2 filled the Skills screen

  // Off again (nothing inside, so no question) and the entry goes.
  await section(page, 'Rules').click()
  await switches.getByRole('switch', { name: 'This game has skills' }).uncheck()
  await expect(section(page, 'Skills')).toHaveCount(0)
  await saved(page)
  expect(() => readFileSync(diskPath(id, 'data/skills.json'))).toThrow()
  expect(await game.problems(id)).toEqual([])
  await page.reload()
  await expect(switches.getByRole('switch', { name: 'This game has skills' })).not.toBeChecked()
})

test('a game that has the files shows the screens; saving something else leaves them alone; a screen of a switched-off feature says so', async ({ page, game }) => {
  const id = await game.create('blank', 'Has files')
  const text = '{\n  "skills": [\n    { "id": "heavy_blow", "name": "Heavy Blow", "mystery": 1, "blocks": [] }\n  ]\n}\n'
  writeDisk(id, 'data/skills.json', text)
  await page.goto(editorUrl(id, 'rules'))
  // (The loader may have something to say about the file: the entry then carries a problem dot, so match the start.)
  await expect(page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name: /^Skills/ })).toBeVisible()
  await expect(section(page, 'Characters')).toHaveCount(0)

  // Remove nothing, change something unrelated: the file is byte-identical.
  await page.getByLabel('Counterattacks').uncheck()
  await saved(page)
  expect(readFileSync(diskPath(id, 'data/skills.json'), 'utf8')).toBe(text)

  // Switching off asks, because a skill would go; Cancel keeps it, Remove deletes the file.
  const sw = page.getByRole('switch', { name: 'This game has skills' })
  await expect(sw).toBeChecked()
  await sw.click()
  const dlg = page.getByRole('dialog', { name: 'Remove all skills?' })
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  await expect(sw).toBeChecked()
  await sw.click()
  await dlg.getByRole('button', { name: 'Remove' }).click()
  await saved(page)
  expect(() => readFileSync(diskPath(id, 'data/skills.json'))).toThrow()

  // The address of a switched-off screen still opens and says what to do.
  await page.goto(editorUrl(id, 'characters'))
  await expect(page.getByTestId('srpg-off')).toContainText('switched off')
})
