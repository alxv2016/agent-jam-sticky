---
name: mascot-animation
description: Mascot animation for Claude's mascot — a squishy, energetic pixel block in the host's colours or greys — and the 8-bit sound of every movement in its moves — the character (its shape on the pixel grid, a pixel half a cell; eyes, hands, legs, five views, a hand over the body in the tint role, a side turned away in the shade role; the model sheet references/mascot_model_sheet.png, and on e-paper its greyscale copy mascot_model_sheet_eink.png, drawn from and never traced), where it stands (clear of the glass's corners, in front of what the page lays under it; only the walk and the skip leave, by a side, wholly), the motion principles every move follows (anticipation, stretch, eases, squash, elastic settle, overshoot, follow-through, planted feet, props with weight), the sound rules (every movement heard, a MascotSfx moment told once from the drawn pose, music only as a timed phrase), and the animation gate — a contact sheet and draft GIFs at real pace and half speed from scripts/mascot_preview.py, with each beat's sound and its reach, until the developer's yes names the GIF; then built with the numbers unchanged, its moments checked in the simulator and heard as a WAV, and looped on the host's stage for the developer's yes on the device; on an e-paper glass, a few-frame flipbook — a move as its key poses, one partial refresh each, one bit a pixel, its window and cost reported (MASCOT §6). Use when adding or changing a mascot move, pose, prop, expression, view or its sound, choosing the mascot's cell, regenerating the model sheet, reviewing a mascot change, or when the developer says "mascot", "Claude's mascot", "deck animation", "pose", "squash", "bounce", "wobble", "walk", "skip", "contact sheet", "draft gif", "sound effect" or "sfx". The host's widget, colour roles, glass, events and demo page are the host's side (.agents/project.py → MASCOT, or .agents/project/ → mascot).
metadata:
  scope: portable
---

# Mascot animation

How a move of Claude's mascot goes from an idea to the glass and the speaker. The mascot is Claude Code's character,
with its moves and sounds, and no host project's proprietary asset.
[`references/character.md`](references/character.md) (`MASCOT`) owns the character, its motion principles and its sound
rules; [`references/build.md`](references/build.md) the tools, building, simulating and the device. **The host's side** —
its widget and synth files, its mascot cell and headroom on its glass, its colour roles' names, its kind of glass, which
event plays which move, its stage, its simulator's commands, its checks — is the host's (The host's side, below).

## When to use
- A new move for a state or event; a new pose, prop, expression or view; a move retimed or re-routed.
- A movement that is silent, or a sound out of step with the glass.
- Changing the mascot's cell on the host's glass — the developer's choice, made on the device.
- Regenerating the model sheet after a change to the character.
- Reviewing a mascot change (the `mascot-animator` agent's review mode, `.agents/agents/mascot-animator.md`).

## The host's side
What the skill reads of the project it works in — `.agents/project.py` → `MASCOT` where the kit keeps its facts in one
Python file, or `.agents/project/config.json` → `mascot` with `profile.md` → Mascot for the prose; `scripts/host_config.py`
reads either, and `python3 scripts/host_config.py <key>` prints one.

| Key | What it is |
| --- | --- |
| `glass` | `frames` — an LCD drawn every frame at 30 fps — or `flipbook` — e-paper, a move as its key poses (MASCOT §6) |
| `frame_ms` | A flipbook's frame: one partial refresh on the mascot's window |
| `theme_h`, `cell_name`, `cell` | The theme header that holds the `Mascot*` roles (as functions, or as `kColorMascot…` constants with their hex in a comment) and the cell; `cell` where the header does not name it yet |
| `board_h`, `width_define`, `height_define`, `corner_px`, `home_column` | The glass: its size, its corners, the column the mascot stands in |
| `background` | The glass behind the mascot, which its eyes show through — black on the deck, white on e-paper |
| `dither` | A flipbook's greys: each hex → its 4 × 4 pattern in screen coordinates, locked to the glass's pixels |
| `frames_h`, `backdrop` | The widget's frames header; a shot of the page under the mascot — None until the host has them |
| `synth_include`, `synth_source`, `synth_header`, `synth_namespace` | The host's synth or note player, for `render_sounds.sh`; None until `render_sounds.cpp` is ported against it — on a buzzer, the host's player of the deck's note format |

- **`MASCOT_FRAMES_H`** points a draft at a copy of a frames header where the host has none; **`MASCOT_THEME_H`** at a
  header holding draft roles not yet in the host's design system; **`MASCOT_CELL`** drafts at another cell.

## The mental model
- **Drawn in code, never from artwork** — rectangles and pixel poses on LVGL canvases, from the model sheet's
  construction; nothing traced, converted or compiled in from a drawing (MASCOT §1).
- **A pixel is half a cell** — the grid is in mascot pixels (the body 16 × 12); the host's cell sets their size on the
  glass, so a move drawn once draws at any cell; distances in a move's keys are px at the 8 px reference cell, scaled
  (MASCOT §2).
- **A move is data plus a pose function** — keyed tracks (`lift`, `wide`, `tall`, `lean`, `x`) with eases, a pose at
  each time; the draft's `pose(t)` in `mascot_preview.py` is the same shape as the firmware's, so approved numbers go in
  unchanged ([build.md](references/build.md)).
- **Sound is told by the pose** — a `MascotSfx` moment fires once as the drawn pose crosses a threshold, played on the
  synth's effect voices; a phrase is music over a whole move (MASCOT §4).
- **The mascot is decorative; the state is text** — the page names what the mascot acts out, so no fact rests on it
  alone.
- **Haptics and lights belong to the host's cues, not the mascot's movements** — a move never fires either (MASCOT §4).
- **On e-paper a move is a flipbook** — its beats as 2–4 key poses, each held one partial refresh, one bit a pixel with
  the greys as the host's dithers; every frame is a refresh with a cost, so a move is drafted with its frames and its
  window beside its reach (MASCOT §6).

## Rules

### Do
1. **Draft every move as GIFs and build only after the developer's yes names one** — a contact sheet of the key poses
   and GIFs at real pace and half speed from `scripts/mascot_preview.py`, each beat named with its sound, the reach
   reported; a sheet, a judge's pass or the agent's own reading never approves (Procedure).
2. **Build from the approved numbers unchanged** — every key, beat and threshold as the draft had it; a fix that moves
   an approved number is a question for the developer, not a build step.
3. **Give every change of motion its physics** — anticipation, a stretch on the launch, eases into an impact and out
   of a launch, a squash on impact, an elastic settle, overshoot on a lean, follow-through on hands, cloth and lids
   (MASCOT §3).
4. **Keep the feet planted** — squash and stretch about the feet; a tilt about a foot, or the body about the hips with
   the legs upright beneath (MASCOT §3).
5. **Light what is in front, shade what turns away** — a hand over the body in the tint role, a side in the shade role;
   never the body's own colour over the body (MASCOT §2).
6. **Give every movement its sound** — each beat has its moment, told once per crossing from the pose; a new kind of
   movement a new `MascotSfx` (MASCOT §4, §5).
7. **Start and end every move on the rest pose**, and say how it stops part way: what plays out, at what pace, and
   what its sound does (MASCOT §3).
8. **Keep the move on the glass** — nothing cut off at the top, the foot or in a corner (`mp.reach()`'s `on_glass`),
   standing in front of what the page lays under it, which a move may cross; only a travelling move (the walk, the
   skip) leaves by a side, wholly, and comes back from wholly past the other (MASCOT §3).
9. **Paint from the mascot's colour roles** — a new colour is a new role in the host's design system first, and only
   the mascot wears them (MASCOT §2).
10. **Integer maths, deterministic** — per mille, 0.1°, px at the reference cell; no float, no random: a blink and a
    walk's way are hashed off the clock (MASCOT §3).
11. **Ask before any view, prop, expression or kind of sound the character has not had** — show it on a sheet; render
    the sound to a WAV.
12. **Draw from the model sheet, and keep it true** — a change to the character is drawn on the sheet by
    `scripts/model_sheet.py`, approved, and the sheet and MASCOT §2 changed in the same change.
13. **Tag every number** — `[DER]` derived for the host's glass, `[H]` read on the host with a
    date, `[M]` watched, heard or measured on the device with a date, `[?]` not yet settled.
14. **Change the docs in the same change as the code** — MASCOT for the character, its motion or its moments; the
    host's mascot sections for the widget, events and colours (the host's side names them).
15. **On a flipbook glass, draft the glass's own pictures and their cost** — GIFs at `frame_ms`, one bit a pixel;
    `mp.flipbook()`'s frames and window reported with the reach; 2–4 key poses within the host's cap, ending on rest,
    played once (MASCOT §6).
16. **On a flipbook glass, hand over the sounds beat by frame** — each frame's moments, collapsed from the LCD's
    in-betweens onto the key pose that carries their beat, told as its refresh starts; on one voice, played in turn
    within the frame's hold, the overflow dropped; the WAV mixed at the frame's pace beside the GIF (MASCOT §6).

### Never
- **Build, or edit the developer's tree for, a move nobody approved** — a draft that needs new frames or code goes in
  a scratch git worktree.
- **Call a move done before the developer's yes on the device** — seen and heard on the host's stage.
- **A plain move from A to B** — a slide, a pop in or a cut with no wind-up, impact or settle.
- **A silent movement, a moment told every frame, or two for one movement.**
- **A sound on a tap, a press or the knob** — the glass answers a press (MASCOT §4).
- **A haptic pattern or a light fired by a move** — the host's cues own them (MASCOT §4).
- **A part over the body in the body's colour, or an outline to make one read.**
- **Trace, convert or compile in another author's drawing**, the model sheet included (MASCOT §1).
- **Put a host project's own character — its face, its shapes, its routines — on the mascot, or the mascot's colours on
  anything else** — a host's characters may be its proprietary assets (the host's side); the mascot's roles are
  the mascot's alone.
- **Stage a move by editing source** — the host's stage loops it: a demo page in the shipping build, or a stage build
  where the host has no page for it (the host's side).
- **On e-paper, an idle loop, a blink cycle or a repeat while waiting** — each is a refresh and a ghost (MASCOT §6).

## Procedure — the animation gate
Each step stops where the developer's word is needed; "a change" at either yes goes back to the draft.
`MP` is `.agents/skills/mascot-animation/scripts/mascot_preview.py`; `SP` your scratchpad; run from the repo root.

1. **Brief** — the state or event, what the move says, its length, loop or once, how it stops part way, a travelling
   move's route; the beats — wind-up, action, impact, settle, rest — each with its sound: an existing moment, a new
   `MascotSfx`, or a phrase if it is music.
2. **Copy the worked example** — `cp .agents/skills/mascot-animation/scripts/examples/finish.py SP/<move>.py`; it shows
   `mp.front()`, `mp.Prop`, `mp.Pose`, `mp.track()` and a `clock()`; the tool also has `mp.work_pose(n)` (the work
   loop's three-quarter poses, from the frames) and `mp.blink_at()` (the blink every move has).
3. **Write the tracks** — `(t, lift, wide, tall, lean, x, ease)` keys, distances in px at the 8 px reference cell; start
   from MASCOT §3's ranges.
4. **Render at the cell** — `python3 -B SP/<move>.py SP/<move>` draws at the host's cell; `MASCOT_CELL=<cell>` draws at
   another, one run per cell, to compare sizes; the frames are the firmware's ([build.md](references/build.md) → The
   tools). `mp.sheet()`, `mp.gif()` at speed 1 and 0.5 (each decoded), `mp.reach()`, and on a flipbook
   `mp.flipbook()`. Read the sheet first: every beat there, feet on the ground, hands readable, `on_glass` true; on a
   flipbook, each key pose readable at one bit.
5. **A new sound** — rendered to a WAV in the scratchpad ([build.md](references/build.md) → A new sound).
6. **Hand over and stop** — the GIFs, sheet and any WAV by absolute path; each beat in one line with its sound; the reach
   (`top`, `bottom`, `left`, `right`, `corner`, with times), and on a flipbook its frames and window and each frame's
   moments, the WAV at the frame's pace beside the GIF; the choices made
   (size, colours, length, sounds, route, how it stops). **Stop for the yes that names the GIF**, and record it as the
   host's harness records an approval — here `verify.py approve <gif> --kind motion`.
7. **Build from the approved numbers unchanged** — [build.md](references/build.md) → Build; docs in the same change; run
   the Checks.
8. **Simulate and hear it** — a scenario that plays the move through the host's stage, its frames against the draft beat
   for beat, its moments tallied against the sheet and mixed to a WAV ([build.md](references/build.md) → Simulate).
9. **Build, flash and loop it on the host's stage** — tell the developer what is looping and what to judge: the pace, the
   impacts, the reach, every movement's sound in step on the speaker.
10. **Their yes on the device approves the move**; a change goes back to step 1.

## Checks
- [ ] (script) A draft's GIFs decode — `python3 .agents/skills/mascot-animation/scripts/mascot_preview.py check <gif>` prints `N frames decoded OK`.
- [ ] (script) The worked example renders — `MASCOT_CELL=<cell> python3 -B .agents/skills/mascot-animation/scripts/examples/finish.py $TMPDIR/finish` prints its reach; the lines it printed at the host's cells are the host's side's — a change that moves either is a change to the finish or the tool. Per host: one with no frames header or roles yet runs it with `MASCOT_FRAMES_H` and `MASCOT_THEME_H`.
- [ ] On a flipbook glass, the draft's hand-over names `mp.flipbook()`'s frames and window, the frames within the host's cap, ending on rest (Do 15), and each frame's moments, with the WAV at the frame's pace (Do 16).
- [ ] The developer's yes named the GIF this build follows before any repo edit, and the build's numbers equal the draft's (Do 1, Do 2).
- [ ] Each beat of the approved sheet has its moment, at its time; no moment on consecutive frames (Do 6).
- [ ] The mascot whole in every shot: nothing cut off at the top, the foot or in a corner; only a travelling move off a side, wholly (Do 8).
- [ ] Every change of motion has its wind-up, impact and settle; feet planted; hands over the body tinted (Do 3–5).
- [ ] The developer's yes on the device, on the host's stage.
- [ ] No picture of a host's proprietary character tracked — only the model sheet when it changed.
- [ ] More for a built move: [build.md](references/build.md) → Checks.

## Traps
- **A draft stands the mascot on a bare glass, not the host's page** → the host's backdrop shot is not one the
  simulator saved, or the host has none yet → pass `backdrop=` a shot of the host's page to draft over what lies under the mascot, `body_xy` for
  another place.
- **`reach()` says cut off but the sheet looks whole** → the draw falls in a corner arc, written but never seen → read
  `corner`: its pixel and time.
- **The draft shows an old value after a one-character edit** → Python reused cached bytecode → `python3 -B`.
- **A jolt where one track hands to the next** → the next track's first key differs from the last one's end → start
  each track on the values the one before ends with.
- **The GIF plays too fast in one viewer** → delays under 2 cs are clamped → `mp.gif()` writes 3, 3, 4 cs; keep it.
- **A big cell overflows the glass** → the moves are tuned for the host's cell on its glass (the host's side) →
  the cell is the developer's choice, made on the device, never by scaling until it fills.
- **On e-paper the eyes vanish** → the host's `background` is not the glass's white → set it; the eyes are holes to it.
- Build, simulator and device traps: [build.md](references/build.md) → Traps.
