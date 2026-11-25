# Mascot Review Rubric

How a review of a change to Claude's mascot or its sounds is scored — by the `mascot-animator` agent in review mode, or
anyone following the `mascot-animation` skill's review: the verdict, the evidence each result needs, and one item per
step of the review. People read it to know what a mascot review will pass or block; coding agents read it before they
score one.

**Where it lives.** In the `mascot-animation` skill, beside the rules it checks — [its SKILL.md](../SKILL.md),
[character.md](character.md) (MASCOT) and [build.md](build.md). A project may add items it learned from what the
developer saw on the glass or heard from the speaker; they live with the project (in the agent-harness kit, the Learned
table in `.agents/rubrics/mascot-animator.md`), and a review scores them too. The host's files, docs, stage and records
are the host's side (SKILL.md → The host's side).

**Stability.** Cite the doc as `MASCOT_REVIEW_RUBRIC §n` and each item by its number, `R1`–`R11`. Do not renumber; add an
item under the next free number.

| § | Section | Look here for |
| --- | --- | --- |
| 1 | [Verdict](#1-verdict) | When a review blocks, when it passes, what never blocks |
| 2 | [Evidence](#2-evidence) | What each result must show |
| 3 | [Items](#3-items) | R1–R11, each with the rule it rests on and the review step that scores it |

---

## 1. Verdict

**A review blocks when an item fails against a skill rule or MASCOT §, or the build skipped the gate; otherwise it passes.**

- **blocking** — the mascot's files changed with no yes naming a GIF in the host's harness (here `verify.py status`
  lists a `motion` approval) and no recorded `no visible change` decision. It also blocks when the yes came after the first run
  that built the move, or the approved GIF changed since the yes. It blocks on a movement with no moment told, or a
  moment told every frame or twice for one movement. It blocks on a draw cut off at the top, the foot or in a corner
  that no approved draft showed. On a flipbook it blocks on a move over the host's frame cap, an idle loop, or a move
  that does not end on rest.
- **pass** — every item passes, is n/a or unsure, and should-fix findings and notes do not block.
- **Never blocks** — a decision the developer recorded for this task (here `verify.py decision`): a judge notes it.

## 2. Evidence

**A result without evidence counts as unsure.**

- **fail** — the file and line, the line quoted, and the rule; for a sound, the scenario's event log lines
  (`<ms> ms  sfx <name>`) and the beat they miss; for the reach, the shot or `reach()` line and what is cut off; on a
  flipbook, `flipbook()`'s frames and window.
- **pass** — what the review read or ran: the files and lines checked, the harness's status lines, the moment tally against
  the beats, or the command and its hits against the baseline.
- **unsure** — what could not be settled, and what would settle it: the glass, the speaker, the developer.
- **n/a** — why the item does not touch this change.

## 3. Items

**One item per step of the review; the step is the agent's review mode's.**

| Item | Looks for | Review step |
| --- | --- | --- |
| R1 | The approved GIF named before any edit: a `motion` approval naming the GIF in the host's harness, the file unchanged since the yes, earlier than the first run that touched the mascot's files — or a `no visible change: <why>` decision; a judge's pass, a sheet or the agent's reading is never the yes (mascot-animation Do 1; the host's side, the gate's records) | 1 |
| R2 | The build's numbers equal the approved draft: keys, beats, thresholds and frames field for field; the host's test shots match it beat for beat (mascot-animation Do 2; build.md → Simulate) | 2 |
| R3 | A sound on every movement: each beat of the draft named with its sound, and every movement in the built move telling a moment at its beat in the host's event log (mascot-animation Do 6; MASCOT §4, §5) | 3 |
| R4 | Each moment told once per crossing, from the pose, with a state flag; none on consecutive frames, none twice for one movement; a new `MascotSfx` named in every place, in order, appended (MASCOT §4 rules 1–3, 10) | 4 |
| R5 | A sound 8-bit, short, in the motion's direction, not coin-like; no sound on a tap, a press or the knob; a phrase only music, made from its move's current key times (MASCOT §4 rules 5–7; the host's doc on its cues) | 5 |
| R6 | Every change of motion with its anticipation, impact and settle — on a flipbook, carried by the key poses chosen; feet planted; hands over the body tinted, sides shaded, never the body's colour over the body (mascot-animation Do 3–5, Never; MASCOT §3, §6) | 6 |
| R7 | The reach on the glass: nothing cut off at the top, the foot or in a corner (`reach()`'s `on_glass` and `corner`), in front of what the page lays under it, which a move may cross, only a travelling move off a side, wholly, and back from the other — judged from `mp.reach()` or the shots, never from a constant (mascot-animation Do 8; the host's doc on the mascot's place) | 7 |
| R8 | The drawing: integer maths, deterministic; each moving part on a small canvas, each tilted canvas no bigger than the work's, a travelling move moving its parts not the root; colours only from the mascot's roles and those roles only on the mascot; no haptic pattern or LED effect fired by a move (mascot-animation Do 9–10, Never; build.md; the `motion` skill's frame budget; the host's design system's mascot colours) | 8 |
| R9 | Docs and tags in the same change: the host's doc on which event plays which move, its mascot sections, its design system's mascot colours, the model sheet when the character changed; every number tagged (mascot-animation Do 12–14) | 9 |
| R10 | The board and the glass: flashed and booted clean (here `verify.py target`), the move looping alone on the host's stage with its sounds, and the device back on the shipping build after, a "Judge on the glass" list; no claim that motion, timing or sound passed from code or a still frame; the developer's yes on the glass recorded (here `verify.py signoff`) — unsure while the review runs before it (mascot-animation Procedure 9–10; the host's side, the gate's records) | 10 |
| R11 | On a flipbook glass: the move's key poses within the host's frame cap, its frames and window reported from `mp.flipbook()` and matched by the build's partial areas, its frames counted toward the host's partial limit, ending on rest, played once — no idle loop, blink cycle or repeat while waiting; each frame's moments collapsed onto the key pose that carries their beat, told as its refresh starts, on one voice played in turn within the frame's hold with the overflow dropped, and the draft's WAV mixed at the frame's pace (mascot-animation Do 15, Do 16; MASCOT §6) | 11 |
