// Character-locked weapons (docs/SRPG.md "Character-locked weapons", editor step): the "Only these characters can wield it" card on the
// Weapons screen, the follow-through when a character is deleted, the checks, and the 360 px phone layout. The proof that matters:
// the exact JSON the editor wrote, that the loader accepts it (/problems is empty) and that the real engine loads it (Play).
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, requirePlayer, saved, test } from './fixtures'
import { rankedGame, shot, shotEl } from './rankskit'

const weapon = (id: string, wid: string) => readDisk(id, 'data/weapons.json').weapons.find((w: any) => w.id === wid)
const card = (page: Page) => page.getByTestId('weapon-lock')
const playButton = (page: Page) => page.getByRole('button', { name: 'Play', exact: true })
const playPanel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })

test('@smoke lock a weapon to two characters, reload, delete one (list cleaned, one Undo), press Play: the real engine loads it', async ({ page, game, request }) => {
  requirePlayer()
  const id = await game.create('strategy-rpg', 'Locks Play', { srpgContent: true })
  await rankedGame(request, id) // carried weapons, groups, a class that wields the sword group

  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await expect(card(page)).toBeVisible()
  await expect(page.getByTestId('weapon-lock-summary')).toContainText('No lock')
  await card(page).getByRole('checkbox', { name: 'Kell' }).check()
  await card(page).getByRole('checkbox', { name: 'Eirik' }).check()
  await saved(page)
  expect(weapon(id, 'iron_sword').wieldedBy).toEqual(['eirik', 'kell']) // in the order of the characters
  expect(await game.problems(id)).toEqual([])
  await shot(page, 'weapon-lock')
  await shotEl(page.locator('#weapon-lock'), 'weapon-lock-card')

  // Still there after a reload; the character's own screen lists the weapon.
  await page.reload()
  await expect(card(page).getByRole('checkbox', { name: 'Kell' })).toBeChecked()
  await expect(page.getByTestId('weapon-lock-summary')).toContainText('Locked to 2 characters')
  await page.goto(editorUrl(id, 'characters', 'kell'))
  await expect(page.getByTestId('character-locked-weapons')).toContainText('Iron Sword')
  await shotEl(page.locator('#character-locked-weapons'), 'character-locked-weapons')

  // Deleting Kell: the question names the weapon, the list loses Kell, Undo brings him back.
  await page.getByRole('button', { name: 'Delete' }).click()
  await expect(page.getByRole('dialog')).toContainText('Weapon “Iron Sword” is locked to named characters')
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(weapon(id, 'iron_sword').wieldedBy).toEqual(['eirik'])
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(weapon(id, 'iron_sword').wieldedBy).toEqual(['eirik', 'kell'])
  expect((readDisk(id, 'data/characters.json').characters as any[]).some((c) => c.id === 'kell')).toBe(true)

  // Unticking the last one removes the key altogether.
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await card(page).getByRole('checkbox', { name: 'Kell' }).uncheck()
  await card(page).getByRole('checkbox', { name: 'Eirik' }).uncheck()
  await saved(page)
  expect(weapon(id, 'iron_sword')).not.toHaveProperty('wieldedBy')
  await card(page).getByRole('checkbox', { name: 'Eirik' }).check()
  await saved(page)

  // The real engine loads it.
  expect(await game.problems(id)).toEqual([])
  await playButton(page).click()
  await expect(playPanel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})

test('a lock nobody can use is worth a look; the card is hidden without carried weapons and the key is left alone', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Locks Checks', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await card(page).getByRole('checkbox', { name: 'Vex' }).check() // an archer: cannot use a sword
  await expect(page.getByTestId('weapon-lock-checks')).toContainText('Nobody on the list can ever wield Iron Sword')
  await saved(page)
  expect(weapon(id, 'iron_sword').wieldedBy).toEqual(['vex'])
  await card(page).getByRole('checkbox', { name: 'Eirik' }).check()
  await expect(page.getByTestId('weapon-lock-checks')).toHaveCount(0)
  await saved(page)

  // Weapons not carried: the card is gone, and the list stays in the file.
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('Weapons', { exact: true }).selectOption('fixed')
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await expect(page.getByTestId('weapon-lock')).toHaveCount(0)
  await saved(page)
  expect(weapon(id, 'iron_sword').wieldedBy).toEqual(['eirik', 'vex'])
})

test('switching characters off asks, names the locks, and removes the lists', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Locks Off', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await card(page).getByRole('checkbox', { name: 'Eirik' }).check()
  await saved(page)
  await page.goto(editorUrl(id, 'rules'))
  await page.getByTestId('srpg-switches').getByRole('switch', { name: 'This game has characters' }).click()
  await expect(page.getByTestId('off-locks')).toContainText('One weapon is locked')
  await page.getByRole('dialog').getByRole('button', { name: 'Remove' }).click()
  await saved(page)
  expect(weapon(id, 'iron_sword')).not.toHaveProperty('wieldedBy')
  expect(await game.problems(id)).toEqual([])
})

test('@smoke on a 360 px phone the lock card and the character line fit without sideways scrolling', async ({ page, game, request }) => {
  await page.setViewportSize({ width: 360, height: 800 })
  const id = await game.create('strategy-rpg', 'Locks Phone', { srpgContent: true })
  await rankedGame(request, id)
  await page.goto(editorUrl(id, 'weapons', 'iron_sword'))
  await card(page).getByRole('checkbox', { name: 'Vex' }).check() // an archer: the check shows too
  await expect(page.getByTestId('weapon-lock-checks')).toBeVisible()
  await expectNoHScroll(page)
  await shotEl(page.locator('#weapon-lock'), 'phone-weapon-lock')
  await saved(page)
  await page.goto(editorUrl(id, 'characters', 'vex'))
  await expect(page.getByTestId('character-locked-weapons')).toBeVisible()
  await expectNoHScroll(page)
  await shotEl(page.locator('#character-locked-weapons'), 'phone-character-locked-weapons')
})
