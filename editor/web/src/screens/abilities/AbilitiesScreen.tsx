// The Abilities screen. E0 gives it a frame only (docs/SRPG.md build order): its owner step puts its screen in the frame.
import { SrpgScreen } from '../srpg/SrpgScreen'

export function AbilitiesScreen({ itemId: _itemId }: { itemId?: string }) {
  return <SrpgScreen feature="abilities" />
}
