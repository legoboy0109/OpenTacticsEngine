import { setFontField, setFontName } from '../../model/game'
import { useDoc, useEdit } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select, type Option } from '../../ui/Select'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'
import { useFonts } from './useFonts'

const CUSTOM = '__file'

export function FontCard() {
  const game = useDoc().game!
  const edit = useEdit()
  const fonts = useFonts().filter((f) => f.id !== 'pixel')
  const font = game.font
  const hasFile = typeof font?.file === 'string'
  // "pixel" is the built-in default: same as having no font key at all.
  const value = hasFile ? CUSTOM : !font?.name || font.name === 'pixel' ? '' : font.name
  const info = fonts.find((f) => f.id === value)

  const options: Option[] = [{ value: '', label: T.fontDefault }, ...fonts.map((f) => ({ value: f.id, label: f.name }))]
  // A name the helper does not list is kept as a choice so it is never silently lost.
  if (value && !hasFile && !info) options.push({ value, label: value })
  if (hasFile) options.push({ value: CUSTOM, label: T.fontCustomFile(font!.file as string) })

  const pick = (v: string) => {
    if (v === CUSTOM) return
    edit((d) => setFontName(d, v === '' ? undefined : v))
  }
  const customised = value !== ''

  return (
    <Card title={T.font} help={T.fontHelp}>
      <div className="stack">
        <Field label={T.fontPick}><Select value={value} onChange={pick} options={options} /></Field>
        {customised && !hasFile ? (
          <FormGrid>
            <Field label={T.fontSize} help={T.fontSizeHelp(info?.size)}>
              <NumberInput value={font?.size} min={4} max={96} allowEmpty placeholder={info ? String(info.size) : undefined}
                onChange={(v) => edit((d) => setFontField(d, 'size', v), 'game:fontsize')} />
            </Field>
            <div className="field-toggle">
              <Toggle label={T.fontSmooth} help={T.fontSmoothHelp} checked={font?.smooth ?? info?.smooth ?? true}
                onChange={(v) => edit((d) => setFontField(d, 'smooth', v))} />
            </div>
          </FormGrid>
        ) : !hasFile && <p className="help">{T.fontFixed}</p>}
      </div>
    </Card>
  )
}
