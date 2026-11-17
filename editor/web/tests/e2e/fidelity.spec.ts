// The fidelity options in the real editor (docs/SRPG.md section 11): the hit roll and experience options on the Rules screen, a unit's
// top level, an ability's experience and condition and the computer's stat caps. The proof that matters is the exact files the editor
// wrote and that the real game loader accepts them (/problems is empty). Desktop and a 360 px phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'

const open = async (page: Page, id: string, section: string, item?: string) => {
  await page.goto(editorUrl(id, section, item))
  // On a phone an item page may show only its own form (no screen title): wait for the page, whichever it shows.
  await expect(page.locator('main, [role="main"], .screen').first()).toBeVisible()
}

const problems = async (page: Page, id: string): Promise<string[]> => {
  const res = await (await page.request.get(`/api/projects/${id}/problems`)).json()
  return (res.problems ?? []).map((p: { message: string }) => p.message)
}

for (const phone of [false, true]) {
  test(`the fidelity options are forms that write exactly their keys (${phone ? '360 px' : 'desktop'})`, async ({ page, game }) => {
    test.setTimeout(180_000)
    if (phone) await page.setViewportSize({ width: 360, height: 740 })
    const id = await game.create('strategy-rpg', 'Fidelity')

    // --- Rules: nothing is written until a choice is made; the note about the forecast appears with the average of two.
    await open(page, id, 'rules')
    const card = page.getByTestId('fidelity-rules')
    await card.scrollIntoViewIfNeeded()
    await expect(card.getByLabel('How a hit is rolled')).toHaveValue('single')
    expect(readDisk(id, 'game.json').rules).not.toHaveProperty('hitRoll')
    await expect(page.getByTestId('hit-roll-note')).toHaveCount(0)
    await card.getByLabel('How a hit is rolled').selectOption({ label: 'Average of two numbers' })
    await saved(page)
    expect(readDisk(id, 'game.json').rules.hitRoll).toBe('average2')
    await expect(page.getByTestId('hit-roll-note')).toContainText('forecast still shows')
    await card.getByLabel('Experience for a fight that did no damage').fill('3')
    await saved(page)
    await card.getByLabel('Computer-controlled units gain experience').uncheck()
    await saved(page)
    const progression = readDisk(id, 'game.json').rules.progression
    expect(progression.xpNoDamage).toBe(3)
    expect(progression.computerGains).toBe(false)
    // A bad formula-free value is flagged by the editor before the loader sees it, and the default is never written.
    await card.getByLabel('Computer-controlled units gain experience').check()
    await saved(page)
    expect(readDisk(id, 'game.json').rules.progression).not.toHaveProperty('computerGains')
    if (phone) await expectNoHScroll(page)

    // --- Computer caps: the rules screen shows the table once the game has stat caps.
    const caps = page.getByTestId('computer-caps')
    if (await caps.count()) {
      await caps.scrollIntoViewIfNeeded()
      const first = caps.getByRole('spinbutton').first()
      await first.fill('120')
      await saved(page)
      expect(Object.values(readDisk(id, 'data/caps.json').computerCaps)).toContain(120)
    }

    // --- A unit's top level.
    const doc = await (await page.request.get(`/api/projects/${id}`)).json()
    const unit = doc.units[0].id
    await open(page, id, 'units', unit)
    const top = page.getByLabel('Top level')
    await top.scrollIntoViewIfNeeded()
    await top.fill('0')
    await saved(page)
    expect(readDisk(id, 'data/units.json').units.find((u: { id: string }) => u.id === unit).maxLevel).toBe(0)
    await top.fill('')
    await saved(page)
    expect(readDisk(id, 'data/units.json').units.find((u: { id: string }) => u.id === unit)).not.toHaveProperty('maxLevel')
    if (phone) await expectNoHScroll(page)

    // --- An ability's experience and condition.
    if (doc.abilities?.length && doc.conditions?.length) {
      await open(page, id, 'abilities', doc.abilities[0].id)
      const extras = page.getByTestId('ability-extras')
      await extras.scrollIntoViewIfNeeded()
      await extras.getByLabel('Experience for the user').fill('7')
      await saved(page)
      await extras.getByLabel('Gives its target a condition').selectOption({ index: 1 })
      await saved(page)
      const ability = readDisk(id, 'data/abilities.json').abilities[0]
      expect(ability.xp).toBe(7)
      expect(ability.inflict.condition).toBe(doc.conditions[0].id)
      if (phone) await expectNoHScroll(page)
    }
    expect(await problems(page, id)).toEqual([])
    await gotoSection(page, 'Rules')
    await expect(page.getByRole('heading', { level: 1 }).first()).toBeVisible()
  })
}
