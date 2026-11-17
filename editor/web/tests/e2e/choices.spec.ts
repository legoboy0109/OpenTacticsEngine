// Dialogue choices (docs/EVENTS.md "Dialogue choices") through the real editor: an "Ask a question" command in a map event with
// answers that each have their own commands (a nested question too), reordering, the limits of two to four answers, the Problems
// list, and the exact JSON that was written and that the real game loader accepts. Then a campaign scene whose answers jump to
// another chapter, and a phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { addSceneCommand, openChapter, scene, sceneForm } from './campaignkit'
import { openMap, tool } from './mapkit'
import { addCommand, editor, eventsTab, form, nameUnit, newEvent, shot } from './storykit'

async function prepare(page: Page, game: { create: (t: string, n?: string) => Promise<string> }, name: string): Promise<string> {
  const id = await game.create('economy-tactics', name)
  await page.goto(`/p/${id}/rules`)
  await page.getByLabel('Add a switch').fill('Gate opened')
  await page.getByLabel('Add a switch').press('Enter')
  await page.getByLabel('Add a variable').fill('Gate answer')
  await page.getByLabel('Add a variable').press('Enter')
  await page.getByLabel('Add a variable').fill('Mood')
  await page.getByLabel('Add a variable').press('Enter')
  await saved(page)
  return id
}

/** An answer's own list of commands ("Question 1: Answer 2"). */
const answerList = (q: number, a: number, outer = '') => `${outer}Question ${q}: Answer ${a}`

test('@smoke a question in a map event: answers with their own commands, a hidden answer, a nested question, reorder, limits: the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await prepare(page, game, 'Choices')
  const mapFile = 'maps/crossroads.json'
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 2, 2, 'eirik')
  await eventsTab(page).click()
  await newEvent(page, 'gate')

  // --- the question
  await addCommand(page, 'Then', 'Ask a question')
  await expect(form(page).getByTestId('choice-form')).toBeVisible()
  await form(page).getByLabel('Who speaks').selectOption({ label: 'Infantry (eirik)' })
  await form(page).getByLabel('The question').fill('An old gate blocks the path. What now?')
  await form(page).getByLabel('Remember the answer in').selectOption({ label: 'Gate answer' })
  // A new question starts with two answers, Yes and No.
  await form(page).getByLabel('Words of the answer (answer 1)').fill('Open it')
  await form(page).getByLabel('Words of the answer (answer 2)').fill('Go around')
  await shot(page, 'choices-form')
  // The limits: two to four answers.
  await expect(form(page).getByRole('button', { name: 'Delete answer 1' })).toBeDisabled()
  await form(page).getByRole('button', { name: 'Add an answer' }).click()
  await form(page).getByLabel('Words of the answer (answer 3)').fill('Ask Eirik first')
  await form(page).getByRole('button', { name: 'Add an answer' }).click()
  await expect(form(page).getByRole('button', { name: 'Add an answer' })).toBeDisabled()
  await expect(form(page).getByText('A question has at most four answers.')).toBeVisible()
  // Problems list: an empty answer is reported at once, then fixed.
  await expect(page.getByTestId('event-checks')).toContainText('answer 4: the words of the answer are empty')
  await form(page).getByRole('button', { name: 'Delete answer 4' }).click()
  await expect(form(page).getByRole('button', { name: 'Add an answer' })).toBeEnabled()
  await expect(page.getByTestId('event-checks')).toHaveCount(0)

  // --- answer 3 is only offered while a switch is on
  const cond = form(page).getByRole('group', { name: 'Answer 3: Condition 1' })
  await form(page).getByLabel('Answer 3: Add a condition…').selectOption({ label: 'A switch is on or off' })
  await cond.getByLabel('Switch', { exact: true }).selectOption({ label: 'Gate opened' })

  // --- each answer's own commands, listed under the question
  await addCommand(page, answerList(1, 1), 'Say something')
  await form(page).getByLabel('Who speaks').selectOption({ label: 'Infantry (eirik)' })
  await form(page).getByLabel('What is said').fill('It creaks open.')
  await addCommand(page, answerList(1, 1), 'Turn a switch on or off')
  await form(page).getByLabel('Switch', { exact: true }).selectOption({ label: 'Gate opened' })
  await addCommand(page, answerList(1, 2), 'Say something')
  await form(page).getByLabel('What is said').fill('A long way round.')
  // a question inside an answer
  await addCommand(page, answerList(1, 3), 'Ask a question')
  await form(page).getByLabel('The question').fill('Eirik shrugs. Trust him?')
  await form(page).getByLabel('Remember the answer in').selectOption({ label: 'Mood' })
  await addCommand(page, `${answerList(1, 3)}, ${answerList(1, 1)}`, 'Say something')
  await form(page).getByLabel('What is said').fill('Then it is settled.')
  await shot(page, 'choices-event')
  await saved(page)

  expect(readDisk(id, mapFile).events).toEqual([{
    id: 'gate',
    when: { battle: 'start' },
    do: [{
      choice: 'An old gate blocks the path. What now?',
      speaker: 'eirik',
      store: 'gate_answer',
      answers: [
        { text: 'Open it', do: [{ say: 'It creaks open.', speaker: 'eirik' }, { setSwitch: 'gate_opened', to: true }] },
        { text: 'Go around', do: [{ say: 'A long way round.' }] },
        {
          text: 'Ask Eirik first',
          if: [{ switch: 'gate_opened' }],
          do: [{
            choice: 'Eirik shrugs. Trust him?',
            store: 'mood',
            answers: [{ text: 'Yes', do: [{ say: 'Then it is settled.' }] }, { text: 'No' }],
          }],
        },
      ],
    }],
  }])
  expect(await game.problems(id)).toEqual([])

  // --- reorder an answer: it takes its commands with it; the whole thing is one undo step
  await editor(page).getByRole('button', { name: /^1\. Infantry asks/ }).click()
  await form(page).getByRole('button', { name: 'Move answer 2 up' }).click()
  await saved(page)
  expect(readDisk(id, mapFile).events[0].do[0].answers.map((a: any) => a.text)).toEqual(['Go around', 'Open it', 'Ask Eirik first'])
  expect(readDisk(id, mapFile).events[0].do[0].answers[0].do).toEqual([{ say: 'A long way round.' }])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, mapFile).events[0].do[0].answers.map((a: any) => a.text)).toEqual(['Open it', 'Go around', 'Ask Eirik first'])

  // --- two questions that remember their answer in one variable are a problem the maker is told about
  await addCommand(page, 'Then', 'Ask a question')
  await form(page).getByLabel('The question').fill('Another?')
  await form(page).getByLabel('Remember the answer in').selectOption({ label: 'Gate answer' })
  await expect(page.getByTestId('event-checks')).toContainText('also saves its answer in this variable')
  await form(page).getByLabel('Remember the answer in').selectOption({ label: 'Do not remember it' })
  await expect(page.getByTestId('event-checks')).toHaveCount(0)
  await saved(page)
  expect(await game.problems(id)).toEqual([])
})

