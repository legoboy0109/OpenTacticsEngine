// Fuel, fuel lost every turn and supplying on a unit type (docs/ECONOMY.md "Repair and supply").
import { FUEL_MAX, FUEL_START, setFuel, setFuelPerTurn, setSupplies, startingBurn } from '../../model/supplyUnits'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function SupplyCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit) return null
  const key = `unit:${id}`
  const fuel = unit.fuel
  const burn = unit.fuelPerTurn
  const canBurn = fuel !== undefined && fuel >= 2
  return (
    <Card title={T.supplyTitle} help={T.supplyHelp}>
      <div className="stack">
        <Check label={T.fuel} help={T.fuelHelp} checked={fuel !== undefined} onChange={(on) => edit((d) => setFuel(d, id, on ? FUEL_START : undefined))} />
        {fuel !== undefined && (
          <Field label={T.fuelAmount} help={T.fuelAmountHelp}>
            <NumberInput value={fuel} min={1} max={FUEL_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setFuel(d, id, v), `${key}:fuel`) }} />
          </Field>
        )}
        <Check label={T.burn} help={fuel === undefined ? T.burnNeedsFuel : !canBurn ? T.burnTankSmall : T.burnHelp} checked={burn !== undefined} disabled={!canBurn && burn === undefined}
          onChange={(on) => edit((d) => setFuelPerTurn(d, id, on ? startingBurn(fuel ?? 0) ?? undefined : undefined))} />
        {burn !== undefined && (
          <Field label={T.burnAmount} help={T.burnAmountHelp(fuel)}>
            <NumberInput value={burn} min={1} max={fuel !== undefined ? Math.max(1, fuel - 1) : FUEL_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setFuelPerTurn(d, id, v), `${key}:burn`) }} />
          </Field>
        )}
        <Check label={T.supplies} help={T.suppliesHelp} checked={unit.supplies === true} onChange={(on) => edit((d) => setSupplies(d, id, on))} />
      </div>
    </Card>
  )
}
