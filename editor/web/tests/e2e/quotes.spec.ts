// Battle and defeat quotes (docs/EVENTS.md "Battle and defeat quotes") through the real editor: the Quotes card of a character (battle
// quotes with an opponent and a frequency, lines in the story's own line editor, defeat quotes by the campaign's fallen rule), the
// checks, the delete question and clean-up, the exact JSON that was written and that the real game loader accepts, and a phone.
import type { Page } from '@playwright/test'
import { newCharacter, switchOn } from './characterskit'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { shot } from './storykit'

const card = (page: Page) => page.getByTestId('character-quotes')
/** The open line's form in one list of lines (each list keeps its own). */
const formOf = (page: Page, list: string) => card(page).getByTestId(`commands-${list}`).locator('.command-form')

/** Adds a "Say something" line to one of the card's lists ("Battle quote 1", "Defeat quote"...) and fills it in. */
async function say(page: Page, list: string, text: string, speaker?: string): Promise<void> {
  await card(page).getByLabel(`${list}: Add a command…`, { exact: true }).selectOption({ label: 'Say something' })
  const form = formOf(page, list)
  if (speaker) await form.getByLabel('Who speaks').selectOption({ label: speaker })
  await form.getByLabel('What is said').fill(text)
}

async function people(page: Page, game: { create: (t: string, n?: string) => Promise<string> }, name: string): Promise<string> {
  const id = await game.create('strategy-rpg', name)
  await switchOn(page, id)
  for (const who of ['Eirik', 'Gorm', 'Mira']) await newCharacter(page, who)
  await saved(page)
  return id
}

const characters = (id: string) => readDisk(id, 'data/characters.json').characters as Array<{ id: string; quotes?: any }>
const quotesOf = (id: string, who: string) => characters(id).find((c) => c.id === who)?.quotes

test('@smoke add battle and defeat quotes: the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await people(page, game, 'Quotes')
  await page.goto(editorUrl(id, 'characters/eirik'))
  await expect(card(page)).toBeVisible()
  await expect(card(page).getByTestId('no-battle-quotes')).toBeVisible()
  // Without a campaign one defeat set is offered.
  await expect(card(page).getByTestId('defeat-quote-falls')).toBeVisible()
  await expect(card(page).getByTestId('defeat-quote-retreats')).toHaveCount(0)

  // --- a quote against Gorm, once in the whole campaign
  await card(page).getByTestId('add-battle-quote').click()
  await expect(card(page).getByTestId('battle-quote-1')).toBeVisible()
  await card(page).getByLabel('Fighting (Battle quote 1)').selectOption({ label: 'Gorm' })
  await card(page).getByLabel('How often (Battle quote 1)').selectOption({ label: 'Once in the whole campaign' })
  // The new quote came with one empty line; the list is the story's: open it and write the words.
  await card(page).getByTestId('battle-quote-lines-1').getByRole('button', { name: /^1\./ }).click()
  // A line with no speaker is the character's own: the empty speaker choice says so (not "Nobody (a narrator)").
  const speaker = formOf(page, 'Battle quote 1').getByLabel('Who speaks')
  await expect(speaker.locator('option:checked')).toHaveText('This character (Eirik)')
  await expect(speaker.locator('option', { hasText: 'narrator' })).toHaveCount(0)
  await formOf(page, 'Battle quote 1').getByLabel('What is said').fill('Gorm! Your raids end today.')
  // A second line, said by a friend, through the "Add a command" list.
  await say(page, 'Battle quote 1', 'Steady, Eirik.', 'Mira (mira)')
  // A general quote, every time.
  await card(page).getByTestId('add-battle-quote').click()
  await card(page).getByLabel('How often (Battle quote 2)').selectOption({ label: 'Every time' })
  await card(page).getByTestId('battle-quote-lines-2').getByRole('button', { name: /^1\./ }).click()
  await formOf(page, 'Battle quote 2').getByLabel('What is said').fill('For the valley!')
  // The defeat quote.
  await say(page, 'Defeat quote', 'Tell them I tried.')
  await shot(page, 'quotes-card')
  await saved(page)

  expect(quotesOf(id, 'eirik')).toEqual({
    battle: [
      { against: 'gorm', once: 'campaign', lines: [{ say: 'Gorm! Your raids end today.' }, { say: 'Steady, Eirik.', speaker: 'mira' }] },
      { once: 'always', lines: [{ say: 'For the valley!' }] },
    ],
    defeat: { falls: [{ say: 'Tell them I tried.' }] },
  })
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('character-checks')).not.toContainText('Battle quote')  // (it still says the characters are not on a map)

  // --- reorder: the second quote moves up, one undo step
  await card(page).getByRole('button', { name: 'Move battle quote 2 up' }).click()
  await saved(page)
  expect(quotesOf(id, 'eirik').battle.map((q: any) => q.against ?? 'anyone')).toEqual(['anyone', 'gorm'])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(quotesOf(id, 'eirik').battle.map((q: any) => q.against ?? 'anyone')).toEqual(['gorm', 'anyone'])

  // --- checks: a second quote against Gorm is only said never, the maker is told in plain words
  await card(page).getByLabel('Fighting (Battle quote 2)').selectOption({ label: 'Gorm' })
  await expect(page.getByTestId('character-checks')).toContainText('Battle quote 2 is a second one against Gorm')
  await card(page).getByLabel('Fighting (Battle quote 2)').selectOption({ label: 'Anyone' })
  await expect(page.getByTestId('character-checks')).not.toContainText('Battle quote')

  // --- remove a quote
  await card(page).getByRole('button', { name: 'Remove battle quote 1' }).click()
  await saved(page)
  expect(quotesOf(id, 'eirik').battle).toEqual([{ once: 'always', lines: [{ say: 'For the valley!' }] }])
  expect(await game.problems(id)).toEqual([])
})

