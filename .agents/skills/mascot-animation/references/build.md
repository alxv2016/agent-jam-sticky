# The tools, building, simulating and the device

What the [mascot-animation](../SKILL.md) Procedure's steps 4–10 run. The tools read the host project's files through
the host's side (`scripts/host_config.py`: `.agents/project.py` → `MASCOT`, or `.agents/project/config.json` →
`mascot`). Where the widget, the synth, the speaker and the demo page live, and the host's own checks, are the host's
side too (SKILL.md → The host's side).

## The tools

Each tool below does the job its row names, and each takes the host's side from `host_config.py`.

| Tool | What it does |
| --- | --- |
| `scripts/host_config.py` | The host's side, for every tool here: `MASCOT` in `.agents/project.py`, else `mascot` in `.agents/project/config.json`; `host_config.py <key>` prints one |
| `scripts/mascot_preview.py` | A draft's `sheet()`, `gif()` (real pace and half speed, every frame decoded), `reach()` (rows, columns and the first corner pixel, with times) on the host's glass; on a flipbook `one_bit()` (the glass's partial refresh, the host's dithers) and `flipbook()` (the frames, the window, the pixels each frame flips — MASCOT §6); `check <gif>` decodes a GIF |
| `scripts/examples/finish.py` | The approved finish as a worked draft to copy |
| `scripts/model_sheet.py` | Draws the model sheet from the mascot's frames and the host's colour roles: [`mascot_model_sheet.png`](mascot_model_sheet.png) on an LCD host, [`mascot_model_sheet_eink.png`](mascot_model_sheet_eink.png) on a flipbook host (`glass` `flipbook`) — `python3 .agents/skills/mascot-animation/scripts/model_sheet.py .agents/skills/mascot-animation/references/mascot_model_sheet_eink.png` |
| `scripts/render_sounds.{cpp,sh}` | Every sound id to a WAV (`render_sounds.sh all <dir>`), and a scenario's moments mixed as the device mixes them (`render_sounds.sh track <events.log> <wav>`), from the host's synth at its rate |

- **The colours** come from the host's theme header, its mascot roles (`mp.role("MascotBody")`); `mp.step()` gives
  neutral steps only — the host's primary colour need not be the mascot's orange.
- **The glass** comes from the host's board config (its width and height) and its corner radius; `mp.in_corner(x, y)`
  says whether a pixel is lost to a corner.
- **The cell** is the host's mascot cell from its theme header; `MASCOT_CELL` overrides it to draft at another size.
- **The frames** are the firmware's frames header; in a tree without them, `MASCOT_FRAMES_H` points at a copy of one.
  Only a draft using `mp.work_pose()` or the flag reads them.
- **The backdrop** is the host's bare glass (`background`) with the mascot centred, or `backdrop=` a shot of the host's
  page under it.
- **The glass** on a flipbook: frames at `frame_ms`, one bit a pixel, the greys as `dither`'s patterns.
- Each path and value: the host's side.

## A new sound

In the scratchpad: copy the host's synth and `scripts/render_sounds.cpp` there; add the sound id, its notes and its row
in the copy, its name in the copy's names. Build and run the copy (`render_sounds.sh`'s compile line, pointed at the
copies) with `all <dir>`; give the developer the WAV beside the GIF.

## Build (after the yes)

The files each step edits are the host's side's (its Build).

1. **Frames** for a pixel move in the frames header, a prop's outline as its own table; **keys** in the widget's source —
   a `struct` of the draft's columns, px at the reference cell, the approved numbers unchanged.
2. **The move** — a move value appended to the widget's move list, its period, when a stopped move has finished, its time
   and pose functions, its branch in the tick and its draw. A move that plays once keeps its value at rest, so asking
   for it again does not replay it.
3. **Its moments** — a front-view move tells from the pose's fields; a keyed beat no field shows (a prop set down, a lid)
   gets a row like the work's `kWorkMoments`. A new kind
   of movement gets a new `MascotSfx` named everywhere
   ([character.md](character.md) → MASCOT §4, rule 10). A phrase only for music, its notes from the keys' times.
4. **Canvases** — each moving part on its own small canvas; a travelling move moves its parts, never the root, so each
   redraws only its own box; each tilted canvas no bigger than the work's (`render-perf-guardrails`).
5. **Its length** as a constant in the host's theme, with a `static_assert` that the tables add up; a colour role per new
   colour, in the host's design system first.
6. **Wire it** — its place on the host's stage, so the move can be looped alone; the host's event that plays it.
7. **Docs in the same change** — the host's mascot sections (the host's side names them).

