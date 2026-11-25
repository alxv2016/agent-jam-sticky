# Mascot

Claude's mascot — the character, its motion and its sounds.

Who Claude's mascot is, how every move of it moves, and how every movement is heard. It holds the reference, the character and
its views, the motion principles with their ranges, the sound rules and the moments. Cited as `MASCOT §n`. The picture
is the model sheet, [`mascot_model_sheet.png`](mascot_model_sheet.png) — open it before drawing anything. For a
greyscale glass, use its e-paper copy [`mascot_model_sheet_eink.png`](mascot_model_sheet_eink.png): the same views in the
panel's greys and at one bit a pixel, as a partial refresh draws them (§6). The mascot is
Claude Code's character, with its moves and sounds, and is not any host project's proprietary asset. What a host project does with it is in that project's docs, named by the host's side. That covers its cell and place on
its glass, its colour roles, which event plays which move, its speaker and its widget's files. The host's side is
`.agents/project.py` → `MASCOT`, or
`.agents/project/config.json` → `mascot` and `profile.md` → Mascot, read by `scripts/host_config.py` (SKILL.md → The
host's side).

| Tag | Meaning |
| --- | --- |
| `[DER]` | Derived for the host's glass |
| `[H]` | Read on the host, with a date |
| `[?]` | Not yet seen on the host's glass — on e-paper, every flipbook value until a move has run there |

| § | Section | Look here for |
| --- | --- | --- |
| 1 | [The reference](#1-the-reference) | The model sheet, drawing never tracing, measures in mascot pixels |
| 2 | [The character](#2-the-character) | Who, nature, shape, eyes, hands, legs, palette by role, the five views, the drawing grid |
| 3 | [Motion principles](#3-motion-principles) | Anticipation to energy, with their ranges; rest pose, headroom, travelling moves, integer maths |
| 4 | [Sound rules](#4-sound-rules) | Every movement heard; a moment or a phrase; told from the pose, once; thresholds in cells; 8-bit and quiet |
| 5 | [The moments](#5-the-moments) | Each `MascotSfx`, when it is told, how it sounds |
| 6 | [On a slow glass — the flipbook](#6-on-a-slow-glass--the-flipbook) | E-paper: a move as its key poses, one refresh each, one bit a pixel, its window, its cost, one voice |

---

## 1. The reference

**The mascot's reference is its model sheet, its character (§2), its motion principles (§3) and the moves already
built** — a new move is drawn from them, drafted and approved.

- **The model sheet sets out on the mascot-pixel grid what §2 says in words** — the front, three-quarter, side and two
  back views with their construction lines and measures, the eyes in each expression, the hands and the legs in each of
  their positions, and the palette by role. The text of §2 is enough to draw a move from, and the sheet is its picture.
- **Draw from the sheet; never trace it** — the mascot is redrawn in its own pixels and rectangles, never traced,
  converted or compiled in from another author's drawing, the sheet included.
- **The sheet is drawn from the mascot's own frames and the host's colour roles** by `scripts/model_sheet.py`, so it
  shows what the firmware draws; it is committed with this skill for open use.
- **Its measures are in mascot pixels, which do not change with the cell**; the sheet's heading "8 px on the glass" is
  a pixel at a 16 px cell — a mascot pixel is half the cell.
- **The moves built from Claude's own animations are redrawn from contact sheets of them**, with published SVGs of
  the mascot and an article explaining them for proportions and timing. None of them is kept, and nothing from them is
  compiled in.

## 2. The character

**Who the mascot is, so a new move can be drawn without a reference sheet.** The model sheet is the picture of this
section.

| Trait | The character |
| --- | --- |
| Who | Claude's mascot, standing for an agent: the session in the body role (Claude orange on a colour glass, black on e-paper), a sub-agent in its own body role, the faded roles in a previous chat |
| Nature | Squishy and energetic — a soft block that squashes when it lands, stretches when it springs, wobbles when it stops, and does nothing by halves: it hops for attention, types furiously, waves the flag with its whole body — and every bit of it is heard |
| Shape | A block: the body 16 × 12 pixels, two 4 × 4 hands half behind its sides, two 2 × 2 eyes, four 2 × 4 legs; no mouth, no neck, no fingers — a pose is told by the body, the hands, the legs and the eyes |
| Eyes | Its whole face: open (2 × 2); shut to the lower row (a blink, a squint); one shut (a wink); `^ ^` (happy — 4 wide, 2 tall, the top row inset a pixel); `> <` (squeezed shut — 4 wide, 3 tall, 2-pixel arms pointing in toward each other); dizzy (a 4 × 4 square spiral round each eye, both turning the same way) |
| Hands | Rest half behind the body, 2 pixels out; raised, a hand stands straight up over its side, the same 4 columns; one carrying something holds it in its top row |
| Legs | Four, standing front, back, front, back — the second and the fourth behind, inset, so from the front all four show in a row — straight under the body, feet on the ground; a raised leg is a pixel shorter and bends at the knee; in a squat shorter and straight; tucked in a jump, shorter with the knees out; in the rig's hop the feet swing out, curving the legs |
| Palette, by role | The body, hands and legs in the body role; the eyes the host's background, holes in it; a side turned away in the shade role; a hand in front of the body in the tint role; props in their own roles (a laptop, a flag with its check and pole, dust and faint dust, sweat, confetti); a previous chat's mascot in faded body, shade and tint; a sub-agent in its own body, shade and tint by index. The host's names for them, and their colours or greys: the host's side |
| Voice | 8-bit, on a synth's effect voices or the host's one buzzer: a pip a blink, a twinkle `^ ^`, a "bwip" off the ground, a tick back on it, a "hmp" into a squat, a breath of noise a lean, a chirp a hand up, a "fwap" a hand down, a whoosh a turn, a pop a bead of sweat (§5) |

**Views.** Five, all on the model sheet; mirrored, a view faces the other way — a travelling move faces the way it goes.
A move in a new view is approved as a move is.

| View | How it reads | Hands |
| --- | --- | --- |
| Front | The rest pose: the body square to the glass, both eyes, four legs | Both half behind the body, 2 pixels out; a raised one straight up over its side |
| Three-quarter, about 25° to its left (the work) | The front 12 pixels wide and a side 4 pixels wide on the glass's right in the shade role; the eyes closer together on the narrower front; the legs evenly 2 pixels apart from a pixel in from the front's edge, the back ones — the second and the fourth — in the shade role, the last a pixel inside the side | The far (left) hand behind it, or showing over what it works at; the near (right) hand out in front of the body in the tint role, its shadow in the shade role on its right — a hand over the body in the body's own colour would vanish into it |
| Side, 90° to its left | Facing the glass's left as the three-quarter does: the side face 10 pixels deep, lit as it turns to the viewer; one eye a pixel in from the leading edge; two legs, the front pair and the back pair, the far legs behind them | The near hand over the body in the tint role with its shadow behind it, the far one hidden |
| Back three-quarter, about 135° to its left (the spin) | The back 12 pixels wide, no eyes, and the side 4 pixels wide on the glass's left in the shade role; four legs, the back face's in the body, the side's in shade | The near hand on the back's right, wholly in the tint role |
| Back, 180° (the spin) | The body 16 pixels wide, no eyes, four legs | Both half in front of the body now, each wholly in the tint role |

- **A part in front of the body is lit, a side turned away is shaded** — a step up or down the body's hue, so the parts
  read as solid without an outline. Never the body's own colour over the body.

**The drawing grid** — `mascot_preview.py`'s, which its docstring owns; condensed here for drawing:

| Part | On the grid |
| --- | --- |
| Body | 16 × 12 pixels at (17, 10)–(32, 21); centre line column 25; hips row 22; ground row 26 |
| Eyes | 2 × 2 at (19, 12) and (29, 12); blink = the lower row; wink = one shut; happy `^ ^` = (ex, 12, 2 × 1) plus (ex − 1, 13) and (ex + 2, 13); chevron `> <` = 4 × 3, 2-pixel arms pointing in; dizzy = a 4 × 4 square spiral round each eye (`mp.front(eyes=…)` draws each) |
| Hands | 4 × 4, half behind the body: (15, 14) and (31, 14); raised straight up = (31, 7), cocked = (31, 15) |
| Legs | 2 × 4 at columns 17, 21, 27, 31, rows 22–25; raised = a pixel shorter, knee out half a pixel; squat = shorter, straight; tucked = shorter, knees out; the hop = feet swung out (`swing`); walking, the raised knees bent toward where it goes (`knee_dir`) |
| Three-quarter | Front 12 wide plus a 4-wide side on the glass's right; the legs at columns 18, 22, 26, 30, the back ones (22, 30) shaded |
| Side | The side face 10 deep; one eye a pixel in from the leading edge; two legs |

## 3. Motion principles

**§3's ranges are an LCD's, read at 30 fps; on a flipbook glass they shape each key pose, not its timing (§6).**

**Every move is the squishy character in motion — movement, momentum, impact and physics in every change of motion,
never a plain move from A to B.** The jump, the work and the finish are the worked examples. Distances are in mascot
cells, two mascot pixels, so they hold at any cell; the ranges below come from the moves at a 16 px cell `[DER]`.

| Principle | What the mascot does | Its range in the moves built |
| --- | --- | --- |
| Anticipation | A wind-up before every launch: a squashed crouch held a beat, a lean away from where it is going | Wide 1030–1110 ‰, tall 870–975 ‰, held up to 100 ms |
| Stretch on a launch | Springs up taller and narrower, the trailing parts dragging | Wide 900–965 ‰, tall 1050–1150 ‰ |
| Momentum | Eases in toward an impact, faster and faster, and out of a launch | A cubic ease-in on the fall or swing to the touchdown, ease-out into the squash and out of a launch — a key's ease takes the segment into it |
| Squash on impact | Every landing, stomp, keystroke or set-down: wider and shorter about the feet | Wide 1050–1150 ‰, tall 830–940 ‰ |
| Elastic settle | After an impact, a wobble that dies: tall, squat, tall, settled | About 950/1070, 1040/965, 985/1015 ‰; 35–60 ms a swing for the pixel moves, 100 ms for the rig's jump |
| Overshoot | A lean, a hop across or a lid swings past where it stops and rocks back | Leans ±2.5–5.5°, the overshoot about a third past |
| Follow-through | The hands, the cloth, a lid lag the body and catch up | A beat behind |
| Planted feet | Squash and stretch scale about the feet; a tilt leaves the feet on the ground — rotating about a foot, or the legs upright under tilting hips | — |
| Squat | The body drops with the feet planted; the legs get shorter and stay straight — never a half-pixel notch, which reads as a glitch | The celebrate's, about 1 cell |
| Weight shift | The legs it leans away from rise and bend; it stomps down on them as it swings back | A pixel shorter, the knee half a pixel out |
| Props have weight | A thing set down bounces; a thing carried is rigid and rides the hand | The laptop's bounce, about 0.3–1 cell |
| Energy | Big, quick, committed — a pace from the reference's 30 fps sheets, a fast beat 67 ms a frame | Hops about 0.7–1.2 cells in the pixel moves, the celebrate's big jump and the rig's 2 cells; leans to 5.5° |

- **Every move starts and ends on the rest pose**, and hands over to the next without a jump.
- **A move stays inside the mascot's headroom** — kept clear of the glass's rounded corners, in front of whatever the
  page lays under it, which a move may cross. The headroom and the corners on the host's glass: the host's side.
- **Only a travelling move — the walk, the skip — leaves the glass**, by a side, wholly, and comes back in from wholly
  past the other, never on both edges at once. Its route and length are derived for the host's glass and its centre.
- **Integer maths, deterministic** — a move's keys in per mille, tenths of a degree and px at a reference cell (8 px;
  the rig's jump 6 px), scaled to the cell it is drawn at.
  No float and no random: a blink and a walk's way are hashed
  off the clock, so a move draws the same every time and a draft matches the build.
- **A move fits LVGL's memory**: a tilted layer no bigger than the work's, in external RAM — its size grows with the
  cell's square.
- **A move is approved twice**: from its draft pictures before it is built, and on the device before it is done, looped
  alone with its sounds.

## 4. Sound rules

**Every movement is heard** — a hand gesture, the eyes, an impact, a jump, a hop, a step, a sway, a lean, a turn, a
crouch, a prop set down, opened or shut, the sweat's beads and their pop. A move with a silent beat is not finished, and
a draft or a review that does not name each beat's sound is not done. A sound reaches the speaker one of two ways;
reach for the first.

| Kind | What it is | How |
| --- | --- | --- |
| A moment | One movement in a move: most sounds | The mascot tells a `MascotSfx` as the frame that shows it is drawn; the host plays its effect sound on one of a few effect voices, mixed over whatever plays |
| A phrase | Music over a whole move: a fanfare, a "hi" a swing, a thinking "boop boop boop" | A sound whose notes are the move's key times, started as the move starts and caught up to where it has got |

1. **Tell a moment from the pose, not the clock** — a threshold the drawn pose crosses, so a change to the keys moves the
   sound with it. A phrase must be made again whenever its move's keys change — prefer a moment.
2. **Tell each once per crossing** — a state flag set when it crosses and cleared when it is back; never a test true on
   every frame of the beat.
3. **One movement, one moment** — a landing's squash is the landing's sound; a hand flap is up then down; two moments on
   one frame for one movement is a double.
4. **The thresholds are in cells**, so they hold at any cell: off the ground past half a cell (a walk's bob is a step),
   back within a quarter; a crouch past a quarter of a cell.
   A sway at a quarter of a cell or a lean of 4.0°; a hand up
   when its top crosses row 11; a step when the raised legs change; a turn at each new view.
5. **Sound the size of the motion, in its direction, short** — pitch slides up for what rises and down for what falls,
   noise for contact and air, the pulse and the triangle for the face.
   A big impact lower and longer, a small one a
   tick; an effect under about 120 ms, so three voices carry a busy move (every effect 10–120 ms `[H]` 2026-11-15).
6. **8-bit only, and quiet** — a small synth's NES-style voices (pulse at 50, 25 and 12.5 % duty, a stepped triangle,
   noise, pitch slides, a short attack and a decay) at low levels. Never a coin — a short note leaping up to a held one.
7. **A press is never heard** — a tap, a button or the knob plays no sound; the glass answers it.
8. **A mascot rebuilt part way learns where it is from its first frame and tells nothing on it** — a screen rebuilt on
   new data does not hear a hop twice; avatars of the same mascot share one blink.
9. **The keyed beats are the exceptions** — a prop's set-down, a lid, a stretch, a wipe's flick and sigh are set at
   their keys' times, so they move only when edited with the keys.
10. **Name a new moment in every place the host names it, in the same order, appended** — the widget's `MascotSfx`, the
    synth's sound id and its notes, the speaker's check that the effect run matches `MascotSfx`, the simulator's
    stand-in names, `render_sounds.cpp`'s names. The host's files: the host's side.
11. **Haptics and LEDs are not a moment's** — a mascot movement is heard only; the host's cues own any motor and lights.

## 5. The moments

Each moment in this table is told at the beat its row names, with the sound in its last column.

| Moment (`MascotSfx`) | Told when | Sound |
| --- | --- | --- |
| `kBlink`, `kHappy`, `kChevron`, `kWink`, `kLook` | The eyes: a blink, `^ ^`, `> <`, the work's wink as it opens (after a spin too), a glance — the eyes leaving a look held three ticks, still frames counted, so a dart's in-betweens held under three ticks pass untold and a glance after a still beat — the look-around's first dart, the skip's wind-up look — is heard | A tiny pip; a two-note twinkle; a squish sliding down; a tink and a higher one; a faint tick |
| `kCrouch` | The body drops past a quarter of a cell into a squat — a wind-up; a landing's squash is the landing's | A low "hmp" sliding down |
| `kHop`, `kLand` | The body rises past half a cell — a walk's bob is a step — and comes back within a quarter of one, or squashes into the ground below the hop's height — a squat springing to a hop passes poses both lifted and squatting, and tells only the hop; a key that touches the ground for an instant may fall between frames | A little "bwip" sliding up; a soft tick |
| `kStep` | A leg lifts: one side raised where it was not | A soft tick of noise |
| `kSway` | A sway reaches a quarter of a cell, or a lean 4.0°; and a rock back through the middle from one, as a walk or a skip stops | A breath of noise |
| `kTurn` | A turn reaches a new view | A quick whoosh |
| `kHand`, `kHandDown` | A hand's top reaches row 11 of the pixel grid, or comes back down: a gesture, each flap — one a frame, so two hands flapping together are one flap | A chirp; a "fwap" |
| `kLaptopDown`, `kLidOpen`, `kLidShut`, `kStretch` | The work's keyed beats (`kWorkMoments`) | A dull thud; a click rising; a clap; a soft rising "aah" |
| `kFlag`, `kBurst`, `kPuff` | The finish's flag up; confetti bursting; a puff of dust | A "fwip"; a sparkle; a soft burst of noise |
| `kBead`, `kPop`, `kWipe` | The sweat: each bead as it appears, their pop, the brow wiped; the flick off its side and the sigh tell `kHandDown` and `kCrouch` | A plip; a bubble's pop; a swish |
| `kTopple`, `kFlop` | The faceplant: tipping over forward; its face landing on the laptop's base. Its other beats tell the moments above at its draft's times (`kFaceplantMoments`), and the skip trip tells its own from the catch (`kTripMoments`) | A thin pulse falling 640 to 160 Hz; a burst of noise, then a low thump sinking 140 to 70 Hz, the longest impact |

## 6. On a slow glass — the flipbook

**On e-paper (the host's `glass` is `flipbook`) a move is its beats drawn as a few key poses, each held one partial
refresh** — the character, its views and its sounds are §2–§5's. What changes is how many pictures a move gets, how each
is drawn and what each costs. The values — the frame's time, the cap, the dithers — are the host's side; none is yet
seen on a glass `[?]`.

| Rule | What it means |
| --- | --- |
| Key poses, no in-betweens | A move is 2–4 frames (the host may cap it lower) — its beats, nothing between them; each held one refresh, `frame_ms` |
| The physics in the pose | §3 is carried by which pose a frame shows: the wind-up is the crouch frame, the impact the squash frame, the settle at most one rebound frame, then rest — a frame never spent on a halfway |
| One bit a pixel | A partial refresh draws black and white only; a grey role is the host's dither (`dither`), so a part reads by its pattern against the body's black and the background's white; a grey the host has no dither for falls to black or white — never rely on it |
| A small window | The host refreshes the box of every pixel a frame changes (`mp.flipbook()`'s `window`); the smaller it is, the faster and the less it ghosts — keep a move about the body, and a travelling move is rarely worth its cost |
| Every frame counts | Each frame is a partial refresh toward the host's limit before a full one; a move's frames, and how often its event fires, are budgeted against it |
| Ends on rest | Every move ends on the rest pose (or the state's still pose), so what stays on the glass is a designed frame — it stays there while the device sleeps |
| Once, never idle | A move plays once on its event; no idle loop, blink cycle or repeat while something waits — each would be a refresh and a ghost |
| One beat a frame | The moments an LCD told on in-between frames collapse onto the key pose that carries their beat; each is told once, as that frame's refresh starts — the ink moves during the refresh |
| One voice | A host with one voice (a buzzer) plays a frame's moments one after another within its hold, `frame_ms`, never mixed: effects are 10–120 ms (§4 rule 5), so about three fit a 460 ms frame; the rest are dropped, never carried into the next frame |
| Sound to ink | The ink changes partway through a partial refresh's drive, so a moment may wait an offset after the refresh starts to land with it; that offset is the host's bench measurement `[?]` until measured, then tuned by ear on the host's demo page |
| A press is the host's | A press's tick is the host's cue, never the mascot's (§4 rule 7) |
| Heard at its pace | A flipbook draft's WAV is mixed at the frame's pace — each frame's moments from its start — and handed over beside its GIF, beat by frame |
| The cheapest form | Where a refresh is too dear — on battery, a page mid-update — the host may draw the move's end pose alone, its moment with it |

- **A draft shows the glass's own pictures** — `gif()` writes a frame every `frame_ms`, one bit a pixel, and `sheet()` its
  stills the same way. The `flipbook()` call reports the move's frames, its window and the pixels each frame flips, which the
  hand-over names beside the reach.
- **The model sheet is drawn for the glass** — the sheet's own paper and lines as on an LCD's, in grey.
  The views in
  the panel's true greys as a greyscale full refresh draws them, on the host's background where a panel stands for the glass, a one-bit row as every partial draws them, each role's swatch both ways.

