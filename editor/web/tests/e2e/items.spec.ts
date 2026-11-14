// Items (docs/ITEMS.md) through the real editor: one item of each kind, a shop, starting items, a drop, a placed unit's own
// items and a side's storage, then the proof that matters: the files the editor wrote load in the game loader.
import type { Page } from '@playwright/test'
import { expectNoHScroll, editorUrl, expect, readDisk, saved, test } from './fixtures'
import { clickCell, openMap, tab, tool } from './mapkit'

const shot = async (page: Page, name: string) => { if (process.env.OTE_SHOT_DIR) await page.screenshot({ path: `${process.env.OTE_SHOT_DIR}/${name}.png`, fullPage: true }) }

async function createItem(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New item' }).click()
  const dlg = page.getByRole('dialog', { name: 'New item' })
  await dlg.getByLabel('Name of the item').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

test('@smoke one item of each kind, a shop, starting items, a drop and storage: the files are exactly that and the game loads', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Items')
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('items')

  // --- Items screen, weapons still fixed: the Weapon kind explains itself and cannot be chosen.
  await page.goto(editorUrl(id, 'items'))
  await expect(page.getByRole('heading', { name: 'Items', level: 1 })).toBeVisible()
  await createItem(page, 'Potion')
  await expect(page.getByLabel('Kind')).toHaveValue('usable')
  await expect(page.getByRole('option', { name: 'Weapon' })).toBeDisabled()
  await expect(page.getByTestId('weapon-kind-locked')).toContainText('carried as items')
  await page.getByLabel('Description').fill('Restores 10 HP.')
  await page.getByLabel('Funds Price').fill('100')
  await page.getByLabel('Number of uses').fill('2')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items).toEqual([
    { id: 'potion', name: 'Potion', kind: 'usable', uses: 2, heal: 10, description: 'Restores 10 HP.', price: { funds: 100 } },
  ])

  // A usable item that heals everyone's whole health and is used on a neighbour; then raise a stat too.
  await page.getByLabel('Who it can be used on').selectOption('ally')
  await page.getByRole('checkbox', { name: 'All HP' }).check()
  await page.getByLabel('HP Raise by').fill('1')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0]).toMatchObject({ target: 'ally', heal: 'full', raise: { hp: 1 } })
  await page.getByLabel('Who it can be used on').selectOption('self')
  await page.getByRole('checkbox', { name: 'All HP' }).uncheck()
  await page.getByLabel('Amount of health restored').fill('10')
  await page.getByLabel('HP Raise by').fill('')
  await saved(page)
  expect(readDisk(id, 'data/items.json').items[0]).not.toHaveProperty('target')
  expect(readDisk(id, 'data/items.json').items[0]).not.toHaveProperty('raise')

  // --- Rules: carried weapons, a bigger inventory, shared storage. Nothing is written until it is touched.
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('Weapons', { exact: true }).selectOption('carried')
  await page.getByLabel('Items one unit can carry').fill('6')
  await page.getByLabel('Shared storage').selectOption('anywhere')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.items).toEqual({ weapons: 'carried', inventorySize: 6, storage: 'anywhere' })

  // --- Weapons: a group.
  await page.goto(editorUrl(id, 'weapons', 'rifle'))
  await page.getByLabel('Group').fill('gun')
  await saved(page)
  expect(readDisk(id, 'data/weapons.json').weapons.find((w: any) => w.id === 'rifle').group).toBe('gun')

  // --- Items: a weapon item and an accessory. Changing kind drops what belonged to the old kind.
  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Rifle Item')
  await page.getByLabel('Kind').selectOption('weapon')
  await page.getByLabel('Which weapon').selectOption('rifle')
  await page.getByRole('switch', { name: 'Unlimited' }).check()
  await createItem(page, 'Lucky Ring')
  await page.getByLabel('Kind').selectOption('accessory')
  await page.getByLabel('HP Bonus').fill('2')
  await expect(page.getByRole('switch', { name: 'Unlimited' })).toHaveCount(0) // an accessory is never used up
  await saved(page)
  const items = readDisk(id, 'data/items.json').items
  expect(items.find((i: any) => i.id === 'rifle_item')).toEqual({ id: 'rifle_item', name: 'Rifle Item', kind: 'weapon', weapon: 'rifle' })
  expect(items.find((i: any) => i.id === 'lucky_ring')).toEqual({ id: 'lucky_ring', name: 'Lucky Ring', kind: 'accessory', bonus: { hp: 2 } })
  await shot(page, 'items-wide')

  // --- Structures: a shop. Only items with a price can be chosen.
  await page.goto(editorUrl(id, 'structures', 'city'))
  const sells = page.getByRole('group', { name: 'Items for sale' })
  await expect(sells.getByRole('checkbox')).toHaveCount(1)
  await sells.getByRole('checkbox', { name: 'Potion' }).check()
  await saved(page)
  expect(readDisk(id, 'data/structures.json').structures.find((s: any) => s.id === 'city').sells).toEqual(['potion'])

  // --- Units: groups and starting items with uses left and a drop.
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await expect(page.getByRole('group', { name: 'Weapons it can use' })).toBeVisible()
  await page.getByRole('group', { name: 'Can wield groups' }).getByRole('checkbox', { name: 'gun' }).check()
  const start = page.getByTestId('unit-start-items')
  await start.getByLabel('Starting items: Add an item…').selectOption('potion')
  await start.getByLabel('Starting items: Add an item…').selectOption('rifle_item')
  await start.getByLabel('Uses left: Potion').fill('1')
  await start.getByRole('checkbox', { name: 'Drops when defeated' }).first().check()
  await saved(page)
  const infantry = readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'infantry')
  expect(infantry.wields).toEqual(['gun'])
  expect(infantry.items).toEqual([{ item: 'potion', uses: 1, drop: true }, 'rifle_item'])
  await shot(page, 'units-wide')

  // --- Map: a placed unit's own items, and a side's starting storage.
  const first = readDisk(id, 'maps/crossroads.json').units[0]
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await clickCell(page, first.x, first.y)
  const props = page.getByTestId('unit-own-items')
  await expect(props).toContainText('starts with what its unit type starts with')
  await props.getByRole('switch', { name: 'Own items' }).check()
  await props.getByLabel('Items: Add an item…').selectOption('lucky_ring')
  await saved(page)
  // Switching on starts from a copy of the unit type's items.
  expect(readDisk(id, 'maps/crossroads.json').units[0].items).toEqual([{ item: 'potion', uses: 1, drop: true }, 'rifle_item', 'lucky_ring'])
  await shot(page, 'map-unit-items')
  // An own, empty list is written as an empty list (it differs from "no key").
  for (let n = 0; n < 3; n++) await props.getByRole('button', { name: /^Remove / }).first().click()
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').units[0].items).toEqual([])
  await props.getByLabel('Items: Add an item…').selectOption('potion')
  await saved(page)

  await tab(page, 'Sides').click()
  await page.getByTestId('side-storage-blue').getByLabel('Starting storage: Add an item…').selectOption('potion')
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').sides[0].storage).toEqual(['potion'])
  await shot(page, 'map-side-storage')

  // --- The proof: the loader accepts all of it.
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByRole('button', { name: 'No problems' })).toBeVisible()

  // Turning the switch off removes the key again (the unit type's items apply).
  await tab(page, 'Tool').click()
  await clickCell(page, first.x, first.y)
  await page.getByTestId('unit-own-items').getByRole('switch', { name: 'Own items' }).uncheck()
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').units[0]).not.toHaveProperty('items')
})