test('@smoke a question in a campaign scene: an answer jumps to another chapter, in the scene before the battle and after it', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await prepare(page, game, 'Branches')
  await page.goto(editorUrl(id, 'campaign'))
  await page.getByRole('button', { name: 'Create a campaign' }).click()
  await page.getByRole('button', { name: 'Add a chapter' }).click()
  await page.getByLabel('Name of the chapter').fill('Tutorial')
  await page.keyboard.press('Enter')
  await page.getByRole('button', { name: 'Add a chapter' }).click()
  await page.getByLabel('Name of the chapter').fill('The Road')
  await page.keyboard.press('Enter')
  await saved(page)

  // Chapter 1's first scene asks (a free speaker name only) and jumps: "Go to a chapter" is offered before the battle too.
  await openChapter(page, 'Chapter 1')
  await expect(scene(page, 'before').getByRole('option', { name: 'Go to a chapter' })).toHaveCount(1)
  await expect(scene(page, 'before').getByRole('option', { name: 'Ask a question' })).toHaveCount(1)
  await addSceneCommand(page, 'before', 'Before the battle', 'Ask a question')
  await sceneForm(page, 'before').getByLabel('Who speaks').selectOption({ label: 'Someone else (type a name)' })
  await sceneForm(page, 'before').getByLabel('Name shown').fill('Guide')
  await sceneForm(page, 'before').getByLabel('The question').fill('Play the tutorial?')
  await sceneForm(page, 'before').getByLabel('Words of the answer (answer 1)').fill('Yes')
  await sceneForm(page, 'before').getByLabel('Words of the answer (answer 2)').fill('No, straight to the road')
  await sceneForm(page, 'before').getByLabel('Remember the answer in').selectOption({ label: 'Mood' })
  await addSceneCommand(page, 'before', answerList(1, 1, 'Before the battle, '), 'Go to a chapter')
  await sceneForm(page, 'before').getByLabel('Next chapter').selectOption({ label: 'Tutorial' })
  await addSceneCommand(page, 'before', answerList(1, 2, 'Before the battle, '), 'Go to a chapter')
  await sceneForm(page, 'before').getByLabel('Next chapter').selectOption({ label: 'The Road' })
  await shot(page, 'choices-scene')
  await saved(page)

  const campaign = readDisk(id, 'data/campaign.json')
  expect(campaign.chapters[0].before).toEqual([{
    choice: 'Play the tutorial?',
    speakerName: 'Guide',
    store: 'mood',
    answers: [{ text: 'Yes', do: [{ goto: 'tutorial' }] }, { text: 'No, straight to the road', do: [{ goto: 'the_road' }] }],
  }])
  expect(await game.problems(id)).toEqual([])

  // A jump to the chapter's own name is refused in plain words.
  await addSceneCommand(page, 'before', answerList(1, 1, 'Before the battle, '), 'Go to a chapter')
  await sceneForm(page, 'before').getByLabel('Next chapter').selectOption({ label: 'Chapter 1' })
  await expect(page.getByTestId('campaign-checks')).toContainText('can only go to another chapter, not its own')
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the question form and its answers at 360 px have no sideways scroll, with four long answers', async ({ page, game }) => {
    test.setTimeout(90_000)
    const id = await prepare(page, game, 'Phone')
    await openMap(page, id, 'crossroads')
    await eventsTab(page).click()
    await newEvent(page, 'gate')
    await addCommand(page, 'Then', 'Ask a question')
    await form(page).getByLabel('The question').fill('A very long question that has to wrap inside the narrow screen without making the page wider than the phone?')
    const long = 'An answer with many words that also has to wrap inside the narrow screen of a phone'
    for (let n = 1; n <= 2; n++) await form(page).getByLabel(`Words of the answer (answer ${n})`).fill(`${long} (${n})`)
    for (let n = 3; n <= 4; n++) {
      await form(page).getByRole('button', { name: 'Add an answer' }).click()
      await form(page).getByLabel(`Words of the answer (answer ${n})`).fill(`${long} (${n})`)
    }
    await expectNoHScroll(page)
    await addCommand(page, answerList(1, 4), 'Ask a question')
    await expectNoHScroll(page)
    await shot(page, 'choices-phone')
    await expect(editor(page).getByTestId('answer-4')).toBeVisible()
  })
})
