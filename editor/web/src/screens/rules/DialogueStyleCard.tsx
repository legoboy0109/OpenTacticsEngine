import { BUBBLE_PORTRAITS, DIALOGUE_STYLES, bubbleNames, bubblePortrait, dialogueProblems, dialogueStyle, setBubbleNames, setBubblePortrait, setDialogueStyle } from '../../model/dialogueStyle'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Check } from '../../ui/Toggle'
import { DS } from './dialogueStyleText'

/** How story lines are shown: the box at the bottom, or speech bubbles (game.json "dialogue", docs/EVENTS.md "Speech bubbles"). */
export function DialogueStyleCard() {
  const doc = useDoc()
  const edit = useEdit()
  const style = dialogueStyle(doc)
  const portrait = bubblePortrait(doc)
  return (
    <Card title={DS.title} help={DS.help} id="dialogue-style">
      <div className="stack tight" data-testid="dialogue-style">
        <div className="kind-row" role="radiogroup" aria-label={DS.style}>
          {DIALOGUE_STYLES.map((s) => (
            <button key={s} type="button" role="radio" aria-checked={style === s} className="pick pick-inline" onClick={() => edit((d) => setDialogueStyle(d, s))}>
              <span className="pick-name">{DS.styleChoices[s].label}</span>
            </button>
          ))}
        </div>
        <p className="help"><strong>{DS.style}.</strong> {DS.styleChoices[style].help}</p>
        {style === 'bubble' && (
          <>
            <div className="kind-row" role="radiogroup" aria-label={DS.portrait}>
              {BUBBLE_PORTRAITS.map((p) => (
                <button key={p} type="button" role="radio" aria-checked={portrait === p} className="pick pick-inline" onClick={() => edit((d) => setBubblePortrait(d, p))}>
                  <span className="pick-name">{DS.portraitChoices[p].label}</span>
                </button>
              ))}
            </div>
            <p className="help"><strong>{DS.portrait}.</strong> {DS.portraitChoices[portrait].help}</p>
            <Check label={DS.names} help={DS.namesHelp} checked={bubbleNames(doc)} onChange={(v) => edit((d) => setBubbleNames(d, v))} />
          </>
        )}
        {dialogueProblems(doc).map((p) => <p key={p} className="help" role="alert">{p}</p>)}
      </div>
    </Card>
  )
}