test('deleting an item that is used asks first and removes it everywhere; changing weapons back to fixed is confirmed', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'ItemsDelete')
  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Potion')
  await page.getByLabel('Funds Price').fill('50')
  await saved(page)
  await page.goto(editorUrl(id, 'structures', 'city'))
  await page.getByRole('group', { name: 'Items for sale' }).getByRole('checkbox', { name: 'Potion' }).check()
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await page.getByTestId('unit-start-items').getByLabel('Starting items: Add an item…').selectOption('potion')
  await saved(page)

  await page.goto(editorUrl(id, 'items', 'potion'))
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog', { name: /Delete the item/ })
  await expect(dlg).toContainText('Unit “Infantry” starts with it')
  await expect(dlg).toContainText('Structure “City” sells it')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  expect(readDisk(id, 'data/items.json').items).toEqual([])
  expect(readDisk(id, 'data/structures.json').structures.find((s: any) => s.id === 'city')).not.toHaveProperty('sells')
  expect(readDisk(id, 'data/units.json').units.find((u: any) => u.id === 'infantry')).not.toHaveProperty('items')
  expect(await game.problems(id)).toEqual([])

  // Weapon items need carried weapons: going back to fixed asks, and the check names the problem if it is confirmed.
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('Weapons', { exact: true }).selectOption('carried')
  await saved(page)
  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Blade')
  await page.getByLabel('Kind').selectOption('weapon')
  await saved(page)
  expect(await game.problems(id)).toEqual([])
  await page.goto(editorUrl(id, 'rules'))
  await page.getByLabel('Weapons', { exact: true }).selectOption('fixed')
  const ask = page.getByRole('dialog', { name: 'Make weapons fixed again?' })
  await expect(ask).toContainText('1 weapon item will stop working')
  await ask.getByRole('button', { name: 'Cancel' }).click()
  await expect(page.getByLabel('Weapons', { exact: true })).toHaveValue('carried')
  await page.getByLabel('Weapons', { exact: true }).selectOption('fixed')
  await ask.getByRole('button', { name: 'Make them fixed' }).click()
  await expect(page.getByTestId('weapon-items-fixed')).toBeVisible()
  await saved(page)
  expect(readDisk(id, 'game.json').rules).not.toHaveProperty('items')
  expect((await game.problems(id)).some((p) => p.file === 'data/items.json')).toBe(true)
  await page.goto(editorUrl(id, 'items'))
  await expect(page.getByTestId('item-checks')).toContainText('weapon items need weapons')
})

test('the Items screen and the item lists work on a phone', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'ItemsPhone')
  await page.setViewportSize({ width: 360, height: 740 })
  await page.goto(editorUrl(id, 'items'))
  await createItem(page, 'Potion')
  await expectNoHScroll(page)
  await shot(page, 'items-phone')
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await page.getByTestId('unit-start-items').getByLabel('Starting items: Add an item…').selectOption('potion')
  await expect(page.getByLabel('Uses left: Potion')).toBeVisible()
  await expectNoHScroll(page)
  await shot(page, 'units-phone')
})