### On a flipbook glass

- **Frames are the key poses** — a move's table is its 2–4 approved poses and the refresh each is held for, no
  in-betweens; the widget draws one pose per refresh, never on LVGL's frame clock.
- **One partial window a move** — the union box of the pixels its frames change (`flipbook()`'s `window`), asked of the
  host's display as one partial area per frame, so the rest of the glass is not driven.
- **Every frame is counted** — each is a partial toward the host's limit before a full refresh, and a move's frames
  are counted with the page's own; a move never forces a full refresh mid-move — the host takes it after rest.
- **The host's stage is its demo page** — every move loops one at a time on a page of the host's test or demo feature,
  in the host's own view of its glass (its width and height, its `home_column` or the glass's centre — never another
  host's layout). A press steps it, with its sound on the host's buzzer or speaker.
- **Sounds at the frame's pace** — each `MascotSfx` moment fires on the key pose that carries its beat, as that frame's
  refresh starts (plus the host's measured offset to the ink, `[?]` until benched).
  On one voice a frame's moments
  queue within its hold and the overflow is dropped (MASCOT §6). `render_sounds` is ported against the host's player —
  on the Sticky, `components/devices/include/sound_rules.h`, the deck's note format on the buzzer. It mixes a draft's
  moments at the frame's pace into a WAV handed over beside the flipbook GIF.
  Until it is ported the host's
  `synth_include` and `synth_source` stay None and `render_sounds.sh` says so.

## Simulate

- **A scenario films the move through the host's stage** — open the stage on the move, then record over the move
  or take a shot at each approved beat. Another move asked for part way for a stop, the move before for the
  hand-over.
- **Its sounds are asserted and tallied** — the simulator's stand-in for the speaker writes each moment to the
  scenario's event log as `sfx <name>`.
  A scenario asserts a beat's moment after a mark, and the log is tallied against
  the sheet's beats and mixed with `render_sounds.sh track`; a beat with no moment, or one every frame, goes back to
  Build step 3. The host's scenario commands: the host's side.
- **Read every shot against the draft** — the same pose at each beat, the mascot whole; a difference is the build's
  fault, or a change that goes back to the draft.

## The device

- **The host's stage loops each move, with its sounds** — a demo page in the shipping build, or a stage build where
  the host has none; never a source edit (the host's side).
- **Flash for the verify step**, open the page, and tell the developer what to judge: the pace, the impacts, the reach on
  the glass, every movement's sound in step on the speaker. On e-paper, judge each key pose's read at one bit and the ghost
  each move leaves. Their yes there is the move's second yes.

## Checks

- [ ] Every move is filmed — each move on the host's stage but Rest is played through it in a scenario.
- [ ] No float in the mascot's source.
- [ ] No deck scenario is silent — every scenario that films a move logs a moment.
- [ ] The sound names agree — `render_sounds.sh all <dir>` builds (its `static_assert`) and writes a WAV per sound id.
- [ ] The model sheet is the script's — `model_sheet.py` regenerates the host's sheet byte for byte: `mascot_model_sheet.png`
  on an LCD host, `mascot_model_sheet_eink.png` on a flipbook host.
- The commands for this host: the host's side → Checks.

## Traps

- **A moment fires every frame through a beat** → two tests share one flag and undo each other → one flag per moment,
  fed the deeper of the values; the tally shows dozens in a second.
- **A landing sounds twice** → its squash crossed the crouch's threshold on the landing frame → a landing marks the body
  crouched, so its squash is the landing's sound.
- **The simulator logs a moment the device drops** → more than the effect voices at once; the oldest is cut → keep a
  beat to one or two moments; spread the rest a frame apart.
- **A phrase drifts off the glass after a timing change** → its notes are the old keys' times → make it again from the
  new keys, or turn its beats into moments.
- **A sound stutters on the device** → the speaker task starved by the UI task → the speaker task runs above the UI
  task's priority, pinned to a core; keep a synth change cheap per sample.
- **A tilted move stutters** → its layer is a whole canvas at 4 bytes a pixel, every frame → split what tilts into
  smaller canvases.
- **A walk or a skip drags the whole glass's frame rate down** → it moved the root, and LVGL redraws its box → move the
  parts.
- **A move replays on every record** → it ended by turning into rest, and the screen asks for it again → a move that
  plays once keeps its value and draws its rest.
- **A blink lands on a shut or happy eye** → the blink only shuts open eyes → draw its lower row only when the pose's
  eyes are open.
- **The flag's checks smear at its edge** → a check bigger than the pixel cuts the outline into slivers → by design at a
  pixel and a half.
