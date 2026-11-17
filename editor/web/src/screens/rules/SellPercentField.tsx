import { SELL_PERCENT_MAX, SELL_PERCENT_MIN, sellPercent, setSellPercent } from '../../model/sell'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { S } from './sellText'

/** What a shop that buys items back pays, as a percent of the price (rules.items.sellPercent). */
export function SellPercentField() {
  const doc = useDoc()
  const edit = useEdit()
  return (
    <Field label={S.sellPercent} help={S.sellPercentHelp}>
      <NumberInput value={sellPercent(doc)} min={SELL_PERCENT_MIN} max={SELL_PERCENT_MAX}
        onChange={(v) => { if (v !== undefined) edit((d) => setSellPercent(d, v), 'rules:sellPercent') }} />
    </Field>
  )
}
