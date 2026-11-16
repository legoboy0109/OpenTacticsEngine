// A whole-number box for the rule builder: reports only real numbers (an empty or half-typed box reports nothing).
import { NumberInput } from '../../ui/NumberInput'

export function Num({ value, onChange, min, max, suffix, label }: { value: number; onChange: (v: number) => void; min?: number; max?: number; suffix?: string; label?: string }) {
  return <NumberInput value={value} min={min} max={max} suffix={suffix} aria-label={label} onChange={(v) => { if (v !== undefined) onChange(v) }} />
}
