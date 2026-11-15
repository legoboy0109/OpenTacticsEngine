import { setGameText, setView } from '../../model/game'
import { gridLabel, gridOfGame } from '../../model/gridSpec'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid, PageHeader } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { FontCard } from './FontCard'
import { StylesCard } from './StylesCard'
import { T } from './text'

export function GameScreen() {
  return (
    <div className="screen">
      <PageHeader title={T.title} help={T.intro} />
      <Needs parts={['game']}><GameForm /></Needs>
    </div>
  )
}

function GameForm() {
  const game = useDoc().game!
  const edit = useEdit()
  return (
    <>
      <Card title={T.about}>
        <div className="stack">
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={game.name} onChange={(v) => edit((d) => setGameText(d, 'name', v), 'game:name')} />
          </Field>
          <Field label={T.description} help={T.descriptionHelp}>
            <TextInput multiline rows={2} value={game.description ?? ''} onChange={(v) => edit((d) => setGameText(d, 'description', v), 'game:description')} />
          </Field>
          <Field label={T.author} help={T.authorHelp}>
            <TextInput value={game.author ?? ''} onChange={(v) => edit((d) => setGameText(d, 'author', v), 'game:author')} />
          </Field>
        </div>
      </Card>
      <Card title={T.gridTitle} help={T.gridHelp}>
        <div className="label-sm">{T.gridLabel}</div>
        <strong data-testid="game-grid">{gridLabel(gridOfGame(game))}</strong>
      </Card>
      <FontCard />
      <StylesCard />
      <Advanced label={T.advanced}>
        <div className="stack">
          <p className="help">{T.screenHelp}</p>
          <FormGrid cols={3}>
            <Field label={T.width}>
              <NumberInput value={game.view?.width} min={64} max={4096} allowEmpty onChange={(v) => edit((d) => setView(d, 'width', v), 'game:vw')} />
            </Field>
            <Field label={T.height}>
              <NumberInput value={game.view?.height} min={64} max={4096} allowEmpty onChange={(v) => edit((d) => setView(d, 'height', v), 'game:vh')} />
            </Field>
            <Field label={T.tile} help={T.tileHelp}>
              <NumberInput value={game.view?.tileSize} min={4} max={256} allowEmpty onChange={(v) => edit((d) => setView(d, 'tileSize', v), 'game:tile')} />
            </Field>
          </FormGrid>
        </div>
      </Advanced>
    </>
  )
}