test('a campaign that brings fallen units back offers both defeat sets; deleting a character follows through its quotes', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await people(page, game, 'Quotes campaign')
  await page.goto(editorUrl(id, 'characters/eirik'))
  // Give Eirik a quote against Gorm and a line Mira speaks, straight to the file's shape through the card.
  await card(page).getByTestId('add-battle-quote').click()
  await card(page).getByLabel('Fighting (Battle quote 1)').selectOption({ label: 'Gorm' })
  await card(page).getByTestId('battle-quote-lines-1').getByRole('button', { name: /^1\./ }).click()
  await formOf(page, 'Battle quote 1').getByLabel('What is said').fill('End of the road.')
  await say(page, 'Battle quote 1', 'I am with you.', 'Mira (mira)')
  await saved(page)

  // The campaign's setting decides which defeat sets the card has.
  await page.goto(editorUrl(id, 'campaign'))
  await page.getByRole('button', { name: 'Create a campaign' }).click()
  await page.getByRole('button', { name: 'Add a chapter' }).click()
  await page.getByLabel('Name of the chapter').fill('One')
  await page.keyboard.press('Enter')
  await saved(page)
  await page.getByLabel('When a unit is defeated').selectOption({ label: 'It comes back next chapter' })
  await saved(page)
  await page.goto(editorUrl(id, 'characters/eirik'))
  await expect(card(page).getByTestId('defeat-quote-falls')).toBeVisible()
  await expect(card(page).getByTestId('defeat-quote-retreats')).toBeVisible()
  await say(page, 'Retreat quote', 'I must pull back!')
  await saved(page)
  expect(quotesOf(id, 'eirik').defeat).toEqual({ retreats: [{ say: 'I must pull back!' }] })
  expect(await game.problems(id)).toEqual([])

  // Deleting Gorm: the question names the quote, and the quote goes with him.
  await page.goto(editorUrl(id, 'characters/gorm'))
  await page.getByRole('button', { name: 'Delete', exact: true }).first().click()
  await expect(page.getByRole('dialog')).toContainText('Eirik’s battle quote against it')
  await page.getByRole('dialog').getByRole('button', { name: /^Delete/ }).click()
  await saved(page)
  expect(quotesOf(id, 'eirik')).toEqual({ defeat: { retreats: [{ say: 'I must pull back!' }] } })
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the Quotes card at 360 px has no sideways scroll, with long lines', async ({ page, game }) => {
    test.setTimeout(90_000)
    const id = await people(page, game, 'Quotes phone')
    await page.goto(editorUrl(id, 'characters/eirik'))
    await card(page).getByTestId('add-battle-quote').click()
    await card(page).getByTestId('battle-quote-lines-1').getByRole('button', { name: /^1\./ }).click()
    await formOf(page, 'Battle quote 1').getByLabel('What is said').fill('A very long line that has to wrap inside the narrow screen of a phone without making the page wider than the phone itself.')
    await say(page, 'Defeat quote', 'Another quite long line for the defeat quote that also has to wrap on a phone screen.')
    await expectNoHScroll(page)
    await shot(page, 'quotes-phone')
    await expect(card(page).getByTestId('battle-quote-1')).toBeVisible()
  })
})
