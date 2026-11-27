# Agent Jam Sticky — Design Tokens

How the firmware implements [DESIGN.md](../DESIGN.md) on the reTerminal Sticky's e-paper glass. It covers the greys at the
panel's two bit depths and the proof that every colour pair holds on both refreshes. It covers the fonts and how they are
compiled, the token header and its names, the refresh policy and feedback in code, and the checks that keep code and
design in step. For anyone building or reviewing a screen or a component on the device, and for the agents that do.

**Source of truth.** The design — every value, rule and reason — is [DESIGN.md](../DESIGN.md), cited by heading path
(DESIGN → Colors → Roles). Its front matter holds the values, and this doc's tables restate them for the design check.
The panel, its refresh modes and their costs are [STICKY_HARDWARE_SPECS](STICKY_HARDWARE_SPECS.md), cited `[HW]`. The
widgets' sizes come from the agent deck's UI kit, cited `[DECK]` (§9). The
symbols are the firmware's token header (§6). Where this doc and the code disagree, find which is intended and fix the
other. How a screen looks on the glass beats both.

**Related.** [STICKY_HARDWARE_SPECS](STICKY_HARDWARE_SPECS.md) — the panel (§2, §4.5), sleep (§6), the buzzer (§4.2).
Steps — bring-up, display work, the pin check: the `sticky-hardware` skill
([.agents/skills/sticky-hardware/SKILL.md](../.agents/skills/sticky-hardware/SKILL.md)).

**Stability.** Code cites this doc as `STICKY_DESIGN_TOKENS §n` for how the code does something, and DESIGN.md by
heading path for why. Do not renumber; add new material under the next free number.

| Tag | Meaning |
| --- | --- |
| `[HW]` | From STICKY_HARDWARE_SPECS |
| `[DECK]` | Read from the agent deck's widget source (a separate repository, §9) by the author on 2026-11-20 — an author's measure that a reader of this repo cannot open; a number is the deck's px, ported by its ratio |
| `[D]` | Derived: calculated from the cited values |
| `[SEEN]` | Seen on the glass by the person, on a V1.0 or V2.0 board running this repo's firmware |
| `[HOST]` | Measured on the computer, not the glass: LVGL's software renderer drawing the real widgets in the real Bitcount fonts at 480 × 800, thresholded at half, as ink to ink of sample words (a descender counts), on 2026-11-21 over 463f7f5 with the working tree, one run |
| `[?]` | Unverified — tracked in §8 |

| § | Section | Look here for |
| --- | --- | --- |
| 1 | [Rules](#1-rules) | The code's side of DESIGN.md's rules: which value, which refresh, which check |
| 2 | [Colour](#2-colour) | The levels in the framebuffer; each role's kind and its value on both refreshes; the contrast proof |
| 3 | [Type](#3-type) | Bitcount Single at one bit, the scale's primitives, the roles, line boxes, glyph ranges |
| 4 | [Space, strokes and shapes](#4-space-strokes-and-shapes) | The scales as code values; the dither; the hard offset and its elevation levels |
| 5 | [Refresh and feedback](#5-refresh-and-feedback) | The refresh policy, the partial counter and its typing-only mode, the rate limits, the buzzer tick |
| 6 | [The token header](#6-the-token-header) | Where the header lives, how a token's name becomes a symbol |
| 7 | [Enforcement](#7-enforcement) | The design check on this doc, Google's linter on DESIGN.md, what is checked by hand |
| 8 | [Open items](#8-open-items) | What is still unverified, and what settles it |
| 9 | [The ring outside and the ported widgets](#9-the-ring-outside-and-the-ported-widgets) | What the agent deck is and where its source lives; the one focus ring and the editing ring, a segment's ring in its track, every ported widget's sizes against the deck's |
| 10 | [Paging and the swipe](#10-paging-and-the-swipe) | How a list view, a text view and a horizontal pager turn a page: the pages, the swipe's thresholds and window, the lift, the ends and who owns a drag |
| 11 | [The keyboard](#11-the-keyboard) | The pages and keys, the key-by-key walk, how a press types, the typing-only partial count, and what the field component will take over |
| 12 | [The select sheet](#12-the-select-sheet) | The bottom sheet a dropdown opens: its height, how many rows a page, and the overlay that takes and gives back focus |
| 13 | [Claude's mascot page](#13-claudes-mascot-page) | The mascot's widget and demo page as ported from the agent deck: the moves and their pace, the cell, the page's layout and controls, its sounds, the V1.0's waveform |
| 14 | [The agent deck port's additions](#14-the-agent-deck-ports-additions) | What the deck's screens needed of the kit: the `title`, `caption-strong` and `caption-regular` roles, words one size down, the small chip, the foot's icons and the link indicator, the stacked status block, a list row's body title and its supporting line, a detail row's labels, the outlined network row, the swipe page, the timed holds, the stepped line, the settings' one left edge and the quiet words' proof |

---

## 1. Rules

### Do

1. **Take every value from the token header, by role** — `kColorTextPrimary`, never `0x00` or a level number (§6).
2. **Draw every frame as if a partial refresh will show it** — the roles' partial-refresh values in §2.1 are how the
   glass looks most of the time.
3. **Compile every glyph a partial refresh draws at one bit a pixel** (§3).
4. **Count partial refreshes and draw the update after twenty as a full one** (§5).
5. **Fire the buzzer tick from the input handler, before the refresh is queued** (§5).
6. **Change DESIGN.md's front matter and this doc's tables in the same change**, then the header (§7).

### Never

- **Paint `scale.primary.1` or `scale.primary.2` solid on a UI element** — fill it with `ui::Fill`, the dither (§4).
  The solid greys are for images drawn with a greyscale refresh (DESIGN → Colors → The four levels).
- **Animate** — no timer drives a redraw faster than §5's limits.
- **Draw a stroke under 2 px, a weight under 600 except `body` and `caption-regular` at 400, or words under `caption`, 26 px.** A lighter
  weight breaks up at one bit a pixel; `body`'s 3 px strokes keep its gaps open, and `caption-regular`'s 1 and 2 px are the person's choice
  for a supporting line, open to the glass (§3).
- **Use a value the header does not have** — decide it in DESIGN.md first.

## 2. Colour

### 2.1 Roles

The panel stores two bits a pixel for a greyscale full refresh, levels 0 to 3. A partial or mono full refresh keeps
one, and a level under 2 becomes black, 2 or more white `[HW]`. The tables below restate DESIGN.md's colours with
their value on each refresh — the design check reads them (§7).

| Token | Full refresh | Partial refresh | Use |
| --- | --- | --- | --- |
| `colors.scale.primary.0` | `#000000` | `#000000` | Level 0 |
| `colors.scale.primary.1` | `#555555` | `#000000` | Level 1 |
| `colors.scale.primary.2` | `#AAAAAA` | `#FFFFFF` | Level 2 |
| `colors.scale.primary.3` | `#FFFFFF` | `#FFFFFF` | Level 3 |
| `colors.scale.sparse.sixteenth` | `#F8F8F8` | `#F8F8F8` | One black pixel in sixteen, as a dither on every refresh — no panel level; the words on a disabled control's face (§4) |
| `colors.scale.sparse.eighth` | `#F0F0F0` | `#F0F0F0` | One black pixel in eight — a disabled row's name on the white page, and elevation 1's offset (§4) |

| Kind | Means | Paired in §2.2 |
| --- | --- | --- |
| surface | A background words are painted on | As the background for every text and non-text role |
| text | Words, or an icon that carries meaning | On every surface, floor 4.5 |
| non-text | Edges, dividers, the focus ring | On every surface, floor 3 |
| fill | A solid background for a control or a state | As the background for every on-fill role |
| on-fill | Words on a fill | On every fill, floor 4.5 |
| exempt | Decoration, a disabled control's face and its words — never an edge, never the only sign of a state — and a mark that sits on one surface only, paired by hand in §2.2 with that surface: `thumb` on `band`, `thumb-on` and `knob` on `surface-on`, and a checkbox's check or a radio's dot, which are `thumb-on` | Not paired by the kinds |

| Token | Kind | Full refresh | Partial refresh | Use |
| --- | --- | --- | --- | --- |
| `colors.background` | surface | `#FFFFFF` | `#FFFFFF` | The glass |
| `colors.text-primary` | text | `#000000` | `#000000` | Every word that must read |
| `colors.text-secondary` | text | `#555555` | `#000000` | Supporting words, timestamps |
| `colors.outline` | non-text | `#000000` | `#000000` | Control and card edges |
| `colors.divider` | non-text | `#555555` | `#000000` | The rule between rows |
| `colors.focus-ring` | non-text | `#000000` | `#000000` | The buttons' focus |
| `colors.surface-action` | fill | `#000000` | `#000000` | The action button |
| `colors.surface-chosen` | fill | `#000000` | `#000000` | A chosen row, chip or option — never focus |
| `colors.surface-on` | fill | `#000000` | `#000000` | A toggle that is on, a slider's fill, a scroll indicator's thumb |
| `colors.text-on-fill` | on-fill | `#FFFFFF` | `#FFFFFF` | Words on any fill |
| `colors.band` | exempt | `#AAAAAA` | `#FFFFFF` | A light-dither fill on every refresh (§4) — the partial column is its solid value, which a fill never draws: a toggle's, segment's, slider's, checkbox's or radio's track, a chip's, card's or tonal button's fill, a progress bar's track, a scroll indicator's pieces, decoration, a disabled control's face |
| `colors.thumb` | exempt | `#000000` | `#000000` | A toggle's thumb on its off track, the light dither — 16 : 1 against the dither's average; paired with `colors.band` in §2.2 |
| `colors.thumb-on` | exempt | `#FFFFFF` | `#FFFFFF` | A toggle's thumb on its black, on track; paired with `colors.surface-on` |
| `colors.knob` | exempt | `#FFFFFF` | `#FFFFFF` | A range slider's knob bar on its black fill; paired with `colors.surface-on` |
| `colors.shadow-1` | exempt | `#F0F0F0` | `#F0F0F0` | Elevation 1's offset, a raised surface's: one black pixel in eight on every refresh (§4.1) — decoration, with no words or marks on it; no surface uses it yet |
| `colors.shadow-2` | exempt | `#AAAAAA` | `#FFFFFF` | Elevation 2's offset, a bottom sheet's: the light dither on every refresh — the partial column is its solid value, which a fill never draws |
| `colors.shadow-3` | exempt | `#555555` | `#000000` | Elevation 3's offset, a dialog's: the dark dither on every refresh — the partial column is its solid value, which a fill never draws |
| `colors.keyline` | exempt | `#FFFFFF` | `#FFFFFF` | The white line round an offset's outer edge, `spacing.stroke.hairline` wide: invisible over white, an edge over a dither (§4.1) |
| `colors.text-disabled` | exempt | `#F8F8F8` | `#F8F8F8` | Words, check, dot and glyph on a disabled control's light-dither face: the sparse dither of one black pixel in sixteen on every refresh (§4) — the value is the pattern's average, in linear light |
| `colors.text-disabled-page` | exempt | `#F0F0F0` | `#F0F0F0` | A disabled row's name and state word, a disabled slider's knob and value, a disabled mark's dot — on the white: one black pixel in eight on every refresh |
| `colors.text-quiet` | exempt | `#555555` | `#000000` | The agent deck's small supporting lines, `caption` in the dark dither on every refresh — a fill: a partial draws it as its dither, the partial column its solid value; 3.50 : 1 on the white, under the text floor by an argued exception (DESIGN → Exceptions → Quiet words; §2.3) |
| `colors.mascot-body` | exempt | `#555555` | `#000000` | The mascot's body, hands and legs: the dark dither on every refresh, a level lighter than its shade and darker than its tint — provisional, refined on the glass — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-shade` | exempt | `#000000` | `#000000` | A side of the mascot turned away (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-tint` | exempt | `#AAAAAA` | `#FFFFFF` | A hand in front of its body: the light dither — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-laptop` | exempt | `#AAAAAA` | `#FFFFFF` | The work's laptop — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-flag` | exempt | `#AAAAAA` | `#FFFFFF` | The finish's flag — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-flag-check` | exempt | `#000000` | `#000000` | The flag's checks (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-flag-pole` | exempt | `#000000` | `#000000` | The flag's pole (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-dust` | exempt | `#AAAAAA` | `#FFFFFF` | A puff of dust at its feet — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-dust-faint` | exempt | `#F0F0F0` | `#F0F0F0` | The puff's fainter half: one black pixel in eight (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-sweat` | exempt | `#AAAAAA` | `#FFFFFF` | The sweat's beads — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-confetti-a` | exempt | `#000000` | `#000000` | The celebrate's confetti, every other piece (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-confetti-b` | exempt | `#AAAAAA` | `#FFFFFF` | And the pieces between — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-faded-body` | exempt | `#AAAAAA` | `#FFFFFF` | A previous chat's mascot: its body — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-faded-shade` | exempt | `#555555` | `#000000` | Its shade — a fill: a partial draws it as its dither, the partial column its solid value (DESIGN → Colors → The mascot's greys) |
| `colors.mascot-faded-tint` | exempt | `#F0F0F0` | `#F0F0F0` | Its tint: one black pixel in eight (DESIGN → Colors → The mascot's greys) |

### 2.2 Contrast pairs

**Every pair clears its floor, 4.5 : 1 for words and 3 : 1 for edges, on both refreshes; the table gives each ratio.**

| Foreground | Background | Floor | Full refresh | Partial refresh |
| --- | --- | --- | --- | --- |
| `colors.text-primary` | `colors.background` | 4.5 | 21.00 | 21.00 |
| `colors.text-secondary` | `colors.background` | 4.5 | 7.46 | 21.00 |
| `colors.outline` | `colors.background` | 3 | 21.00 | 21.00 |
| `colors.divider` | `colors.background` | 3 | 7.46 | 21.00 |
| `colors.focus-ring` | `colors.background` | 3 | 21.00 | 21.00 |
| `colors.thumb` | `colors.band` | 3 | 9.04 | 21.00 |
| `colors.text-on-fill` | `colors.surface-action` | 4.5 | 21.00 | 21.00 |
| `colors.text-on-fill` | `colors.surface-chosen` | 4.5 | 21.00 | 21.00 |
| `colors.text-on-fill` | `colors.surface-on` | 4.5 | 21.00 | 21.00 |
| `colors.thumb-on` | `colors.surface-on` | 4.5 | 21.00 | 21.00 |
| `colors.knob` | `colors.surface-on` | 4.5 | 21.00 | 21.00 |

- **The ratios are the levels', computed from their hex values** — the glass's own white and black sit closer, so each
  ratio is an upper bound on the device `[?]`.
- **A grey fill draws as a dither, not the partial-refresh column** — the tables give the solid value a text or line
  role draws. A fill of level 1 or 2 is its pattern (§4), averaging `#898989` and `#E0E0E0` in linear light `[D]`.
  Black words on the light dither average 16 : 1. White words on the dark dither 3.5 : 1, under the floor — so words
  sit only on the light dither, in black (DESIGN → Elevation & Depth → Dither) `[D]`.
- **The thumb's pair is on its own surface, `band`** — full refresh 9.04 : 1 against the solid level 2, partial 21 : 1
  against white, and 16 : 1 against the dither's average `[D]`. It never sits on the white glass, so no pair is filed
  there. The same for the white thumb and the knob, which sit on `surface-on` alone.
- **An offset is decoration and exempt** — `shadow-1`, `shadow-2` and `shadow-3` carry no words and no state, so no pair is filed
  for them, as for `band`. Their averages in linear light against the white page are 1.14 : 1, 1.31 : 1 and 3.5 : 1. Level 2
  over the light dither is the same pattern as its ground (1.00 : 1) and the eighth over it 1.16 : 1, which is what the keyline
  is for. A raised surface's boundary is its `spacing.stroke.strong` outline, black on white, the table's 21 : 1 pair `[D]`.
- **A track's own edge is not a 3 : 1 boundary** — the light dither averages 0.75 white in linear light, 1.3 : 1
  against the white glass. So what identifies a toggle, a segment or a slider is a part that is a boundary. These are the
  black thumb (16 : 1 on the dither), the black fill and its white knob, the black chosen item, the ring when focused,
  and the state or the value in words `[D]`.
- **`colors.text-secondary` on a fill has no pair** — 2.82 : 1 on black; words on a fill are `colors.text-on-fill`.
- **A disabled control's words have no pair, by exemption** — WCAG 1.4.3 excuses an inactive component, and the person's
  brief is that it should look inert ("disabled elements are exempted from accessibility contrast"). The ratios are the
  patterns' averages in linear light `[D]`: `colors.text-disabled` (fifteen white in sixteen) on the light-dither face
  (three in four) is 1.23 : 1; `colors.text-disabled-page` (seven in eight) on white is 1.14 : 1. The face against white
  is 1.31 : 1. They are held by nothing but legibility on the glass, which they have on a V1.0 and a V2.0 `[SEEN]`, and by
  the reason in solid `caption`, `text-secondary`. A pair under the floor is never filed in the pairs table, so the design check does not read it.
- **A checkbox's check and a radio's dot are `colors.thumb-on` on `colors.surface-on`** — the table's pair, 21 : 1. An
  off box has no black part, so it keeps a `spacing.stroke.default` `colors.outline` edge, 21 : 1 on the glass, rather
  than rely on the dither alone, 1.3 : 1.

### 2.3 Quiet words

**Supporting words in the dark dither do not clear the floor, so `colors.text-quiet` is an `exempt` role held by an argued exception, never paired.** The agent deck's
reset lines, a sub-agent's tokens, "4 sessions" and a request's agent name are drawn in `ui::Fill(1)` in the approved mocks, the
person's look, 2026-11-25. That is a word of `caption` or `label` whose every stroke pixel is the dark dither (`kDitherDark`, `0x56`:
one white pixel in four on every refresh, §4.1), where `text-secondary` is solid `#555555` on a greyscale frame and black on a
partial one (§2.2).

| Words | Ground | Linear-light average of the words' pixels | Ratio | Floor |
| --- | --- | --- | --- | --- |
| `text-secondary` on a full refresh | `colors.background` | `#555555` | 7.46 | 4.5 |
| `text-secondary` on a partial refresh | `colors.background` | `#000000` | 21.00 | 4.5 |
| Quiet words, every refresh | `colors.background` | `#898989` (one in four white) | 3.50 | 4.5 |

- **The ratio is the dither's average, the measure this doc uses for every dither (§2.2)** — `contrast.py '#898989' '#FFFFFF'` prints
  `FAIL 3.50:1 (floor 4.5:1)`. It is the same 3.5 : 1 as white words on the dark dither (§2.2), and the reason DESIGN → Elevation &
  Depth → Dither keeps words off it. Only a disabled control's words, decoration and a logotype may sit under the floor (DESIGN → Colors →
  Contrast floors), and supporting words are none of them.
- **The role is `exempt` and no pair is filed** — a `colors.text-quiet` of kind `text` would fail D1 on its own pair, and the check reads
  `exempt` as a role it does not pair, as it reads a disabled control's words. It is the person's decision, 2026-11-25, that these words stay
  grey: DESIGN → Exceptions → Quiet words argues it against principle 1 and bounds it to a supporting line, `caption`, on the deck's screens,
  never the one thing a screen says. No other word takes the role, and the floor holds for every one.
- **The look is in the kit behind one call** — `ui::QuietWords(label)` (`components/ui/ui.cpp`) sets the label's text colour to
  `ui::Fill(kColorTextQuiet)`, the token in the header (`kColorTextQuiet`, level 1). It is on the kit page ("Quiet words") beside
  `text-secondary`. A screen of the deck takes it for the four places above.

## 3. Type

- **The face is Bitcount Single**, a pixel font, at weights 600 and 700, and 400 at 30 and 26 px alone. The Regular, SemiBold and Bold are
  the upright instances of its variable font, Version 1.001 (`wght` 400, 600 and 700, `ELXP` 0, `ELSH` 0, `slnt` 0, `CRSV` 0),
  from petrvanblokland/TYPETR-Bitcount, cut with fontTools' instancer and under the SIL Open Font License
  (`components/ui/fonts/bitcount/OFL.txt`). The Regular is for `scale-30-regular` and `scale-26-regular`.
  The lighter weight's ink is shorter, so it compiles to a shorter line, on the same cells. The table gives each file's.

| File | `wght` | `line_height`, `base_line` at 30 px |
| --- | --- | --- |
| `BitcountSingle-Regular.ttf` | 400 | 24, 3 (21, 3 at 26 px) |
| `BitcountSingle-SemiBold.ttf` | 600 | 26, 4 (24, 4 at 26 px) |
| `BitcountSingle-Bold.ttf` | 700 | 26, 4 (24, 4 at 26 px) |

- **Every size is compiled at one bit a pixel** by `lv_font_conv` 1.5.3 (`components/ui/fonts/bitcount/make_fonts.sh`),
  so a stroke is solid black on every refresh. No font engine runs on the device.
- **Bitcount draws on a 100-unit cell, a tenth of the em, so every size is a multiple of 10 px but 26** — each cell then falls
  on whole panel pixels, 3 px at 30 px and 6 px at 60 px. A size between draws its cells unevenly, and 26 px does: its
  2.6 px cells fall 2 and 3 px wide. The person chose it over 20 px, too small on the glass, after both on a V1.0
  (2026-11-26; DESIGN → Typography → Face). `design_tokens.h`'s `static_assert` allows 26 alone off the tens. Weight widens each
  cell's square by 20 units at 600 and 30 at 700, so the two weights look alike below about 50 px `[D]`.
- **The scale is the primitives, `typography.scale-<size>-<semibold|bold>`, and `typography.scale-30-regular` and `typography.scale-26-regular`.** The sizes are
  26, 30, 40, 50, 60, 70 and 80 px with every character the strings use, and 100, 120 and 160 px with figures only. Each comes at
  both weights, with a line pitch of 1.2 em to 40 px and then tapering: 56 px at 50, 64 at 60, 72 at 70 and 80 at 80.
  Each step is on the 4 px grid and never under the compiled font's own `line_height`. A pitch under it would be clamped
  to it, since `LineSpace` stops at 0. The figures' pitch is the height of their digits, a font box rounded up to 4 px: 68 at 100, 80 at 120 and
  108 at 160 (one line only, so no leading is seen). They are `kTypographyScale30Semibold` and its
  siblings in `design_tokens.h`. The two 400s are `kTypographyScale30Regular` and `kTypographyScale26Regular`, each with the same glyph range as the other sizes
  (`bitcount_400_30.c`, `bitcount_400_26.c`, the "•" included); `make_fonts.sh` compiles both. The 26 px fonts' objects on the
  ESP32-S3 are 3,076 bytes at 400, 3,376 at 600 and 3,764 at 700, against 3,702 for `bitcount_400_30`. The app image grew
  2,816 bytes when they took the 20 px fonts' place, 1,110,992 to 1,113,808 `[D]`.
- **A role is one primitive.** The kit's are `hero` `scale-60-bold`, `display` `scale-50-bold`, `headline` `scale-40-bold`,
  `body` `scale-30-regular` on a 40 px line, `label` `scale-30-bold` and `caption` `scale-26-semibold`. The agent deck adds
  three. The first, `title`, is `scale-30-semibold` (`kTypographyTitle`, 36 px line). The second, `caption-strong`, is
  `scale-26-bold` (`kTypographyCaptionStrong`, 32 px line). The third, `caption-regular`, is `scale-26-regular`
  (`kTypographyCaptionRegular`, 32 px line). `title` takes `bitcount_600_30`, `caption-strong` `bitcount_700_26` and
  `caption-regular` `bitcount_400_26`. `ui::Type` gains `kTitle`, `kCaptionStrong` and
  `kCaptionRegular` after `kCaptionLoose`, and `ui::Font`, `LineBox` and `NewLabel` take them as any role. In the header, `kTypographyBody` is
  `kTypographyScale30Regular`'s size and weight on a 40 px line. Google's linter allows no group under `typography`, so DESIGN.md
  writes each role's values out with its primitive named beside it.
  A Wi-Fi network's name is `caption-strong` over its facts in `caption-regular`, the status band's news is `caption`, and a choice card's name is `label`. A select sheet's options
  are `body`, the chosen one `label`. `caption-loose` is `caption`'s primitive on a 40 px line (`kTypographyCaptionLoose`), a
  text view's. The deck's is a 24 px line on 16 px type, 1.5 em `[DECK]`; 1.5 em of `caption`'s 26 px is 39, and on the 4 px grid 40.
- **A role's line height is its line pitch** — 64, 56, 48, 40, 36 and 32 px, 40 for `caption-loose`, and 36 for `title` and 32 for
  `caption-strong` and `caption-regular`. `ui::NewLabel` and the
  screen's style set LVGL's line space to the pitch less the compiled font's height. So the lines of a block step by the pitch.
  A single line is the compiled font's height, about 0.9 em. That is 24 px for `caption` at 26 px and 26 for `label` at 30
  (`line_height` 26, `base_line` 4). It is 24 for `body` at 30 (`line_height` 24, `base_line` 3) and 52 at 60. A block of n lines is
  n − 1 pitches and one font height `[D]`. A widget's name over a detail is made whole line boxes instead (§9). A free label stays
  the font's height.
- **The glyph ranges** are ASCII and · ° × – — ’ • … at 26 to 80 px, and the space, the figures and the colon at
  100 px and up; Bitcount has no arrows, so no string uses one. What a character outside them draws on the glass is
  open (§8).
- **Every character is 0.6 em wide** — the face is monospaced — so changing figures keep their width without a
  tabular setting; `body` fits 24 characters across `spacing.content`, and `caption` 27 `[D]`.
- **No letter spacing** — it would break the cells off the pixel grid.

## 4. Space, strokes and shapes

- **Every spacing, stroke and radius is a whole number of panel pixels**, as DESIGN.md's front matter gives it — no
  scaling at run time.
- **`spacing.target`, 64 px, is a hit area**: a control drawn smaller pads its touch box to it.
- **The gaps are role tokens, each a step of the scale.** The header has `kSpacingPairSm` 8, `PairMd` 16, `PairLg` 24, `PairXl` 32,
  `kSpacingItem` 16, `kSpacingGroup` 24, `kSpacingSection` 48 and `kSpacingEdge` 24 (DESIGN → Layout → Spacing).
  A pair's gap is the gap between the two texts' own boxes, the font's height with no leading. The line boxes of `ui::NewLabel`
  and `detail::Words` hide it. The leading a line box adds below the upper text and above the lower is taken back as a negative
  margin (`widgets::detail::Glue`, `internal/glue.h`), so the column's `pad_row` is the gap `[D]`.
  A section heading's `pad_top` is the section less the parent's own `pad_row`, so the two add up to 48 whatever the parent sets.
  The page's first heading, with nothing above it, keeps `spacing.lg`.
- **A stepped line moves `kSpacingMarqueeStep`, 48 px, a step** — a whole number of pixels, `spacing.marquee-step` (§14.6).

### 4.1 The dithers

- **A grey fill is a dither** — `ui::Fill(level)` gives levels 1 and 2 their fill codes (`FillCode`, in
  `glass_rules.h`, with `OneBitWhite`, the pattern each draws; `tests/host` checks both) in LVGL's L8 frame.
  The codes are `lvgl_display::kDitherDark` (`0x56`) and `kDitherLight` (`0xAB`), one step above the plain greys.
  The one-bit packing (`PackMono`) draws them as fixed patterns locked to the panel's pixel grid: level 1 one white pixel
  in four, level 2 three in four. Every refresh keeps them. A greyscale refresh reads the codes as the true greys. Text and
  lines take `ui::Level`, so their greys threshold to solid black or white. An anti-aliased edge pixel thresholds
  too, unless its blend lands exactly on a code, when it draws as the dither — rare, and why words sit only on the
  light dither. Both patterns read as distinct greys, and alike, on both boards `[SEEN]`.
- **Two sparse dithers are the one text drawn as a pattern** — a disabled control's words (DESIGN → Components → States →
  Disabled). The function `ui::Fill` gives levels 4 and 5 of the token header (`kColorTextDisabled`, `kColorTextDisabledPage`) their codes,
  `kDitherSixteenth` (`0xC1`) and `kDitherEighth` (`0xC2`). `OneBitWhite` takes the pixel's column and row on the
  panel, not their parity. The sixteenth is black at every fourth column on every fourth row (one pixel in sixteen).
  The eighth is black at every fourth column on every second row (one in eight). Each black pixel is one of the light
  dither's, so words on the face are the face with its dots taken out, and the pattern never moves with what is drawn
  over it.
  The codes sit in the top quarter of the grey range, mid-band, so a greyscale refresh reads both as white (images only,
  where no disabled control stands) and an anti-aliased edge rarely lands on one. The one-bit font draws every stroke
  pixel as the text colour, so a glyph keeps the pattern with no grey edge. `tests/host/disabled_words_lvgl_test.cpp`
  renders `label` and `caption` in the real Bitcount fonts, and the A1 check and speaker-off marks recoloured, on LVGL's
  L8 frame. It checks that the only values are the ground's and the code, and that the code is on exactly the mark's
  ink `[HOST]`.
  All of it is legible on both boards `[SEEN]`.
- **A raised surface's offset is its own shape, `spacing.overlay-offset`, 6 px, away — in the dither of its elevation level,
  with a white keyline round it.** The offset is drawn by `ui::AddOffset` (`components/ui/ui.cpp`) before the box, which
  covers all of it but the exposed edge. It is a hard offset, never a blur or solid black. `kLevels[]` there is the one table of the scale
  (`design_tokens.h`'s `kElevation0` to `kElevation3`, the table below). The keyline is a white rectangle one
  `kSpacingStrokeHairline` larger on every side, drawn under the offset with its radius grown by the same, so a white line rings
  its outer side. The reach, offset and keyline together, is 8 px, which the box's parents let out. `ui::LetRingOut` allows 10 px, the
  6 px editing ring's reach beside the 8 px of the ring and the offset here.
  The side is the surface's: down and to the right for a floating box (the dialog, `OffsetSide::kBottomRight`), up from the top
  for a sheet that hangs off the glass's foot (`OffsetSide::kTop`). Each level draws its own pattern at the
  panel's pixel grid, which every one-bit refresh keeps. `tests/host/elevation_lvgl_test.cpp` renders each level on LVGL and
  reads the band, the keyline over a black ground and the untouched box `[HOST]`. Each reads on both boards `[SEEN]`.

| Level | Offset dither | Colour token | Dither code |
| --- | --- | --- | --- |
| 0 | None | — | — |
| 1 | The eighth | `kColorShadow1` | `kDitherEighth` |
| 2 | The light dither | `kColorShadow2` | `kDitherLight` |
| 3 | The dark dither | `kColorShadow3` | `kDitherDark` |

### 4.2 Rows, buttons and pages

- **A list row's padding is two values** — `spacing.sm` above and below, `spacing.md` either side; DESIGN.md's
  front matter carries the vertical one, since a component takes one padding token. Its title and supporting line
  are one line each on their line boxes (§9), 36 px of `label` and 32 of `caption`: 68 px. That is 4 more than the 64 inside the
  80 px row's padding. So the pair stands centred 2 px into it either side, its ink well clear of the row's edge `[D]`.
  Rows are `spacing.md` apart with no rule between: a ring reaches 8 px, so a rule in a 16 px gap would touch it.
- **The ring is outside every control (§9)**, so a button's label has the whole box: a half button `(432 − 16) / 2` =
  208 px wide, 16 px from its neighbour, keeps 202 px inside its 3 px edge for 10 characters of `label` at 18 px a
  character, 180 px. A full-width button keeps 426 px inside its edge for 22 characters, 396 px, 15 px clear a side. A footer button `(432 − 2 × 16) / 3` = 133 px keeps 127 px for 6 characters, 108 px `[D]` (DESIGN →
  Voice & Content Strategy → Labels and states). **The padding is `spacing.md` a side**, which leaves 170 px in the half
  button (208 − 2 × 3 − 2 × 16) and 95 px in the footer button, less than the words. The label is as wide as its words and
  centred, so it sits over the padding and is clipped by nothing — LVGL clips a child to its parent's whole box, not its
  padded content — and never reaches the edge: 11 px from the half button's 3 px edge, and 9 to 10 px from the footer
  button's (127 − 108 = 19, shared by the two sides in whole pixels) `[D]`. A longer label would reach the edge, which is
  why a button's name is shortened, never cut off (the citation above).
- **A page's rows, with a footer: eight 64 px one-line rows (8 × 80 − 16 = 624), six two-line rows of 80 (6 × 96 − 16 =
  560). Without one, seven (7 × 96 − 16 = 656)** — the footer is `spacing.footer` 96 `[D]`.

## 5. Refresh and feedback

**The 20th partial refresh after a full is drawn full, and every press ticks on the buzzer before the glass answers; the tables give each limit and sound.**

| Token | Value | What the code does |
| --- | --- | --- |
| `kRefreshPartialLimit` | 20 | Counts partial refreshes; the next update after 20 is drawn full, and a full one resets the count — proven on the SSD1677, and on the SSD2677 no ghost seen before it `[HW]` `[SEEN]`; while the keyboard is open only the frames a typing press or a page switch caused count, and `kUncountedPartialLimit` (60) caps the rest (below) |
| `kFeedbackTickMs` | 100 ms | The buzzer tick sounds as the input watcher reads a press — one poll, 10 ms (`kWatchMs`), after it — even during a refresh: `devices::Tick` plays `sound.tick` (`kSoundTick`), 30 ms of the full square at 4 kHz, through the buzzer's note player, whose task runs above the watcher and cuts whatever sound is playing, so a tick never waits behind another sound ([STICKY_HARDWARE_SPECS §4.2](STICKY_HARDWARE_SPECS.md#42-devices)) |
| `kSoundTick` | 4 kHz, the full square, 30 ms | `sound.tick`: every press and every repeated step — `devices::Tick`, which `lvgl_display::SetPressFeedback` runs |
| `kSoundAtEnd` | 2637 Hz twice, 25 ms each, 45 ms apart | `sound.at-end`: a step that can go no further — `lvgl_display::PlayCue(Cue::kAtEnd)` from the list view, the text view, the horizontal pager and the range slider (below) |
| `kSoundError` | 4186, 3322, then 2794 sliding to 2093 Hz; 600 ms | `sound.error`: that did not work — `devices::Play(sound::Word::kError)`, and the status line's error news (`Cue::kError`) |
| `kSoundSuccess` | 3136 then 4186 Hz on the 25 % pulse; 260 ms | `sound.success`: only once the action succeeded — `devices::Play(sound::Word::kSuccess)`, and the status line's success news (`Cue::kSuccess`) |
| `kSoundRecordingStart` | 3520, then 5274 Hz on the 25 % pulse; 235 ms | `sound.recording-start` — before the recording, never in it |
| `kSoundRecordingStop` | 5274, then 3520 Hz on the 25 % pulse; 235 ms | `sound.recording-stop` |
| `kSoundAttention` | 3136 then 2638 Hz, twice, 90 ms apart; 590 ms | `sound.attention`: a request or question joins the agent deck's queue (STICKY_AGENT_DECK §6) |
| `kSoundLinkUp` | 3136 Hz, then 4460 sliding to 4700 Hz; 330 ms | `sound.link-up`: the deck's link comes up |
| `kSoundUsageLimit` | three falling slides from 2352 to 2040 Hz, then a wobble at 2000–2050 Hz; 600 ms | `sound.usage-limit`: the deck's usage record names the plan's limit |
| `kSoundAgentStart` | 2800 sliding to 2200 Hz; 120 ms | `sound.agent-start`: an agent on the deck starts working, new or back from idle or done |
| `kSoundBandLowHz`, `kSoundBandHighHz` | 2000, 6000 Hz | `sound.band-low`, `sound.band-high`: every pitch between, round the buzzer's 4 kHz resonance; the header's `static_assert`s hold the ten to it as it compiles (§6), and `tests/host/sound_rules_test.cpp` checks the sounds as played `[HOST]` |
| `kSoundTickMaxMs`, `kSoundMaxMs` | 40, 600 ms | `sound.tick-max`, `sound.max`: a tick's longest and any other sound's, held by the same two |
| `kSoundMomentsMaxMs` | 460 ms | `sound.moments-max`: the mascot's moments told since the last refresh, joined and played in turn as the next refresh starts, while they fit; the rest dropped (below) |
| `kSoundMascotBlink` … `kSoundMascotFlop` | 10–120 ms each | `sound.mascot-*`: Claude's mascot's 25 moments, in `widgets::MascotSfx`'s order — the agent deck's `kFx*` effects at the deck's own pitches, the person's ear on a V2.0 (2026-11-24, the console's `sound ab`) over the same moved into the band; the header's `static_assert`s hold them to `sound.max` |
| `kSoundMascotTyping`, `Spin`, `Success`, `Wave`, `Think` | 0.9–3.6 s each | The mascot's music, the deck's `kWorking`, `kSpin`, `kSuccess`, `kWave` and `kThink`: `devices::PlayMusic` keeps it in time with its move under every other sound (`sound_rules.h` `MusicAt`); the mascot page picks it as the deck's stage does (`mascot_rules.h` `MusicOnView`) |
| `kWaitIndicatorDelayMs` | 1000 ms | Declared for DESIGN.md's rule — a wait's indicator drawn only once the wait has run this long, `motion.wait-indicator`; no screen has a wait indicator yet |
| `kSelfUpdateMinMs` | 5000 ms | The period of the hardware-test screens' self-updates (Sensors, Battery and power) — DESIGN.md's floor between two self-driven redraws of an area, `motion.self-update`, which no code enforces |
| `kClockTickMs` | 60000 ms | Declared for DESIGN.md's clock redraw, `motion.clock-tick`; the firmware has no clock yet |
| `kMotionKeyRepeatDelayMs` | 600 ms | How long UP or DOWN is held before it steps again, `motion.key-repeat-delay` — `kKeyRepeatDelayUs` in `key_rules.h`, which also lets a step that drew nothing be followed this long after it |
| `kMotionKeyRepeatPeriodMs` | 500 ms | How often a held UP or DOWN steps after that, `motion.key-repeat-period` — `kKeyRepeatPeriodUs` in `key_rules.h` |
| `kMotionMarqueeRestDelayMs` | 4000 ms | How long a stepped line rests with its start in view before its passes, each time it begins, `motion.marquee-rest` — `widgets/src/marquee.cpp` `kRestMs` (§14.6) |
| `kMotionMarqueeStepDelayMs` | 1000 ms | The one clock every stepped line moves on, `motion.marquee-step`: one lv_timer, one partial refresh a step (§14.6) |
| `kMarqueePasses`, `kMarqueeGapSpaces` | 3, 3 | A stepped line's passes before it rests, and the spaces before its words come round: counts, no size or time, so no DESIGN token — in `token_parity.py`'s `HEADER_ONLY` as the refresh policy's are |

- **The ten sound words are DESIGN.md's `sound.*` tokens, and `devices` plays them by word** (`sounds.h`: `kSounds`, in
  `sound::Word`'s order, `devices::Play(Word)`). The hardware test's Buttons and buzzer screen has a row for each, and the
  console's `sound <name>` plays one by its token's name (`tick`, `at-end`, `error`, `success`, `recording-start`,
  `recording-stop`, `attention`, `link-up`, `usage-limit`, `agent-start`). The person chose the first six by ear on a
  V1.0. They chose the deck's four from a draft (2026-11-26). The deck's four lead (`sound::Leads`, DESIGN → Motion →
  Sound). While one sounds, `devices::PlayMoments` drops the frame's moments.
- **Claude's mascot's moments sound as the frame that shows them starts.** A page that shows the mascot hands its
  `MascotSfx` to `devices::QueueMoment` (`sounds.h`: `sound::Moment`, the same 25 in the same order, which
  `tests/host/mascot_lvgl_test.cpp` asserts). `lvgl_display::SetFrameHook`, which the shell sets to
  `devices::PlayMoments`, plays those held as each rendered frame goes to the glass. They play in the order told, one
  after another on the buzzer's one voice, never mixed, within `sound.moments-max`. The first that would pass it and every one after are
  dropped, never carried into the next frame (`sound_rules.h` `TakeMoments`, `tests/host/sound_rules_test.cpp` `[HOST]`).
  The mascot's widget tells its moments at the deck's pace as it draws. A frame renders and goes out only as the last
  refresh ends, so a frame's moments are those told since. A press's tick cuts them as it cuts any sound. Whether the ink's
  change lands with them — the drive is about 416 ms on a V2.0 — is not yet heard on the glass `[?]` (MASCOT §6).
- **The mascot page shows the refresh it costs.** The hardware test's `hw.mascot` page (`components/hw_test/mascot_page.cpp`,
  its passes in `mascot_rules.h`, `tests/host/mascot_rules_test.cpp` `[HOST]`) counts the glass's frames in the stage's
  corner as "n of N". A three-line readout under the move shows the partials since the last full refresh (`Stats::partials`
  of `kRefreshPartialLimit`) and the full refreshes since the page opened. A move draws through a full refresh: the frame
  the twentieth partial falls on is drawn full and the move goes on, and the readout's counts are redrawn with the next
  frame. Stopping a move drops its moments not yet played (`devices::DropMoments`). Not yet seen on the glass `[?]`.
- **A widget asks the at-end sound by cue, never the buzzer** — `lvgl_display::PlayCue(Cue::kAtEnd)`, which the shell
  sets to `devices::Play(Word::kAtEnd)` (`SetCuePlayer`). The list view and the horizontal pager ask it when a swipe would
  turn past the last or the first page. The text view, open for reading, when UP or DOWN or a swipe would. The range
  slider, open for editing, when a step would pass its end — each repeated step of a held key too. A key's tick has
  sounded already, from the input watcher. The at-end sound follows within one read of the keypad and cuts it, so the
  person hears the tick's start and then the double tick. A swipe's tick sounded at the contact. Its at-end sounds at the
  lift. UP and DOWN that walk a list's rows leave it at its end for the next control, as the group wraps, and ask nothing.
  `tests/host/slide_off_lvgl_test.cpp` and `swipe_lvgl_test.cpp` count the cue `[HOST]`. On a V1.0 and a V2.0 the at-end
  sound cuts a key's tick, and the outcome sounds read through a refresh in flight `[SEEN]`.
- **Outcomes sound where they happen** — the hardware test's Microphone screen plays `sound.recording-start` at the press,
  lets the microphone settle at least as long as the sound has left, so the recording never holds it, and closes on
  `sound.recording-stop`, or `sound.error` when the read failed. Its Storage screen's Read says its outcome in the
  status line — "Card read." sounds `sound.success`, "No card. Insert one to read it." and the mount failure sound
  `sound.error` — while its label keeps the facts (the next bullet). The kit page's keyboard and dialog
  are samples with no real outcome — Join with too short a password is a disabled action, which gets the tick alone, and
  a destructive action confirmed gets the press's tick (DESIGN → Motion → Sound) — so they sound none.
- **The status line sounds by cue and refreshes the band alone** — `router::Say` asks `lvgl_display::PlayCue(Cue::kSuccess)`
  or `Cue::kError` just before it sets the band's words, and the shell's cue player (`app_shell`) maps them to
  `devices::Play(Word::kSuccess)` and `kError`. Info asks nothing. Held under an overlay, the news sounds when it is drawn,
  as the overlay closes. The band is a fixed 80 px zone (`spacing.list-row-height`, y 44 to 123), so a line shown, replaced
  or retracted is one partial refresh of that zone, 480 × 80, and nothing below it changes. The render checks this: every
  pixel from y 124 down is identical empty, with one line and with two. Each counts toward `kRefreshPartialLimit`. A line
  that leaves with its screen costs no refresh of its own. The same words again are no refresh, but success and error sound again. The lower
  rule, a 2 px hairline drawn only while news shows, is the thinnest feature on the glass and the one thing a retraction can
  leave a faint trace of. On a V1.0 and a V2.0 a retraction leaves none and the band reads as plain words with no box.
  So the rule stays drawn only while news shows `[SEEN]`.
  `tests/host/status_rules_test.cpp` and `status_line_lvgl_test.cpp` hold the rules and the layout `[HOST]`.
- **A full refresh is drawn for** these, in order, with the code that sets each in
  [STICKY_EPAPER_WAVEFORMS §3.4](STICKY_EPAPER_WAVEFORMS.md#34-which-refresh-a-frame-gets):
  - the first frame after boot or wake;
  - a full a screen asks for (`RequestFull`, the hardware-test screens');
  - the update after `kRefreshPartialLimit` partials;
  - the one-bit frame after a greyscale refresh;
  - on the V1.0, the pause of 2 s after 150 frames by scans;
  - as a recovery, a partial that fails, drawn again in full at once, and the next frame after a failed refresh, settle or
    standby, since the glass is then unknown.

  A greyscale full refresh is for a still screen showing an image (DESIGN → Colors → The four levels). No product screen
  has one yet, and the hardware-test Four greys screen draws one as a fixture.
- **DESIGN.md also keeps a full refresh for closing a full-glass overlay and for the frame left before sleep** (DESIGN →
  Motion → Nothing animates). The router sets one as a full-glass overlay closes, and the keyboard is the one
  (`router::CloseOverlay` calls `lvgl_display::RequestFull`). No screen has that last frame yet. A bottom sheet and a dialog
  are part of the page and open and close on a partial, and the router asks no full refresh for them. Whether the sheet's partial close
  leaves no ghost where its `band` chosen row stands is unverified on both boards `[?]`.
- **While the keyboard is open only the presses that type, and a page switch, count toward the twenty** (the person's decisions): the keyboard sets
  `lvgl_display::SetPartialCounting(PartialCounting::kTypingOnly)` as it opens and `kEvery` as it closes. It calls
  `lvgl_display::NoteTyping()` in each press that changes the text: a character, Space, Delete on a non-empty text, and
  the field's clear cross, which changes the text as Delete does. It calls it too in each page
  switch (Shift, 123, ABC, #+=), which counts as one partial, since the redraw is on the same screen. `Send` reads
  the flag once per frame (`s_typed.exchange(false)`). So a frame that coalesces a ring move and a typing press counts,
  and one of ring moves counts nothing toward the twenty, on a V2.0 and for the V1.0's two-partial page change alike.
  The arithmetic is `PartialsCounted` and `PartialsOverLimit` in `glass_rules.h`. A frame that counts none is
  never forced full by the twenty, so the 21st typed character is drawn full. The moves that count none are not free.
  They add to a count of their own, `s_uncounted` in `display.cpp`. The frame that would take it past
  `kUncountedPartialLimit` in `glass_rules.h`, 60, is drawn full, which resets both counts (the person's decision; a
  start, to be tuned on the glass `[?]`). It is the same `PartialsOverLimit` arithmetic, so the 61st ring move is the
  first full. `tests/host/glass_rules_test.cpp` runs
  both rules against a counter `[HOST]`. What the moves leave is the dither and the black rings, which build up between
  those fulls, and leave no ghost on a V1.0 and a V2.0 `[SEEN]`. Opening the keyboard's screen is not a typing
  press. Its close is a full refresh.
  **On the V1.0 the ceiling holds unless a scan preset is chosen.** Its default preset is `Keep waveform`
  (`kDefaultTuning1677`), which never scans, so every frame is a partial and both counts apply as on a V2.0. With a scan
  preset chosen on the Display screen, a frame that changes at most 15 % of the glass (`kScanMaxChanged`), such as a ring
  move or a typed character, is drawn by scans. `Send` returns from `SendByScans` before either count, so it adds to
  neither `s_stats.partials` nor `s_uncounted`. The bound on a long walk there is the scan rule's own: after
  `kScanFramesBeforeFull` (150) scan frames the next pause of `kFullAfterPauseMs` (2 s) draws a full refresh (the scan
  bullet below).
- **A new screen on the V1.0 is two partials, through white**, not a full one. It takes 1.0 to 1.25 s against a full's 1.6 s,
  and counts as two toward `kRefreshPartialLimit`, so one that would pass it is drawn full. Ten page changes, then a
  full refresh, left no trace on the glass over a run of 80 `[SEEN]`.
- **Two full refreshes never run back to back** (DESIGN → Do's and Don'ts). A full or greyscale refresh asked for
  sooner after the last one ended than that one took waits out the rest in `Send` (`WaitAfterFlash`; its arithmetic is
  `FlashWaitMs` in `glass_rules.h`, which `tests/host` checks). Then it runs with the frame LVGL drew. It is never
  dropped (DESIGN → Motion → Nothing animates). The gap is the last full or grey refresh's own measured time — after a
  full, about 1 s on a V2.0 and 1.6 s on a V1.0. After a grey, the gap is the grey's time, 1.25 s on a V2.0 and 1.31 s on a V1.0
  (STICKY_EPAPER_WAVEFORMS §4). So it follows each controller and the temperature without a constant. Leaving Four
  greys straight after it lands held the full 546–607 ms on a V2.0 and 587–852 ms on a V1.0, and two Full presses held
  the second 1443–1523 ms on a V1.0 `[SEEN]`. A press during the wait still ticks and is replayed after it, in the
  frame after the held one. A failed refresh sets no gap, since it flashed nothing, so a retry never waits on a busy
  timeout. The full that redraws a failed partial runs at once, a recovery, rare, and the one exception.
- **Input during a refresh is kept** (DESIGN → Motion → Press). A task of its own, above LVGL's, reads UP, DOWN, OK
  and the touch panel every 10 ms, ticks on each press, and queues each press and release. LVGL replays the queue when
  the refresh ends, so a short press inside a refresh is never lost and moves focus once. An OK pressed before the frame
  the last OK caused has been drawn is ignored, though it ticks. A queued OK acts where the moves queued before it
  left focus, and a double tap is replayed as two taps (the person's choice, from the glass). A press
  or a move is queued only with room left for its release, so a release is never lost. Holding OK for 1 s runs the escape
  chain ([DESIGN → Focus and input](../DESIGN.md#focus-and-input)), once a hold, whether or not LVGL saw the hold begin (`OkHold` in `key_rules.h`).
  The tick sounded at once during a full refresh `[SEEN]`.
- **A held UP or DOWN steps again by `KeyRepeat` in `key_rules.h`, at the glass's pace** (DESIGN → Focus and input; the
  person's decisions: 600 ms, then every 500 ms, and every step ticks). LVGL's own repeat is off —
  `lv_indev_set_long_press_time` and `lv_indev_set_long_press_repeat_time` at `UINT16_MAX` on the keypad — since it stepped
  every 100 ms after 400 ms and turned a control's editing off on each repeat. When the queue is empty and the key is still
  down by the watcher's word too (`s_step_held`), the keypad read asks `KeyRepeat::Step`. A step is due once the key has
  been held `kMotionKeyRepeatDelayMs`, `kMotionKeyRepeatPeriodMs` after the last step. The SSD1677's scans must no longer
  drive the last frame (`RefreshBusy`), and the last step's frame must have landed (`LastRefreshEndUs` after it). For a
  step that drew nothing, the wait is `kMotionKeyRepeatDelayMs` after it. A frame being drawn needs no test of its own.
  The flush runs on LVGL's task, the task that reads the keys, so no read runs during one. The landed-frame test is what
  holds a step back until its frame is on the glass. The read then sends a release and a press, the press
  through `KeySeen` and the tick, and mapped to the editing keys when a control is open for editing. The release stops it
  at once and nothing is queued. OK never repeats (`KeyRepeats`). The last step is timed from when LVGL acted on it, not
  from when the watcher saw the press, so a press replayed after a refresh waits for its own frame. `tests/host/key_rules_test.cpp`
  checks the delay, the period, both gates, the no-frame wait, the release and one escape per hold `[HOST]`.
- **With no ring on the glass the first press only rings** — the rule is [DESIGN → Focus and input](../DESIGN.md#focus-and-input), the mechanics the router's key read hook
  ([APP_ARCHITECTURE §7](APP_ARCHITECTURE.md#7-screens-routes-and-focus)). On the glass the press ticks at once, as every press does, and its frame is the ring's partial. `ok_gate` in
  `ReadKeys` drops an OK pressed before the last OK's frame has landed. It treats that frame as the OK's, so an OK pressed
  once the ring is on the glass acts. A held UP or DOWN whose press only rang steps at `kMotionKeyRepeatDelayMs`.
- **An open overlay holds the screen under it, by DESIGN.md's rule** — what changes underneath is kept in state and
  drawn when the overlay closes, so the overlay's own refresh is never queued behind the page's. The select sheet, the dialog
  and the keyboard are overlays on the router's stack (APP_ARCHITECTURE §7). The screen under one hears `OnCovered(true)` as the
  first opens and `OnCovered(false)` as the last closes. The hardware-test screens that update themselves (Sensors, Battery
  and power) pause their timer meanwhile and, uncovered, read at once (`lv_timer_ready`).
- **Which refresh a frame gets is the display's policy** — a refresh by the controller's own waveform, or, on the
  SSD1677, scans for a small change. Scans never count toward `kRefreshPartialLimit`, and after 150 scan frames the next
  pause of 2 s gets the full refresh ([STICKY_EPAPER_WAVEFORMS §3.4](STICKY_EPAPER_WAVEFORMS.md#34-which-refresh-a-frame-gets)).

## 6. The token header

- **One C++ header carries every token** — `components/design_tokens/include/design_tokens.h`, written by hand, named
  in `TOKEN_FILES` in `.agents/project.py`, and held equal to DESIGN.md's front matter by `tests/host/token_parity.py`
  (§7). The check catches the one-sided change a generator would prevent — a value changed on one side alone fails
  the host tests — and leaves the header's comments, its derived symbols and its `static_assert`s where a person
  writes them.
- **A symbol in the header that is no front-matter token is in the check's `HEADER_ONLY` list, with its reason.** They
  are the elevation struct's `kNoShadow`, the derived `kFocusRingReach` and `detail::kScale`, and the touch thresholds
  (§10). The rest are the refresh policy's `kRefreshPartialLimit` and `kFeedbackTickMs`, and a stepped line's counts, `kMarqueePasses` and `kMarqueeGapSpaces`.
  §5's names for three motion delays (`kWaitIndicatorDelayMs`,
  `kSelfUpdateMinMs`, `kClockTickMs`) are in its `ALIASES` list, each checked equal to the token symbol it names. An
  entry the header no longer declares, or that a front-matter token now names, fails as stale. Any other symbol needs
  its token in DESIGN.md first.
- **The arithmetic between tokens is checked as the header compiles** — `static_assert`s at its foot:
  - two rings never touch (`kSpacingItem` ≥ twice `kFocusRingReach`, 8);
  - the footer holds its buttons' rings;
  - a list row is at least a target;
  - a flick's movement is inside its distance;
  - every type size is a multiple of 10 px but 26, and every line to 80 px a multiple of 4 px;
  - every sound is in the buzzer's band, the tick at most 40 ms and the others at most 600 ms.

  `display.cpp` checks that the content and its edges fill the glass's 480 px. A value changed alone that breaks one fails the build. The
  type check reads the header's own list of primitives, `detail::kScale`, so a new primitive joins it.
- **A token's path becomes its symbol**: `colors.text-primary` → `kColorTextPrimary`, `spacing.target` →
  `kSpacingTarget`, `spacing.stroke.focus` → `kSpacingStrokeFocus`, `rounded.sm` → `kRoundedSm`.
- **A colour is stored as its level, 0 to 3**, with the hex in a comment — `kColorTextSecondary = 1;  // #555555`; the
  two sparse dithers are levels 4 and 5 (§2.1).
- **A motion token is one symbol holding its delay, the time the code waits** — `kMotionWaitIndicatorDelayMs`, or
  `kMotionStateChangeMs` where the delay is 0. Nothing animates, so every duration is 0 ms and every timing
  `steps(1, start)`, and a token that changes either needs its symbol decided first.
- **A sound's limits carry their unit and its words are arrays of notes** — `sound.band-low` → `kSoundBandLowHz`,
  `sound.tick-max` → `kSoundTickMaxMs`, `sound.tick` → `kSoundTick[]`, each note `{wave, hz, hz_end, ms, volume}`.
- **A type token is a struct of its size, its line box and its weight**, a role being one primitive by name;
  `ui::Font(role)` gives the compiled font LVGL draws it in, and `ui::Bitcount(size, weight)` any primitive's (§3).
- **A component reads the header through two layers, as the agent deck's do (§9)** — `components/ui` is the theme (fonts,
  styles, `Level`, `Fill`, the one focus ring by `RingOutside`, `RingEditing` and `LetRingOut`), and `components/widgets`
  builds DESIGN.md's kit from it. A widget's sizes are its tokens (`kSpacingTarget`, `kSpacingToggleWidth`, `kSpacingMark`),
  never a literal, so a widget sized for 235 ppi keeps the size the kit gives it in millimetres.

## 7. Enforcement

- **The design check runs on this doc** — `python3 .agents/skills/design-system/scripts/design_check.py --file
  docs/STICKY_DESIGN_TOKENS.md` proves every pair in §2.2 meets its floor on both refreshes, every pair the kinds call
  for exists, and every ratio is true; `verify.py run` runs it when DESIGN.md or this doc changes.
- **Google's linter runs on DESIGN.md** — `npx @google/design.md lint DESIGN.md` checks its token references, its
  sections and their order, and each component's contrast. It fetches the package from npm, so the guard asks first.
- **DESIGN.md's front matter and the token header agree, both ways** — `bash tests/host/run.sh` runs
  `python3 tests/host/token_parity.py` and its `--test`. Every token in the front matter's colours, type, radius,
  spacing, elevation, motion and sound, its refs resolved, has the symbol §6's naming rule gives it, with the same
  value. A colour is compared by its level and its comment's hex, a type token by its size, line and weight, an
  elevation by each field and a sound by each note. A field the header's struct has no place for fails. Every symbol
  in the header is a token, an alias equal to its token, or in its `HEADER_ONLY` list. A mismatch names the token's
  path, both values and the header's line. It does not read the components, which are refs to the tokens it compares,
  a type token's family, or this doc's tables.
- **DESIGN.md's front matter and §2's tables agree by hand** — a script to compare them is open (§8).

## 8. Open items

**Each item below is unverified, with what settles it.**

| Item | Status | Unblocked by |
| --- | --- | --- |
| A script that compares DESIGN.md's front matter with §2's tables | Open | A change to a colour role, which §2's tables would have to follow by hand |
| The glyph ranges beyond ASCII and · ° × – — ’ • … — a name with an accent, a currency — and what a character outside them draws: LVGL's placeholder box is on (`CONFIG_LV_USE_FONT_PLACEHOLDER`) | Open | An out-of-range character drawn on the glass; the first strings that need more |
| The glass's real white and black, against the levels' nominal 21 : 1 | Open | A reading with a light meter on a V1.0 and a V2.0 board |
| The kit port's controls built and not yet seen on the glass: the checkbox and radio rows, the button icons, the horizontal pager, the roller with the Clock screen, the carousel with the Welcome screen, wrapping detail and radio rows, the escape chain's first link and slide-off; and whether a facts page's 432 × 480 ring leaves a ghost on either panel, and whether people press OK on a facts page and wait | Open | The kit page and the hardware test's screens walked on a V1.0 and a V2.0 |
| When a mascot moment's sound lands against the ink's change: the offset from a refresh's start to the ink moving, which a moment waits for (§13) | Open | A bench measurement on a V1.0 and a V2.0 |
| The mascot's music — the typing, the spin, the success, the wave and the thinking — in time with its move under the moments (§13) | Open | Heard on the board |
| The mascot's greys, provisional until the person sees the mascot on the glass (§13) | Open | The demo page seen on a V1.0 and a V2.0 |
| The select sheet with no outline and its chosen row on `band` (§12): whether its partial close leaves a ghost where the dithered row stood, and whether its edge holds over a dithered field, card or chip | Open | The sheet opened and closed on a V1.0 and a V2.0 |
| A reduced-motion setting for Claude's mascot — its branch the move's end pose alone, its moment with it (MASCOT §6) — since the moves run at the glass's pace and only the demo page plays them; the widget's `lv_anim_start` carries an `a11y-exception` until then | Open | The first screen other than the hardware test's demo page that shows the mascot moving |

## 9. The ring outside and the ported widgets

- **The agent deck** is the author's other firmware — a 480 × 320 landscape status panel for coding agents on a 166 ppi
  glass — whose UI kit Agent Jam Sticky ports. This doc owns the ported sizes, and the rest link here. Its source is the
  separate `claude-agent-deck-3` repository, which this repo does not hold and a reader of it cannot open. The author
  read `components/widgets/` (each widget's code), `components/theme/` (its tokens) and `components/icons/source/` (the
  artwork) on 2026-11-20, with the author's own reference pictures of every widget from the deck's
  simulator, kept on the author's machine. A widget is ported by its shapes, proportions and
  layout, never redrawn. Its sizes are the deck's px × 1.416 (235 / 166), rounded to a token. Its tones map to black,
  white and the light dither in the order of their strength, and its motion is left out. Every number in the table below is `[DECK]` in its
  second column and `[D]` in the third and fourth.
- **The one focus ring is outside every control**, `spacing.focus-offset` clear of its outer edge — its outline
  included — with white between, the outline's radius the control's plus its pad and width (LVGL's `outline_pad`).
  So its outer corner is 20 round a `rounded.lg` control, its inner edge 16, and 8 round a square one `[HOST]`.
  The controls are a button, an icon button, a chip, a row, a card, a text view, a toggle, a checkbox, a radio and a
  range slider, black on the white glass, whether the control is chosen or not. It reaches 8 px, so stops sit
  `spacing.md` apart and each container above one lets it out (`ui::LetRingOut`, an `ext_draw_size` of the larger reach)
  or pads it. The list view's band is 8 px wider and taller than its rows all round, which is the ring's reach. The
  footer's 16 px over and under its buttons keep it 8 px from the glass's edge `[D]`. A control open for editing — a
  range slider, a roller, a text view being read — draws it at `spacing.stroke.edit`, 6 px. It then reaches 10 px, 14 px
  from the glass's edge on a 24 px margin, and a name 16 px above a slider leaves 6 px of line box clear of it, and its glyphs more `[D]`. A segment's ring is round the focused item inside its track. The track's `spacing.sm` inset is the ring's
  reach (4 + 4), so the ring ends on the track's edge and a neighbour's gap, 8 px, holds it. The track's `rounded.xl` 16
  is concentric with the item's `rounded.md` 8 and the ring `[D]`. A chosen control keeps the black ring, with 4 px of white between.
- **The screen audit holds every ring whole and alone** — `tests/host/screen_audit_lvgl_test.cpp` focuses each stop of
  every screen and overlay as a key does and measures two things, BYTE-90's `ui_probe` `RingClip` and `RingCount`.
  - **The focus ring cut off, range 0.** The ring's box is the control's, grown by its outline's width and pad in its
    focused state, and in its editing state where `ui::RingEditing` styles one. The audit counts the px the box loses
    on each side to the glass and to each ancestor. An ancestor lets its children draw in its own box. With
    `LV_OBJ_FLAG_OVERFLOW_VISIBLE` it lets them draw in its box grown by its `ext_draw_size`, as LVGL clips them, so a
    container set overflow visible without `ui::LetRingOut`'s reach still cuts. Two things are not measured. One is a
    sibling or an overlay drawn over the ring: the status bar or the footer over scrolled content, an overlay over a
    lower screen. The other is an ancestor's `clip_corner`, which rounds the cut.
  - **The rings drawn at once, range 1** — the objects in a focused state whose outline is drawn.
  - **A fixture runs first, and the audit stops if it misreads.** Each case is read by the measure and drawn so on the
    glass. A button in a clipping box reads its ring cut 8 px at the bottom, 5 on the left, 8 at the top and 8 on the
    right. The same button in a box `ui::LetRingOut` lets out reads none. In a box set overflow visible with no
    `ext_draw_size` it still reads 8. A button open for editing, its ring 10 px out, reads 7 on the right. Two buttons
    ringed read two rings, and one alone reads one.
  - **Each ringed frame is kept in `build/host/audit/rings/`**, named by screen and stop, so a refactor can be proven
    pixel for pixel with `cmp`. No stop on any screen reads a ring cut or a second ring `[HOST]`.
- **The deck's one base radius, 10, is `rounded.lg` 12 everywhere it appears** — buttons, chips, rows, cards, a text
  view's ring, a range slider's track. Its small radius 5, a checkbox's mark, is `rounded.md` 8. Its pills — the
  toggle, the progress and step pieces, the scroll indicator's, the page dots, the radio mark — are `rounded.full`.
- **Every ported widget, the deck's number → scaled ×1.416 → Agent Jam Sticky:**

| Widget | Deck px `[DECK]` | Scaled `[D]` | Agent Jam Sticky px | Token |
| --- | --- | --- | --- | --- |
| Toggle | track 66 × 30, thumb 36 × 24 inset 3 | 93 × 42, 51 × 34, 4 | 96 × 44, 52 × 36 inset 4 | `toggle-width`, `toggle-height`, `toggle-thumb-width`, `xs` |
| Segment | track 252 × 48 → 357 × 68, items inset and apart 6 | 8.5 | 64 high, items 48, 8 round and between | `target`, `sm`, `rounded.md` / `xl` |
| Range slider | 36 high, fill 30, knob 6 × 24 | 51, 42, 8.5 × 34 | 52, 44, 8 × 36 | `slider-height`, `xs`, `sm` |
| Standard button | 150 × 44, edge 2 reserved on every variant, words 12 in | 212 × 62, 2.8, 17 | 212 × 64, edge 3, padding 16 | `button-width`, `button-back` (Back in words, 160), `target`, `stroke.default`, `md` |
| Chip, compact button | 48 high, pad 18, edge 3 (chosen) | 68, 25, 4.2 | 64 high, pad 24 (a compact button 24 and its 3 px edge), edge 4 on a chip | `target`, `lg`, `stroke.strong` |
| Progress bar | stroke 9 (5-hour 12), gap 6 | 13, 17, 8.5 | 12 (16), gap 8 | `progress`, `progress-lead`, `sm` |
| Step bar | stroke 9, gap 8 | 13, 11 | 12, gap 12 | `progress`, `step-gap` |
| Page dots | dot 6, current 20 × 6, gap 6 | 8.5, 28 | 8, 28 × 8, gap 8 | `page-dot`, `page-pill`, `sm` |
| Value row | 36 (48 in a list), words 12 in | 51 / 68, 17 | 64, words 16 in | `target`, `md` |
| Detail row | name 16 over value 18, 6 pad, 55 | 78 | 84 (8 + 32 + 36 + 8) | `sm`, `caption`, `label` |
| Radio row | 60 high, words 14 in, mark 30 (edge 3, dot 18) | 85, 20, 42.5 | 80 at least (4 + 36 n + 4: 80, 80, 116, 152 for one to four lines), 20, 42 (edge 4, dot 26) | `list-row-height`, `card-inset`, `mark`, `mark-dot` |
| Choice card | min mark + 24, words 14 in, gap 12 | 76, 20, 17 | 74 (`mark` + 2 × `md`; 100 with a `label` name over a one-line detail, 36 + 32 + 2 × `md`), 20, gap 16 | `mark`, `card-inset`, `md` |
| Roller | — (the person's words: a stepped picker, Agent Jam Sticky's own) | — | three rows of 64, as wide as its widest value and 24 either side (84 for two figures), outline 2, open band 72 × 64 | `target`, `lg`, `stroke.hairline`, `xs` |
| Carousel | — (BYTE-90's carousel slid its cards; Agent Jam Sticky turns them in place) | — | a card 432 wide filling the height, footer slots 133 wide and 16 apart | `content`, `card-inset`, `stroke.default`, `md` |
| Icon button | tile 54, glyph 30 (56 %) | 76, 42 | 64 (the hit-area floor, drawn and hit alike), glyph art 40 (62 %) | `target`, `icon` |
| Glyph beside words | icon 24, gap 9 | 34, 13 | art 40 (ink 18 to 33), 16 of white from its ink to the first letter | `icon`, `md` |
| Checkbox, radio | — (the person's words: like the switch) | — | 44, `rounded.md` or full, edge 3 off, dot 16 | `check-size`, `check-dot`, `stroke.default` |
| Scroll indicator | lane 6 × 176, gap 6, 7 from the glass | 8.5 × 249 | 8 × 248, gap 8, x 467 | `scroll-thumb`, `scroll-length`, `scroll-x`, `sm` |
| Status block | mark 60, column gap 16, heading 22 over 16 | 85, 23 | mark drawn 85 tall, compiled at most 73 wide, flush left in a slot of 73, 24 from the slot to its words, so every block's words stand at x 121 (`edge` + 73 + `lg`); a block with no mark leaves the slot empty; heading `label` 30 over its description in `body` 30 on 40, `pair-md` apart | `status-mark`, `status-mark-slot`, `lg`, `pair-md` |

  The deck's type sizes — 16, 18, 22 — are `caption` 26, `label` 30, `label` 30: the scale has 26 and then tens. Its names are
  `label_xs` and `label_sm`; its medium and bold are weights 600 and 700.
- **A name over a detail stands on line boxes.** That covers a toggle row's name over its reason, a status block's heading over
  its line, a home row's title, a detail row and a choice card. `ui::FitLineBox` pads a label above and below by its role's pitch
  less the compiled font's height, half each with the odd pixel above. A one-line `label` is then 36 px, a `caption` 32, and a
  block of n lines n pitches. A dotted label is one line box tall.
  A detail row's name and value and a radio row's words wrap on the same boxes. `widgets::detail::WrapWords`, shared with the
  choice card, takes no colour of its own, so a disabled row's `text-disabled` reaches it.
  A radio row is 4 + 36 n + 4 high, at least 80: 80, 80, 116 and 152 for one to four lines. A detail row is 8 + 32 a + 36 b + 8.
  The host render drew every wrapped frame pixel for pixel as the draft did `[HOST]`.
  `widgets::detail::Line` and `Words` and `widgets::MakeListRow` use it. `tests/host/fit_line_box_lvgl_test.cpp` checks it on LVGL's real
  label code for `label`, `caption` and `caption-loose`, with odd and even spare pixels, dotted and wrapped `[D]`.
- **A pair's gap is the gap between the two texts' own boxes.** The line boxes above add leading to each text, so a pair is glued
  with `widgets::detail::Glue` (`internal/glue.h`). It takes the leading back as a negative margin, and the column's `pad_row` is
  then the gap between the font boxes. The dialog's title stands `pair-xl` over its message, and a status block's title `pair-md`
  over its description. `tests/host/section_gaps_lvgl_test.cpp` measures both, the heading's gaps, and the status block's
  words at one x and its marks within `status-mark-slot` `[D]`.
- **The white between two texts** is `pair-sm`, 8 px, between a name's and its line's own boxes, or the two line boxes' leading
  where that is more. At 26 px on a 32 px line the leading is 8, `pair-sm` itself; a `caption-regular` line's font is shorter in
  its box, so it stands 10 under a `caption-strong` name. The host tests check each pair against the two `[HOST]`. A status block's
  heading and its line stand 22 apart. A toggle row's or a checkbox row's reason stands so under its name only while it shows. The
  row grows to hold it — 100 px for a toggle row whose reason wraps to two lines, 68 for a checkbox row's one — and is 64 without it.
- **Heights on the host render** `[HOST]`: a row is 80 and a toggle row 64, its name, state word and toggle centred in it, the
  toggle 10 px above and below. A choice card is 132 with a name over two detail lines (16 + 36 + 2 × 32 + 16) and 172 with a
  long name wrapped over its detail. A detail row is 84 (8 + 32 + 36 + 8). A status block's text is 119, a heading over two `body` lines
  (36 − 5 + 16 + 80 − 8), at 432 px wide with or without a mark. A free label on a screen, `ui::NewLabel` alone, stays the font's
  height.
- **The scroll indicator is the deck's lane**, a `spacing.scroll-thumb` wide stroke of separate round-capped pieces: a
  short track piece in the light dither, `spacing.sm` of white, the black thumb, `spacing.sm` of white, the rest of the
  track. It is `spacing.scroll-length` long at most, its left edge `spacing.scroll-x` on the glass. That is 1 px clear of
  the 6 px editing ring round a full-width view (24 + 432 = 456, + 4 + 6 = 466), and 5 px from the glass's edge. The thumb
  (`surface-on`) is the view's share of the length, never under a dot. Where it stands is DESIGN → Components → The kit's rule.
  The hold at the ends is the deck's `draw()` `[DECK]`, 16 px here (a gap and a dot), and the even spread starts past 15
  pages at 248 px. `widgets::detail::ThumbAt` (`src/internal/scroll_rules.h`) is the rule, and
  `tests/host/scroll_test.cpp` checks it for 1 to 60 pages `[D]`. Everything fitting hides the indicator.
  **Where it is centred:** on the glass's height (400), the same on every page, for a list that fills the content column,
  Home's. For a view in a page that holds more, the kit page's, it is centred on the view itself, where the glass's
  middle is not the view's. `widgets::IndicatorCentre` says which. The lane stands 19 px past its host's right edge, more than a ring reaches, so
  the host's `ext_draw_size` is widened to it: LVGL clips a child to its parent's area plus that size, and the lane would
  not draw at all `[D]`.
- **The three status marks and the choice card's check are the deck's artwork at one bit** — `components/widgets/marks/make_marks.py` box-averages
  the 128 × 128 art, thresholds it at half and packs it as LVGL's A1, so a mark draws solid in the ring or the box that
  holds it. A check is 26 × 18 in a checkbox's mark. The status block's check, clock and link-off are 57 × 41, 73 × 73
  and 73 × 68. Agent Jam Sticky has no icon set. The person decided on the status block's three marks. The choice
  card's check is a fourth, and these four are the widgets component's alone, each beside its word (DESIGN →
  Components → Icons).
- **The sixteen glyphs are BYTE-90's art at one bit** — the ten below on the 40 px art box, and six more cut to a box each from the
  same PNGs (`make_marks.py`'s `crop`, no trim, so the three signal glyphs register over one another). The signal comes
  in three strengths, 40 × 30 each, cut from their shared ink box (3, 21) to (125, 111) of the 128 canvas. The padlock
  is 19 × 24, the check for a row's facts line 28 × 20, and the dropdown's chevron 14 × 28. Each equals the approved
  draft's pixels byte for byte `[D]`. Every stroke is 3 to 4 px (the padlock 2 to 3), none under `spacing.stroke.hairline`. The ten:
- **The ten glyphs are BYTE-90's art at one bit on the 40 px art box** (`spacing.icon`), made by the same script from
  `byte90-series3/assets/icons` (a second repository this repo does not hold). The ink sizes are an arrow 27 × 26, a
  cross 22 × 22, play 18 × 22, pause 24 × 26, stop 22 × 22, the speaker 32 × 28 and with a slash 33 × 34, a check 26 × 20,
  the microphone 24 × 33 and a refresh 28 × 28. They are `Glyph` and `IconAction` in `include/widgets/glyph.h`. A glyph
  outside the closed set of DESIGN → Exceptions → The icon button does not compile as an `IconAction`. Every ink pixel but a few diagonal
  join pixels is in a 2 × 2 block, so no stroke is under `spacing.stroke.hairline` `[D]`. A button's glyph is a child
  image recoloured to its words' colour (`detail::NewMark`), and from the disabled state it takes `text-disabled`, which
  the button passes down (`LV_OBJ_FLAG_STATE_TRICKLE`). The ink to the first letter is 15 px of white on the host render, the
  font's glyph offset taking one of the 16 `[HOST]`. A half button holds 7 characters beside a glyph (28 + 16 + 7 × 18 = 170)
  and `Refresh`, the widest, fits.
- **A disabled control is one rule built in one place** — `detail::DitherWhenDisabled` (`src/internal/base.cpp`): the face
  the light dither, the reserved edge transparent (its width kept, so nothing moves) and the words `text-disabled`, which a
  child label inherits. A rim object (`DisabledRim`) is gone. The black parts are white parts knocked out of the face: a
  toggle's thumb on either end, a chosen segment item, a slider's fill. What stands on a white part is
  `text-disabled-page`. These are a chosen segment item's word, and a slider's knob and its value while it fits in the fill (on
  the track it is `text-disabled`). As a build decision the draft left open, a radio row's or choice card's dot or
  check on its white mark is the same, since the mark has no edge. A row's own name and state word are `text-disabled-page` (a setting row sets each
  word's colour itself, since a label's own colour outranks its parent's disabled style). The reason stays solid `caption`.
- **A focused control comes into view at once, by the theme (`ui::ShowWhenFocused`)** — LVGL's own scroll on focus
  animates, which the glass would draw frame by frame. So a tappable `widgets::MakeListRow` and every widget's `JoinGroup` take
  `LV_OBJ_FLAG_SCROLL_ON_FOCUS` off and scroll the pages round the control without animation. The nearest page that
  scrolls then moves a little further, so the ring (4 + 6 px, open for editing) is in view too. A list view's rows
  name their list, so the page behind brings the whole list into view and the list turns its own pages: a row never
  scrolls the band `[D]`.
- **BYTE-90's network row, dialog, dropdown and pager are built from the approved drafts.** Each is built from the repo's own rows
  and buttons, in tokens.
  The **network row** (`widgets::MakeNetworkRow`) is the home row's shell: 80 px, `rounded.lg`, 16 either side and 8 above and
  below. Its name is `caption`, 344 px wide: the row's 400 inside its padding, less the 40 px signal glyph and a 16 px gap. That
  holds 22 characters at 26 px, then three stops or a stepped line. The facts line is `caption-regular`, 32 px high, with `spacing.sm` between a glyph and its word
  and `spacing.lg` before the second fact. The signal glyph is a 40 × 30 canvas, alone and with no word (DESIGN → Exceptions → The
  signal glyph without a word), centred on the row's height and floating at the right end. The unlit bars are the strongest glyph
  recoloured `ui::Fill(band)`, the dark dither on the inverted row, under the lit glyph on the same canvas.
  On a V1.0 and a V2.0 the unlit bars read as faint grey strokes, so the 2 px outline is not needed `[SEEN]`.
  The **dialog** (`MakeDialog`) has outline 4, `rounded.md` and `spacing.lg` inside. Its title is `label` and its message `body`.
  The title stands `pair-xl` (32) over the message between their own boxes, and `group` (24) over the buttons. The two half buttons
  are 180 px wide and `spacing.item` apart.
  The **dropdown** (`MakeDropdown`) is a value row with the chevron `spacing.md` after the value. While it is disabled, its reason
  stands under it a page gap.
  The **horizontal pager** (`MakeHorizontalPager`) is a band 448 × 80 round a strip of 64 px items. A page holds whole items to
  432 px, and the next page starts a full view (448 px) on, so no item peeks at the edge. "1 of 2" stands alone in `caption`,
  `text-secondary`, 26 px, left-aligned at the strip's left edge and 16 px under the chips, with no page dots. Its host test checks
  the words, their place and that nothing else stands there `[HOST]`. The host renders beside each draft are in the task's
  scratchpad, not the repo.
- **The roller is built from the pure rules in `roller_rules.h`** (`widgets::MakeRoller`, `MakeNumberRoller`; its behaviour is DESIGN → Roller).
  It has a white face `rounded.lg` 84 × 192 for two figures and three labels each centred in its 64 px row. The hairline
  outline is a floating child over the face, so the rows are placed from its top. The open band, `surface-chosen`,
  `rounded.md`, 72 × 64 at x 6, stands behind the middle row while the group edits. A step changes three labels' words and nothing else, 232 px on the host render `[HOST]`. Mechanics that a
  change must keep: a press does not focus it (`LV_OBJ_FLAG_CLICK_FOCUSABLE` is off, since LVGL's click focus ends an edit even on the control
  already focused, which would close it under a tap on a row). Its arrow scroll is off (`LV_OBJ_FLAG_SCROLL_WITH_ARROW`, which would
  scroll the rows a quarter of their height on every step). A transparent part wider than the roller gives the swipe's claim somewhere
  to scroll. The group's `LV_STATE_EDITED` is the one truth that opens it, shows the band and the footer's bar, and closing it reports one
  change when the value is not the one it opened on. The footer's bar (`MakeRollerFooter`) is bound with `SetRollerFooter`. It hides only
  the footer's parts that are showing at the time, shows those again, and forgets a roller that is deleted while open. `tests/host/roller_rules_test.cpp`
  and `roller_lvgl_test.cpp` hold the rules and the widget on LVGL's real input, and the Clock screen's set (`hw_test/clock_rules.h`) `[HOST]`.
- **The carousel is one card turned in place** (`widgets::MakeCarousel`; its behaviour is DESIGN → Carousel): the card is 432 × 588 on the
  Welcome screen, the content column's width and the rest of its height. Its words stand 20 px in from the card's edge (17 inside the outline).
  Under it "2 of 4" in `caption` and the page dots `spacing.md` after, and the footer's three slots, each 133 wide and `spacing.md` apart, at
  x 24, 173 and 322 — Back in a slot of its own, so hiding it on the first card leaves it empty. A turn sets the card's two labels, the words,
  the dots, Next's label and Back's visibility in one LVGL frame, so a V2.0 draws one partial. Hiding Back hands the ring to Next
  (`router::SetHiddenHandingFocus`). The swipe is the pages' horizontal claim on the card, which has an invisible part wider than itself for
  the drag to go to. Holding OK leaves through `router::AttachEscape`, which the screen's `OnCreate` calls and the router runs after the
  overlays and before going back, so it works at the root. The host render of the built screen is pixel for pixel the draft redrawn with Skip
  outlined `[HOST]`. `tests/host/carousel_lvgl_test.cpp` drives it with a scripted finger and the buttons, `carousel_escape_lvgl_test.cpp` holds OK at
  depth 1 and 2 `[HOST]`. The hardware test's "Welcome" screen shows it with four cards of illustrative copy.
- **A row's content is centred on its height** — one line or more (DESIGN → Components → The kit). LVGL sizes a flex
  track to its tallest child, so a row's `min_height` alone pads below its content. The pattern that holds is a content
  column that takes the row's height and centres its lines (`setting_row.cpp`, `select_sheet.cpp`). A row with a fixed
  height centres its track (`value_row.cpp`, `widgets::MakeListRow`). `tests/host/row_centring_lvgl_test.cpp` builds
  every row kind on LVGL's real layout and fonts, with one line of content and with more. It asserts the content
  block's centre is within 1 px of the row's for toggle, toggle icon, checkbox and radio button rows (one line, two,
  disabled with a reason), value rows, the dropdown, home rows and radio rows. It does the same for choice cards, radio
  rows of one to four lines, detail rows of one to three, a list view's band cut to its page's rows, network rows (in a
  list view too) and the select sheet's options (one line, two, three) `[HOST]`. Every row measured an offset of 0 px on
  2026-11-21, and an option row whose column lost its minimum height measured −12 px — the test sees it.
- **A page with no row in the focus group has a stop of its own** (`list_view.cpp`'s `BuildStops`, called by `UpdateListView`; DESIGN →
  Components → The kit). The stop is a `detail::Bare` child of the list's frame, not of the band, so a row's index is still its place in
  the pages. It has `rounded.lg` corners and `ui::RingOutside`, and is joined to the group by `detail::JoinGroup(stop, frame)`,
  so the page brings the view and the ring into view as a row's does. It draws nothing at rest. It is never hidden, since
  LVGL's focus step skips a hidden object. While its page shows, it has the box of the page's rows (`ShowStops`, from the
  first row's top to the last row's bottom, in the frame's own coordinates, 432 × 480 for five facts of the About
  screen's and 432 × 352 for four). Every other stop is 0 × 0, so none of it is ever on the glass and the router's
  "first control in view" never lands on it. Its `LV_EVENT_FOCUSED` turns to its page
  (`OnStopFocused`), as a row's does. It takes no touch (`Bare` clears `LV_OBJ_FLAG_CLICKABLE`: a swipe goes to the band as before), OK sends
  it a click nothing hears, and `LV_OBJ_FLAG_SCROLL_WITH_ARROW` is off on it and on the band. LVGL has a swap and no insert, so
  `PlaceStop` adds it at the end of the group and swaps it down to its page's place: before the first row of a later page
  that takes focus, else after the last row or stop before it. A second `UpdateListView` makes the stops anew. The old
  ones leave `State::stops` before they are deleted, since deleting a focused one refocuses another whose handler reads
  it. A list of facts only puts them back where its first old stop stood. The first call of such a list has nothing to
  stand by and adds them at the end, so it is made before the footer's Back. A swipe moves the focus from a row or stop
  of the list to the new page's first row that takes focus, or its stop (`FocusOnPage`). The option `PageWords::kUnder` adds a
  `caption` `text-secondary` label to the frame at `spacing.md` under the view's whole height and makes the frame that
  line taller, only while there is more than one page. The field `ListView::words` is nullptr when not asked. The About
  screen's host render beside the draft is pixel for pixel the same on all fifteen frames `[HOST]`. The real About,
  twelve facts, is two pages of six, 536 px each. `tests/host/facts_page_lvgl_test.cpp` walks a facts screen of two
  pages and a mixed list of three by UP and DOWN, round the wrap. It checks the box, the ring's width, reach and
  colour against a button's, OK doing nothing, the words, that LVGL's arrow scroll stays off (the same key does scroll
  the band with the flag put back), and a second `UpdateListView` after the footer and with a stop focused.
- **A radio row's and choice card's group is the siblings that share a parent** (`choice_card.cpp`'s `detail::MakeChoice`, shared by
  `radio_row.cpp`): the one `LV_EVENT_CLICKED` listener, added first so a caller's own sees the group settled, carries a tag as its
  user data, one-of-a-set or any-number. A sibling is told by finding that listener, so the object's own user data stays the
  caller's. It chooses it: a disabled one is ignored, the chosen
  one sends nothing, another clears the group's other tagged children and then sends `LV_EVENT_VALUE_CHANGED` on itself. A card of any
  number flips and sends each time. `SetRadioChosen` and `SetChoiceChosen` set the state, clearing the group when they choose, and send
  nothing. The hardware test's hand-clearing of both is gone. Its Reminders screen listens for the change and says the choice in the
  status line (`router::Say`). `tests/host/choice_group_lvgl_test.cpp` drives OK and a tap through the real input.
- **The kit page shows every widget on the glass** (`UI kit` on the hardware-test home), each with a disabled sample where
  it can be disabled:
  - buttons, buttons with a glyph, icon buttons, toggle rows, toggle icon rows, checkbox rows, radio button rows, rows,
    chips, segments;
  - progress, step and page dots, radio rows, choice cards, sliders;
  - a paged list view and text view, Wi-Fi network rows, the failed-join dialog;
  - dropdowns that open the select sheet (three options, eight with a long one, and a disabled one);
  - rollers (Count stops, Hour goes round, with the footer's Cancel while one is open);
  - a horizontal pager of chips (the carousel has its own Welcome screen);
  - the keyboard opened from two buttons, and status blocks.
- **The kit page turns by whole-section pages** (`hw_test.cpp`: `KitPageColumn`, `KitUnits`, `PageKit`; DESIGN → Layout →
  Paging). Its blocks stand in a column inside a band a ring's reach (10 px, the open ring) wider and taller all round. A
  page is the column moved up to the page's top and the band cut at its bottom, so the content keeps its height and Back
  its place. The units: a section whole when it fits the content's 628 px; else its groups, split at their names
  (`KitName`); else its blocks, a block too tall split into the blocks inside it. The heading always joins what follows
  it, the first block cut to the room the heading leaves. 28 pages. The walk turns to the page that holds the focused
  control (`KitPageOf`: its highest ancestor that fits one page, so a list view's rows stay the view's). A swipe begun
  anywhere turns one page (`widgets::ClaimPageSwipe`), `sound.at-end` past either end, a ring the new page hides moved
  to its first control. A page with nothing to press gets a stop: an unseen box over its blocks with the ring round
  them, `rounded.lg`, in the walk between its neighbours' controls (Status blocks and Stacked status blocks).
  `tests/host/screen_audit_lvgl_test.cpp` measures each page as a view, and counts the column as the page, as it counts
  a scrolling column `[HOST]`.

  `tests/` has no check of the widgets, only of the dither's words (§4) `[D]`. The host render of
  each beside the draft is the gate-2 picture.

## 10. Paging and the swipe

### 10.1 Pages

- **A list's pages are whole rows that fit its view** (`list_view.cpp`'s `Repage`): the band's view is `rows_per_page` rows of the
  nominal height and the gaps between them. Each page takes the rows that fit it, the next page starting with the next row.
  Rows of the nominal height make `rows_per_page` of them a page, as always. A row that grows with its words (a select sheet's
  third line) takes the room of more than one and a page holds fewer. The room under the last page's last row is what its first
  row's top and the view leave, so every page is as tall as the first.
- **A horizontal pager's pages are whole items that fit `spacing.content`** (`horizontal_pager.cpp`): the band is
  `spacing.content` + 2 × `spacing.sm` wide. Each page starts a band's width after the last one's start (a margin on its
  first item), so no item of the next page shows at the edge. A turn scrolls the band to `page` × 448 px at once.

- **A list view and a text view hold whole pages and turn them whole** (DESIGN → Layout → Paging): the band is a
  scrollable LVGL object with room added under its last page, so every page is as tall as the first and nothing ever
  scrolls back to fill the glass. A page turn is a jump of the band's scroll to the page's first row or line, drawn in
  the next refresh. A list's page is `rows_per_page` rows, `spacing.md` apart. A text view's is its `lines` of
  `caption-loose` less one, the last line of a page the first of the next.
- **A text view's words stand `spacing.sm` inside its view on every side.** The view is `lines` × 30 + 2 × 8 px tall and
  the full 432 px wide. The band that holds and clips the words is the inside of that, 416 px wide and `lines` lines
  tall, at an 8 px inset. A band that is the view itself would show a part of the next line in the inset. The band is
  the focus stop and draws the ring `spacing.focus-offset` + 8 px out from its own edge (`ui::RingOutside`'s `inset`), its
  corners `rounded.lg` less 8. So the ring stands where it would round the view, the focus ring's radius and weight
  unchanged: 4 px, or 6 px open for reading. The first glyph is 8 px inside the view, 12 px from the ring's inner edge and
  16 px from its outer edge when focused (18 open) `[D]`. At 416 px of 12 px characters a line holds 34 characters, and
  the content's 660 px hold 21 lines of 30 px with the two insets, 20 new lines to a page. The view's box and the ring
  beside it are brought into view as one (`ui::ShowWhenFocused`'s `show`), and the scroll indicator stands 1 px clear of
  the 6 px ring `[D]`.

- **A horizontal swipe is the same decision with the axis swapped** — `ClaimSwipe(band, turn, SwipeAxis::kHorizontal)` reads the
  band's `scroll_x`: a finger moving left is positive, so it turns forward, and the pure rule in `swipe_rules.h` is unchanged and
  tested by the same `swipe_test.cpp`. Only the band's own axis is taken (`LV_OBJ_FLAG_SCROLL_CHAIN_HOR` off): a vertical drag that
  starts on the strip chains to the page behind, which is the page's.

### 10.2 The swipe's decision

- **A swipe is read on lift, by `widgets::detail::ClaimSwipe`** — the band is made the object a vertical drag claims, so
  the page behind never scrolls by a pixel (no scroll chaining, no momentum, no gesture bubbling; the touch check's
  `touch-exception` marks that line). LVGL reads a drag after its own `LV_INDEV_DEF_SCROLL_LIMIT`, 10 px. Each move is
  logged with its time and the net travel so far, finger up positive, and the band is put back where the page keeps it at
  once, so nothing moves under the finger and the page lands in one refresh after the lift. LVGL's `LV_EVENT_GESTURE`
  is not used: it does not fire while another object scrolls, as the type scale page does.
- **The decision is `PageTurn` in `components/widgets/src/internal/swipe_rules.h`**, a pure function tested on the computer
  by `tests/host/swipe_test.cpp` (`bash tests/host/run.sh`). The thresholds are in the token header and hold
  under a finger on a V1.0 and a V2.0 `[SEEN]`:

  | Rule | Value | Token |
  | --- | --- | --- |
  | A swipe commits by distance | the net travel is at least 64 px, one target, however slowly | `kTouchSwipeDistance`, from `spacing.target` |
  | or by a flick | the net travel is at least 24 px **and** the finger moved at least 12 px the same way in the last 100 ms before the lift | `kTouchFlickDistance`, `kTouchFlickMovement`, `kTouchFlickWindowMs` |
  | The window slides | it is the movement between the position 100 ms before the lift and the position at it, from a ring of the latest 32 moves (time, net travel); a move exactly 100 ms old is inside | `Drag`, `MovedWithin` |
  | A finger that rested places | nothing moved in the window, so there is no flick, however fast the drag was before it | |
  | A pull-back places | the net travel counts moves back, so a drag pulled back under 64 px turns nothing, and a flick the other way is not this swipe's | |
  | One swipe, one page | however far or fast; up is +1, the next page, and down is -1 | |

### 10.3 The lift, the ends and who owns a drag

- **The lift is `LV_EVENT_SCROLL_THROW_BEGIN`, not the end of the scroll** — LVGL sends it to the scrolled band as the
  finger leaves, inside the touch read, so the window ends at the lift. `LV_EVENT_SCROLL_END` comes later, from the throw's
  animation callback, after whatever the task is doing, such as a refresh of a second or more. It would read a flick as
  a finger that rested. The drag's moves are read only while `lv_indev_active()` holds this band as its scroll object.
  So a key's or a focus's scroll, and the scroll LVGL makes from that animation callback, where there is no active input
  device, are never a drag `[D]`.
- **The swipe's own scrolls are left alone** — the put-back under the finger and the page turn are made with
  `lv_obj_scroll_to_y`, which sends the band `LV_EVENT_SCROLL_BEGIN`, `LV_EVENT_SCROLL` and `LV_EVENT_SCROLL_END`, the
  same events a finger's drag makes, while the finger still holds the band. A `settling` flag, set around each of them,
  has the three handlers return at once. Without it the put-back begins a drag, which puts the band back, and so on until
  the stack runs out. `tests/host/swipe_lvgl_test.cpp` drives LVGL's real input and scroll code with a scripted finger and
  checks it. Scroll events are never more than two deep. A 64 px drag (a finger's 73: LVGL's scroll limit keeps 9 of its
  first px) turns exactly one page and 63 does not. The page stays turned, the next swipe starts from it, a flick turns,
  and a scroll with no finger is left where it went (`bash tests/host/run.sh` builds LVGL from `managed_components` once,
  into `build/host/lvgl`) `[D]`.
- **A drag that began on a disabled child is read at the end of the scroll** — LVGL sends no release and no
  `LV_EVENT_SCROLL_THROW_BEGIN` for a disabled object, only the `LV_EVENT_SCROLL_END` its throw ends with, which carries the
  input device (LVGL's own scrolls carry none). The drag is decided there, once. Its distance is exact, and a flick at its
  end may read as a finger that rested if the task is busy since the lift. The test drives it too `[D]`.
- **The ends and the refresh** — at an end nothing turns, nothing wraps and nothing resists (the buttons wrap), and the tick
  sounded at the press is the answer. A swipe made while a refresh holds LVGL's task is read when the task runs again.
  What the glass then does is for the device walk `[?]`.
- **Who owns a drag** — the band owns a vertical drag that starts on it, on a row or between rows: the tap is cancelled
  and the page turns on lift. A slider has no drag of its own: a vertical drag that starts on one is the page's, and the
  slider's release after it sets nothing (below). A horizontal drag on a slider is its value, landing on lift, on the track,
  its hit area or within `spacing.md` of them. A lift farther off is slide-off. The router cancels the press
  (`router::CancelOnSlideOff`, [APP_ARCHITECTURE §7](APP_ARCHITECTURE.md#7-screens-routes-and-focus)), the slider hears `LV_EVENT_PRESS_LOST` and keeps the value it had when the finger went
  down (saved on `LV_EVENT_PRESSED`). `tests/host/slide_off_lvgl_test.cpp` checks both `[HOST]`. The page
  behind a view — the kit page, which turns by whole sections — never takes a drag that starts on a view, and takes any
  other.
- **A slider's release is not a tap after a drag** — a touch that scrolled anything leaves the track's value alone
  (`lv_indev_get_scroll_obj`), so a vertical drag that began on a slider never sets it.

## 11. The keyboard

### 11.1 Pages and keys

- **Pages and keys are data** (`components/widgets/src/internal/keyboard_pages.h`): six rows of six slots on four pages. The pages hold letters in the
  order of the alphabet, capitals, digits with common punctuation (123) and the rest of the punctuation (#+=). Each key is one slot
  or two joined. A row with no keys (the symbols page's fourth) draws nothing and holds no stop.
  A slot is `spacing.target`, 64 px, and the gap `spacing.key-gap`, 12 px: six slots are 444 px across, 18 px from each side of the
  glass, and six rows 444 px down, from y 128 under the status bar's 44 (the field's slot 16 down, the count line 88). The bottom
  row is three keys of two slots each, 140 px: Shift (the word alone, no glyph), 123 (ABC beside the letters; #+= and 123 on the
  symbol pages' first slot) and Space. So each label, at `label`'s 18 px a character, stands centred with 25 px either side
  (Shift, Space) or 43 px (123, ABC, #+=). The three stand in slots 0, 2 and 4 on every page, so a page switch keeps the place.
  `tests/host/keyboard_pages_test.cpp` checks the geometry, the equal bottom row, that every letter and every printable ASCII
  character is on a page, and that no page types one character twice `[HOST]`.
### 11.2 The walk

- **A key is the focus stop, a row is not** (the person's decision: no row level): `widgets::OpenKeyboard` makes each key a
  plain object with `ui::RingOutside`'s one ring, `spacing.stroke.focus` 4 px and `spacing.focus-offset` out, so a key's ring reaches
  8 px and leaves 4 px of the 12 px `spacing.key-gap` clear. No key is ever `LV_STATE_EDITED`, and the group is never put to editing, so
  `input.cpp` sends UP and DOWN as focus moves. `BuildWalk` puts the stops in the group in the order of the glass: the field, its
  button, every key row by row from the left, Cancel, the action. A page switch makes the keys again, so it makes the walk again
  and puts the focus back on the key of the same row at the nearest slot — the place is kept. The old keys first leave the
  keyboard's own group (`lv_group_remove_all_objs` on its overlay's group), so none is deleted while focused.
- **Hold OK** runs the escape chain and never types. What it does is [DESIGN → Focus and input](../DESIGN.md#focus-and-input), and how is the `Escape` row of [APP_ARCHITECTURE §7](APP_ARCHITECTURE.md#7-screens-routes-and-focus). The
  keyboard's part is its overlay `escape`, which cancels at once, with no row to leave, and answers Cancel.
### 11.3 Typing and the field

- **A press types once however long it is held**: the keypad read sends a key on release and a hold goes to the escape chain (§11.2), so a
  hold never repeats a character. A touch on any key types it on `LV_EVENT_CLICKED`, the release on the key. A finger that slides off, past the key's hit area and `spacing.md`, gets the router's slide-off and sends
  `LV_EVENT_PRESS_LOST`, which cancels it (WCAG 2.5.2). The tick still sounds on contact, in the input task. Shift, 123, ABC and #+=
  act the same way. Delete removes one character. A press past `max_length` ticks and types
  nothing.
- **The text is the field component's** (the person's decision): the keyboard keeps none, and calls `KeyboardTarget` — `append`,
  `erase` and `text` — for a character, Delete and what has been typed. A slot `spacing.content` × `spacing.target` stands at
  the field's place for it. `SetKeyboardTarget` sets the target once the field is made in the slot, and `KeyboardTextChanged`
  tells the keyboard the text changed another way (§11.4). The count line under the slot reads the text
  ("3 of 8 characters needed", then "9 characters"). The action is disabled (the one disabled look) until
  `min_length`. It names the count line as its reason with `widgets::LinkDisabledReason`, which draws nothing. The
  screen audit's disabled-reason check (`tests/host/screen_audit_lvgl_test.cpp`) reads that line as its reason. The
  88 px between the last key row and the footer is white (DESIGN → Components → The kit: no hint line), which keeps
  the keys' rings clear of the footer's.
- **The typing-only partial count is §5's**. The keyboard is a full-glass overlay (APP_ARCHITECTURE §7): `OpenKeyboard` makes its
  screen, opens it on the router's overlay stack, and builds the keys into the overlay's group. The stack loads it, a page
  change, two partials through white on the V1.0. Closing it, by the action, Cancel or holding OK, sets the counting back
  to every partial. It closes the overlay, which brings back the page it opened over as one full refresh and focus on
  what opened it, ringed if it wore the ring when the keyboard opened. It then deletes its screen after the event and
  answers `done`. The page under it is kept, not built again.

### 11.4 The field

- **`widgets::MakeTextInput` and `MakePasswordInput`** (`components/widgets/src/text_input.cpp`; the person's decisions on `input_mock2`):
  a `lv_button` frame `spacing.content` × `spacing.target`, white, a `spacing.stroke.default` outline, `rounded.xl` (16; the base 12
  would leave the button's corner and its ring off the frame's corner centre). The words are in `label` 16 px in from the edge
  (`spacing.md`), and a placeholder in `caption` stands 8 px (`spacing.sm`) after a still caret 3 × 36. The typed area runs from 16 px in to
  the button's hit square, 352 px: `(352 − 3) / 18` is **19 characters**, and a longer value shows its last 19, the caret in view.
  Disabled is `DitherWhenDisabled` and the words' own `text-disabled`. The trickle flag passes the state to the row and the labels,
  since a colour a child sets for itself beats its parent's disabled style.
- **The button is the kit's icon button at another size**: `detail::MakeIconButtonSized(parent, action, variant, size, radius, words)`
  in `icon_button.cpp`, which `MakeIconButton` calls with `spacing.target` and `rounded.lg`. The field calls it with ghost, `spacing.xl` (32)
  and `rounded.xl` − `kSpacingMd` = 0 (square). So its face, ring, disabled look and group join are the icon button's own.
  The ring is RingOutside: 4 px, `spacing.focus-offset` out, so 8 px of reach, the ring's outer radius 8 on the same corner
  centre as the frame's. It stands 16 px from the frame's right, top and bottom edges (13 px inside the 3 px outline).
  Its hit area is made by `lv_obj_set_ext_click_area(button, (spacing.target − size) / 2)`. That makes it the 64 × 64
  square at the frame's right end, the frame's edges its own `[HOST]`. `tests/host/text_input_lvgl_test.cpp` checks
  the three distances and the corner points of the square on LVGL's real layout. `IconAction` gains `kClear` (the cross, `glyph_close`),
  `kEyeOpen` and `kEyeSlashed`. The eye pair, `glyph_eye_on` and `glyph_eye_off`, is cut by `make_marks.py` from BYTE-90's `visible-on` and
  `visible-off` with one shared crop (2, 12, 126, 120), 30 × 26, no trim, so the pair registers. The eye shows the action.
  On a V1.0 and a V2.0 the eye and the clear button read inside the `rounded.xl` corner, and the ring's white gap to the
  outline shows `[SEEN]`.
- **The password mask and the reveal**: the field keeps the real text and draws `•` (U+2022, in the font) for each character but the last,
  which is in the clear until `lv_timer` fires after `kMotionPasswordRevealDelayMs` (`motion.password-reveal`, 1500 ms, LVGL's own
  password show time). The next typing press (`InputAppend`) masks it as part of its own redraw and starts the time again; Delete, the
  eye, `SetInputText` and the field going away end it. The timer is a one-shot LVGL frees itself, stopped in the frame's delete event
  `[HOST]`. The hide is no press and never calls `NoteTyping`, so it counts none toward `kRefreshPartialLimit` (§5; it adds one to the
  uncounted ceiling). The V2.0 draws two partials over the text per character that count one `[?]`. A gate 3 look at the field's
  ghost after a long password settles it.
### 11.5 Wiping a typed password

- **A typed password is wiped from every working copy the field keeps** (SECRETS §5): the text is a fixed buffer of
  `kInputMaxText` (64) bytes in the field's state, never a `std::string`, which reallocates as it grows and leaves its old buffer, the
  password in it, unwiped. A buffer that never moves is wiped where it stands. `detail::SecureZero` (`internal/secure_zero.h`) is
  `mbedtls_platform_zeroize`. It writes through a volatile pointer so the compiler cannot drop it as a dead store, which
  a plain `memset` before a free allows. mbedTLS is a private requirement of the widgets component. It wipes the whole buffer on the cross,
  `SetInputText` and the field going away (before `delete`), and the one byte Delete frees. The visible text the field builds for the
  label (bullets, or the clear characters while shown) is a stack buffer wiped as soon as the label has copied it. **LVGL's label keeps its
  own copy of the visible text**, and frees the old copy when the text changes, leaving its bytes in the freed block. The field wipes
  that copy in place, through the pointer LVGL gives, before it sets new text and before the label goes. While the text is masked
  that copy is bullets. So only the revealed state — the eye pressed, or the last typed character in the clear — leaves the password in
  LVGL's buffer, until the next redraw, the reveal time's end or the eye. The wipe goes as far as LVGL allows. The copy
  that `lv_label_set_text` makes itself is wiped only on the next change. A block the allocator has moved is out of reach.
  `tests/host/text_input_lvgl_test.cpp` checks that each of those paths zeroes the buffer, and that the wipe begins over the password, through a spy on the stand-in for
  `mbedtls_platform_zeroize` (`tests/host/stubs/mbedtls/platform_util.h`) `[HOST]`.

### 11.6 The field in the walk

- **The walk**: the frame is a stop (`Focusable`) on a page and in the keyboard alike, and its button the next. In the keyboard
  `KeyboardJoinField(keyboard, field.frame, field.button)` rebuilds the group so the walk is the field, the button, every key,
  Cancel, the action, and round again (it opens on the first key: DESIGN → Focus and input). The frame has no handler there, so OK on it
  and a tap on it do the tick and nothing else. The group skips a hidden button, so the cross going leaves the walk, and its
  handler moves focus to the next stop at once (`lv_group_focus_next`). LVGL puts every button-class object in the default group as
  it is made, so a frame and a button join it by themselves and the walk is built after. **Never both rings**: the frame carries
  no `LV_OBJ_FLAG_STATE_TRICKLE`. That flag passes every state a parent takes to its children, `LV_STATE_FOCUSED` and
  `LV_STATE_FOCUS_KEY` included, so a focused field would draw the button's ring too. Disabled is passed by hand
  to the words and the button (`SetInputEnabled`). `tests/host/text_input_lvgl_test.cpp` checks, on a page and in the keyboard and for
  both inputs, that the field focused rings the field alone and the next move rings the button alone. It also checks the walk's order, the
  wrap both ways, the place kept by a page switch, that joining the field leaves the group's editing off, and that holding OK cancels
  `[HOST]`. A further check holds that no label on the keyboard's screen is a hint at any stop. The field keeps its action words for
  `IconButtonWords`, which a screen with a footer hint reads. The keyboard reads none. The field sends `LV_EVENT_VALUE_CHANGED` after the eye or the cross, and the caller tells the
  keyboard (`KeyboardTextChanged`).

## 12. The select sheet

- **The sheet is the dropdown's open hook**: `widgets::MakeDropdown` installs `OpenSelectSheet` as the field's opener, so OK or a tap
  on the field opens the sheet; `SetDropdownOpener` swaps in another. A choice is `ChooseDropdown`, which sends
  `LV_EVENT_VALUE_CHANGED` once and only for a change.
- **Its height is a token sum** (`components/widgets/src/internal/sheet_rules.h`): 16, the title's 36 line box, 16, the
  rows' band (64 n + 16 (n − 1)), 16 and the foot's 96, which the table below gives per page. A list of
  fewer options than a page draws only its rows. It is the glass's width, 480, and a corner radius taller than its room, hung
  past the glass's foot. So only its top edge and its `rounded.2xl` corners show. It has no outline: the offset (elevation 2,
  §4.1) rising from its top edge stands it off the page (DESIGN → Exceptions → The select sheet's edge).

| Rows | Height | Top at y |
| --- | --- | --- |
| Four | 484 px | 316 |
| Three | 404 px | 396 |
| Two | 324 px | 476 |

- **Four rows a page, or three when four would cover the field**: the field's bottom plus the ring's 8, 8 of white and the
  offset's 6 must clear the sheet's top. So a field ending at y 294 or higher takes four, and one ending between 295 and 374
  takes three. A lower one takes three too, never fewer, since two would turn the page too often. The title names the field. `RowsPerPage` is the rule,
  `tests/host/sheet_rules_test.cpp` checks it at both edges `[HOST]`.
- **A row is `body` in 364 px, the chosen one `label`.** That is the row's 400 inside its padding, less the check's 28 and 8, so a line
  holds 20 characters. The words wrap onto a second line and a third, each on the role's line box: 40 for `body`, 36 for `label`.
  The words column takes the row's 64 px minimum and centres its lines. A row is 64 px for one line, 96 for two `body` lines and
  136 for three (§9's centring rule and test).
  The chosen row is `band`, the light dither (`ui::Fill`, a UI fill on every refresh), under black words, with the check at its
  right end, black, 28 × 20, and no word (DESIGN → Exceptions).
  A page holds the whole rows that fit, so a three-line option can cost a page a row (§10.1). With two among seven, every page
  of the host test shows whole rows above the foot `[HOST]`.
- **The foot holds Cancel alone, centred** — no words say which key does what (DESIGN → Accessibility).
  `tests/host/overlay_lvgl_test.cpp` checks the sheet has no outline, the chosen row's fill and words, and no key words `[HOST]`.
- **It is an overlay on the router's stack** (APP_ARCHITECTURE §7): opening calls `router::OpenOverlay` before a row is made, so
  the rows and Cancel are its own group's only stops, and focuses the chosen row. The field under it keeps the page's focus with
  its ring off. Closing calls `router::CloseOverlay`, which binds the page's group again and brings the field's ring back if it showed. It then
  deletes the sheet's objects and reports a choice. Its overlay `escape` is Cancel, which holding OK reaches through the escape
  chain ([DESIGN → Focus and input](../DESIGN.md#focus-and-input); [APP_ARCHITECTURE §7](APP_ARCHITECTURE.md#7-screens-routes-and-focus)). A screen deleted while the sheet is open
  takes the sheet's objects with it, and their delete event closes the overlay first. A full-screen transparent catcher under the
  sheet is clickable and does nothing, so a touch above the sheet is eaten and a drag does not reach the page behind.
- **A dialog opens over it the same way** (`widgets::OpenDialog`): its two buttons are the only stops, and closing it gives the
  buttons back to the sheet's chosen row, ringed if it wore the ring when the dialog opened.
- **The kit page** shows a dropdown of three options, one of eight whose last option is two lines, and a disabled one. Opening
  the sheet and walking it is a gate 3 look for the scrolled page behind and the partial refreshes.

---

## 13. Claude's mascot page

**Claude's mascot is ported from the agent deck with its moves' numbers unchanged, and plays as a few-frame flipbook:
whatever pose is current as each partial refresh starts, one bit a pixel, about 2 frames a second on either board** —
the person's choice, 2026-11-24. The widget is `components/widgets/src/mascot.cpp` (`include/widgets/mascot.h`, its
frames in `src/internal/mascot_frames.h` as the deck has them, its lengths in `src/internal/mascot_theme.h`); the demo
page is `components/hw_test/mascot_page.cpp`, route `hw.mascot`, Home's row before About. It is drafted and reviewed
with the [`mascot-animation`](../.agents/skills/mascot-animation/SKILL.md) skill, its host side in `.agents/project.py`
→ `MASCOT`. The page is built and not yet seen on the glass `[?]` (§8).

- **The moves are the deck's first, seen at the glass's pace; hand-picked key poses come after,** through the
  animation gate — the person's choice, 2026-11-24. A move's length is the deck's and the widget's own data
  (`mascot_theme.h`), not a token, since DESIGN → Motion's tokens hold no duration. Hello, which the deck's stage
  table leaves out, takes its own length, 1.8 s `[D]`.
- **The walk, the skip and its trip take 11.3, 7.4 and 10.7 s**: the deck's lengths re-timed for the route's fewer
  steps at this cell, no key or pace touched; the trip's band is derived for this glass, and the stage trips at the
  first skip either way `[D]`.
- **The cell is 24 px** (`spacing.mascot-cell`), 1.5× the deck's, a mascot pixel three whole 4 × 4 dither tiles — the
  person's choice over 2×, whose finish, dust and work ran off the 480 px glass.
- **The page is portrait, 480 × 800** (`kScreenWidth`, `kScreenHeight`), not the deck's landscape stage. A 360 px
  stage band stands under the status bar, the mascot centred on its bottom edge (`kMascotPageCentreX` 240,
  `kMascotPageCentreY` 324). It is free to go off the glass: the finish's flag reaches 10 px past the right edge in
  its widest frames. The status bar's title names the move and its place; under the stage, a three-line readout of its
  frames and the glass's partials and fulls (§5), Previous and Next, the Move sounds toggle, and Back, Loop and
  Play/Stop in the footer — the person's answers and yes, 2026-11-24.
- **It opens on Rest, still, with no blink at rest.** Each move runs as the deck's stage runs it (`mascot_stage_run`),
  the live "n of N" in the stage's corner, N the pass's length over a frame until it has played and the count it took
  after. Loop replays the move with the deck's 1.5 s rest between passes until Stop.
- **Its sounds are the deck's own, at its pitches, on the note player, one voice, synced at the glass's pace.** The
  person chose them by ear on a V2.0, 2026-11-24. They win over the same sounds moved into the buzzer's band, and over
  the deck's synth streamed through the buzzer, which hisses. How the moments are queued and played is §5.
- **On a V1.0 the page keeps the person's preset, by default the kept waveform, about 1.84 frames a second** — the
  person's choice, 2026-11-25, after the faster drives left the moves' ghost on the mascot's dithered body
  ([STICKY_EPAPER_WAVEFORMS §3.5](STICKY_EPAPER_WAVEFORMS.md#35-presets-and-defaults)).
- **The page is the first place where something animates**: DESIGN → Motion's "Nothing animates" is answered for
  it by DESIGN → Exceptions → The hardware-test demo, and for the agent deck's screens by DESIGN → Exceptions → Claude's mascot on the
  deck's screens (§14).

## 14. The agent deck port's additions

What the agent deck's screens needed of the kit, each opt-in: every existing widget's default look is unchanged, and the audit's frames
of the hardware test (the kit page's first seventeen glass-fulls, measured while it scrolled by pixel, and every ringed stop's
frame) match the frames drawn before the additions, byte for byte `[D]`. The checks are `tests/host/deck_kit_lvgl_test.cpp` (281 checks on LVGL's real layout, fonts and renderer at 480 × 800),
`tests/host/token_parity.py` for the tokens and the design check for §2; each addition has a specimen on the kit page, "Small chips" to
"Quiet words" at its foot. The deck's screens are built from these calls alone; the harness that drafted them reached past the
kit in places, and these are the places.

### 14.1 Words and roles

- **Three roles** — `title`, `caption-strong` and `caption-regular` (§3), the last with the one new font, `bitcount_400_26`. The role table of `ui`, `s_roles`, has ten rows now, in the order of the enum. The hardware test's type scale page roles and the screen audit's role list know the three. So a 30 px semibold, a 20 px bold or a 20 px regular
  is a role there and no longer an off-scale size.
- **Words one size down: `widgets::SetWordsRole(obj, role)`** (`widgets/words_role.h`) — walks `obj` and every child and brings each label
  set in a larger size than `role` down to it. It sets the font, LVGL's line space (the role's pitch less the font's height) and
  `ui::FitLineBox`, so the label is whole line boxes tall with its words centred in each (§9). A label at the role's size or under it is
  left alone. So a toggle row's name and state word, a value row's name and value, a dropdown's, a radio row's, a list row's title and a choice
  card's name come down to `caption` or `caption-strong`. A `caption` supporting line, a reason or a hidden label stays.
  `widgets::SetLabelRole(label, role)` sets one label to a role whatever size it holds, with the same font, line space and line box. The size of a
  label is read from `ui`'s compiled fonts (`ui::Bitcount`), never from its line height. A gap held by a negative margin
  (`widgets::detail::Glue`) follows the new line box. The rows' own geometry does the rest. A toggle row's words column and a value row, dropdown field or list row of fixed height keep their 64 or 80 px with the words centred. A radio row's column keeps its 72. A choice
  card keeps its floor of 74 and centres its words, growing about its centre. The host test builds every kind at one line and two, with the reason
  showing, disabled and chosen, and reads each row's height and the offset of its content from its centre: 0 px in every case, and the
  heights are what the kit gave them `[D]`. One call is one choice for a whole control. A label set again later keeps its role, and a second
  call changes nothing.
- **A name over its line keeps `pair-sm`** — a choice card's, a list row's and a detail row's name stand over their line on line boxes
  (§9). A role whose leading is less than DESIGN → Layout → Spacing's 8 px would leave their own boxes closer. After the words come
  down, `SetWordsRole` makes the gap up in the name's margin, `pair-sm` less the leading of the name's bottom and the line's top,
  wherever the parent is a column with no gap of its own, both are showing and neither is glued. At 26 px on a 32 px line the
  leading is 8, `pair-sm` itself, so no margin is added. A choice card at `caption-strong` is 96 px (16 + 32 + 32 + 16) and a detail
  row one size down 80 (8 + 32 + 32 + 8), and the kit's defaults, left alone, stay as they are. The agent deck's question pairs a
  `caption-strong` name with a `caption-regular` detail, whose font is shorter in its 32 px box. Their leading is 10, so the deck
  pulls the detail up 2 px (`margin_top`, `answer_screens.cpp`): the pair stands `pair-sm`, and the card is 94 px for a line each. The screen audit reads this gap as the `spacing` proxy "pair gap by font box"
  `[D]`.
#### Detail rows and list rows

- **A detail row one size down** — `widgets::SetDetailRowOneSizeDown(row)` (`widgets/detail_row.h`): the label in `caption-strong` over its value in `caption`
  (the person, 2026-11-26: "Label bold, value semibold"). It sets the label's role first (`SetLabelRole`), so the gap `SetWordsRole` makes up is measured from the
  line box as it will stand, then brings the value down to `caption`. Both are 32 px lines, whose leading, 8 px, is `pair-sm` between their own boxes: the row is 80 px for a
  line each, 8 + 32 + 32 + 8 `[D]`. The default row, name `caption` over value `label`, is 84 px, 8 + 32 + 36 + 8.
- **A list row's roles and fit** — `MakeListRow(…, ui::Type title = kLabel, ListRowFit fit = kDots, uint32_t key = 0, ui::Type supporting_role = kCaption)`; `fit` is the stepped line's opt-in (§14.6).
  `supporting_role` is the supporting line's role, stepped or dotted: the needs-you list passes `caption-regular` under its `body` names, 26 px regular under 30 px regular, and every other row keeps `caption`. The `body` role is the deck's needs-you names, 30 px regular on a 40 px
  line. So a title in `body` and a `caption` line under it are 40 + 32, 72, inside the row's 80 less its 2 × `spacing.sm`, 64. They stand
  centred 4 px into the padding either side, and the row stays 80 high. The `widgets` component now requires `ui` publicly, since its headers name `ui::Type`.
  The arrows of APP_ARCHITECTURE §2 are unchanged, `widgets` having depended on `ui` already.
- **The heading's count line** — `layout::Quiet(parent, text, fit, role = kCaption)` (`agent_deck/deck_ui.h`) takes the role of the quiet words. The needs-you list passes `caption-regular` for "4 sessions" under `title`: 30 px semibold over 26 px regular, a step in size and in weight, the colour still `text-quiet`, the dark dither. `layout::TuckUnder(heading, line)` stands the line `spacing.xs` under the heading between their own boxes. It takes the two line boxes' leading back in the line's margin, so the tuck holds at any size. The Wi-Fi page's headings and the agent line under a request's or a question's tool take it too. The host test reads the font against `ui::Bitcount(26, 400)`, its line box and its place `[HOST]`.

### 14.2 The small chip, the foot's icons and the link indicator

- **The small chip** — `widgets::MakeSmallChip(parent, text, control = false)` (`widgets/chip.h`): `spacing.chip-small` (36 px) high,
  `rounded.lg`, the light dither, `title` words in black `spacing.md` either side and centred, as wide as its words (194 px for "2 working",
  230 for "1 needs you"). A reading is an object with no press and no place in the focus group. A control is a button in the group with the
  kit's ring outside it, `LV_EVENT_CLICKED` for the caller, and its hit area grown by `(spacing.target − spacing.chip-small) / 2`, 14 px, so it
  is hit across 64 above and below; disabled it is the light dither and its words `text-disabled`. The host test reads the height, the
  words' centring on both axes, the 64 px hit span and the hit test one pixel past it, the group's count, and the ring's width.
- **The foot's icons and the link indicator** — the deck's own art through `marks/make_marks.py`, box-averaged and thresholded as every mark:
  Wi-Fi 38 × 28 (`wifi-on.png`), Settings 34 × 34 (`settings.png`), the link up 34 × 18 (`link_on.png`) and down 34 × 32
  (`link_off.png`), each on `spacing.icon`'s box and trimmed to its ink, and the status Wi-Fi 81 × 60 at `spacing.status-mark`'s 85
  (`wifi-on.png`). Regenerating `marks.c` changed none of the existing marks: the diff holds additions only `[D]`. `IconAction::kWifi` and
  `kSettings` (and `Glyph::kWifi`, `kSettings`) are the icon button's, in any variant (the foot's are tonal, as the sound toggle beside
  them). **Power** is BYTE-90's `power.png`, 34 × 34 on the same box (`glyph_power`), added through the same script with additions
  only in the diff `[D]`. Its symbols are `IconAction::kPower` and `Glyph::kPower`, the foot's last icon button, `Turn off`, DESIGN → Exceptions → The icon
  button. It turns the deck off from `power_leave`'s sequence (APP_ARCHITECTURE §1). `widgets::MakeLinkIndicator(parent, up)` is the image itself in `text-primary`, no box, clickable or group member.
  `SetLinkIndicator(indicator, up)` swaps its face. A row that centres it vertically keeps either face centred. The deck's foot is the caller's
  layout: the indicator on the left, the icon buttons flush right.

### 14.3 The stacked status block and the network row

- **The stacked status block** — `widgets::MakeStackedStatusBlock(parent, heading, description, mark)` (`widgets/status_block.h`): the mark,
  `spacing.item` under it the `label` heading, `spacing.pair-md` under that between their own boxes (the same `Glue` as the beside block) the
  `body` line in `text-secondary`. It is a column as wide as its parent with every part centred across it, and the words wrap and centre inside it. The
  host test reads the mark's, the heading's and the line's box centres within one pixel of the block's. It reads the ink of each line within a glyph's
  side bearing of it (a full stop's ink stands left in its cell, 9 px on "later.") and both gaps. The `StatusMark::kWifi` mark is 81 px wide against
  the slot's 73, which the beside block's words stand at one x for (§9). A beside block given it draws no mark rather than cut one. The
  stacked block is a mark's own width, so it has no slot.
- **The network row's name** is `caption-strong`, 26 px bold, in every look and for a dotted or a stepped name, over its facts in `caption-regular`: the same 32 px line, so the row stays 80 px with its name where it stood. The dotted label takes the role's font and line box, the stepped line is made in the role (§14.6). The inverted row, the card row and the row at rest alike `[HOST]`.
- **The network row as a card** — `MakeNetworkRow(…, NetworkLook look = kInverted)`; `NetworkLook::kCard` draws the connected or connecting row on the light dither, `colors.band`, with no edge, and the name, the facts, the check, the padlock and the lit bars `text-primary` (the person, 2026-11-26: "light grey background not outline"). The unlit bars are the dark dither, which shows on the light ground.
  - **The card keeps the row's padding**, so the name stands at the x and y it has at rest, and the row stays 80 px `[HOST]`.
  - **The look is the row's flag** (`LV_OBJ_FLAG_USER_3`), so `SetNetworkRow` draws it each time, and only a row with the flag: a face a screen gives another row stays. A row at rest is the ordinary row in either look; the deck's list of networks stands its rows at rest on a card's face (`layout::CardFace`), and a row on that face draws its unlit bars dark too. The ring is untouched.
  - **The host tests read** the fill, that there is no edge, the ink of every word and glyph, the unlit bars and the name's place, on the kit's row and on both deck pages `[HOST]`.

### 14.4 The swipe page

- **`widgets::MakeSwipePage(parent, width, height, turn, user, axis)`** (`widgets/swipe_page.h`) — a bare container, `width` by `height`, that
  claims a drag along its axis with the swipe the list view, the text view and the pager share (§10). It uses `detail::ClaimSwipe` on a scrollable
  object that has room to go (a 1 px child twice its length), and the band is put back at once so nothing moves under the finger. On the lift
  one swipe turns one page and calls `turn(dir, user)` — +1 for a swipe left or up. The deck's home carousel is one, 480 × 800, sideways;
  a long question's band is one, vertical, which scrolls its own pages with `lv_obj_scroll_to_y`. The same thresholds as every page swipe
  (`kTouchSwipeDistance`, the flick); a drag across the axis chains to the page behind. It ignores its parent's layout and lets the rings of
  its controls out. It joins no focus group: the screen's keys turn its pages. `tests/host/swipe_lvgl_test.cpp` drags a page of its own with
  a scripted finger — one turn each way along its axis, with the owner's pointer, the page unmoved, none across it — and
  `tests/host/deck_screens_lvgl_test.cpp` swipes the carousel sideways `[HOST]`.
- **`widgets::ClaimPageSwipe(page, turn, user)`** — makes a screen's glass-wide page claim an up or down swipe begun anywhere on
  it: its heading, its empty room, a control (whose press the swipe cancels) or a band that pages itself, which keeps its own
  swipe. `turn(dir, user)` turns what the screen pages: `widgets::TurnListView` on the Wi-Fi page, the needs-you list and the
  hardware test's home, Buttons and buzzer and About (`SwipeTurnsList` in `hw_test.cpp`), the settings' `ShowPage`,
  `widgets::TurnTextView` on the request and the question's own turn, each with `sound.at-end` past either end (DESIGN → Layout
  → Paging; the person, 2026-11-26). The page has room to go (a 1 px child twice its height) and never moves: the swipe puts it
  back under the finger, and a scroll no finger holds — LVGL's scroll-on-focus bringing a control past the page's foot into
  view — is put back at once, before a frame is drawn. A sideways drag chains to the screen behind. `tests/host/swipe_lvgl_test.cpp`
  drags a page from a button, both ways, and focuses a control below it; `tests/host/deck_screens_lvgl_test.cpp` turns the Wi-Fi
  page's facts from Back and its heading, the settings from their heading, a long request from Allow and its heading, a long
  question from its heading, and hears the needs-you list's at-end cue from its heading; `tests/host/screen_audit_lvgl_test.cpp`
  turns the hardware test's home, Buttons and buzzer and About from Back and back from the status bar, and fails the audit if a
  list does not turn or Back is pressed `[HOST]`.

### 14.5 The holds, and what is not here

- **The three holds** — `kMotionStartupHoldDelayMs` 2000, `kMotionAnsweredHoldDelayMs` 2300 and `kMotionTimedOutHoldDelayMs` 1800, each the
  time the code waits as §6 names a motion token; DESIGN → Exceptions → Timed leaves is their argument. The firmware's two timers and the
  screens they leave are the deck feature's, and nothing in this stage starts one.
- **The mascot's cycle lengths a page may read** — `widgets::kMascotIdleCycleMs`, 5000, and `kMascotLookCycleMs`, 5600, in `mascot.h`: the
  idle's lean and the look-around, the lengths `mascot_theme.h` keys the moves to (`kMascotIdleMs`, `kMascotLookMs` are these). The needs-you
  list's avatars ask for rest at their end (STICKY_AGENT_DECK §4.3). `mascot_voice::Voice::Attach` takes a `same_blink_ms` so several mascots
  on one voice, a list's avatars, tell a blink they share once.
- **What is not here** — the deck's startup logo and the deck's screens themselves: they are the `agent_deck` feature's
  ([STICKY_AGENT_DECK](STICKY_AGENT_DECK.md)).

### 14.6 The stepped line

A one-line field too long for its room rests, then moves along its words in steps, where it ended in dots. The argument is DESIGN → Exceptions → Stepped lines on the deck's screens; the screens' side is [STICKY_AGENT_DECK §4.11](STICKY_AGENT_DECK.md#411-stepped-lines).

- **The API** is in `widgets/marquee.h`: `widgets::MakeMarquee(parent, role, text, {align, key, carry})`, `SetMarquee(line, text)` and `ProbeMarquee(line)`.
- **The tests:** `tests/host/marquee_lvgl_test.cpp` holds the widget's checks on a fake clock; `tests/host/deck_screens_lvgl_test.cpp` holds each screen's `[HOST]`.
- **The clock and a control's line** are §14.7; **a key, a covered screen, the clock's limit and the kit's calls** are §14.8.

**The values are tokens.**

- **Rest:** `kMotionMarqueeRestDelayMs`, 4 s.
- **Step:** `kSpacingMarqueeStep`, 48 px, every `kMotionMarqueeStepDelayMs`, 1 s.
- **Passes:** `kMarqueePasses`, 3, then it stands at its start (§14.7).
- **Gap:** the words come round after `kMarqueeGapSpaces`, 3, spaces of the line's own font.
- **A pass** is the words and the gap, `run` px, in `ceil(run / 48)` steps. Each step goes on from the last, and the words come round at `run`.
- **The last step of the last pass** is short and lands on the start.
- **Worked case:** the 62-character doing line of a session's page is 744 px in `caption`. A pass is 780 px and 17 steps; three passes are 51 s of moving, then 4 s of rest `[D]`.

**A line is a box, a window and a label.**

- **The box** is the role's line box high and `lv_pct(100)` wide unless its caller sets another width. It draws nothing.
- **The window** clips, and the label moves in it.
- **A line that fits** is the label alone, as wide as the box and aligned in it. A centred name stays centred.
- **A fitting line draws pixel for pixel as the dotted label it replaces.** The host test renders a list row and a network row each way and compares every pixel of the 80 px row `[HOST]`.
- **A line standing after its passes draws that label too.** Its label holds the words once in `LV_LABEL_LONG_MODE_DOTS`, the room wide and one line box high, and the host test compares its pixels with a dotted label's: none differ `[HOST]`.
- **A line too long** holds its words twice with the gap between, so the label is as wide as they are.
- **A step** is `lv_obj_set_x` on the label. The words are drawn whole at their role at one bit a pixel; only where they stand changes.
- **The window reaches `kBleed`, 1 px, left of the box.** The box lets it out (`LV_EVENT_REFR_EXT_DRAW_SIZE`, as §9's ring).
- **The reason for the bleed:** the Bitcount fonts' stems stand at most 1 px left of their cell in ASCII, in `caption`, `label`, `title` and `caption-strong`; `caption-regular` and `body` have none. A window cut there would take the first letter of "Bash" a column thinner than the dotted label drew it `[HOST]`.
- **The label's long mode is the wrapping one** while the line fits or steps, never LVGL's `CLIP`, which clips to the label's own box. It is `DOTS` while the line stands after its passes, and the wrapping one again when the line begins again.

**The room is the box's own content width.**

- **It is measured** with `lv_text_get_size` in the label's font when the layout gives one (`LV_EVENT_SIZE_CHANGED`), and again at a step if it changed.
- **So a row fits again** whose padding a screen sets after making it, or after a resize.

### 14.7 The stepped line's clock and controls

The lines of §14.6 step on one clock, and every line stands at its start after its passes, by these rules.

**One clock.**

- **One `lv_timer`**, `kMotionMarqueeStepDelayMs` apart, steps every line that is due. Lines step together and the glass draws one frame for them: the host test counts one flush for each step of two lines `[HOST]`.
- **Its life:** it is created with the first line that steps and deletes itself at a step that finds none.
- **A line counts its rest** in time from when it came to rest. It moves at the first step of the clock at least `kMotionMarqueeRestDelayMs` after.
- **A line begun when no other is mid-pass** (the clock is reset then) rests exactly 4 s.
- **A line begun between two steps** rests 4 to 5 s.
- **A step** is a change of state on LVGL's timer, drawn by the next frame the display renders.
- **A frame the mascot is already drawing** carries the step.
- **A step the mascot's frame does not carry** is a partial refresh of its own, for the three passes only, which counts toward `kRefreshPartialLimit` (STICKY_AGENT_DECK §4.11). Not yet measured on the glass `[?]`.

**After its passes every line stands at its start.**

- **The last step of the last pass** lands on the start and sets the line's `stopped`: a line in a control and a reading alike (the person, 2026-11-26: "Stop after 3 passes"). Its label holds the words once, ending in dots (§14.10). It moves no more and draws nothing.
- **The clock stays** while a stopped line is under it, one `lv_timer` a second that changes no object, so a line sees its page covered and shown again. It deletes itself when no line steps.
- **A line in a control** has an ancestor in a focus group (`lv_obj_get_group`). At each step the clock reads its control's state: a rising `LV_STATE_FOCUSED` or `LV_STATE_PRESSED` begins the line again from the start, resting first. So focus arriving takes a line mid-pass back to its start, and a stopped one moves again, at the next step, at most a second late.
- **A reading** (a line in no control) begins again, resting first, in three cases.
  - **Its words change:** `SetMarquee` with other words, or a screen built again with other words under the line's key (§14.8). The same words leave it standing.
  - **Its page is turned to:** a carousel page's lines are made again with the page's own keys, so they are new and at rest `[HOST]`.
  - **Its screen is shown again:** a step that finds the screen covered (not active, or an overlay open) marks a stopped reading, which keeps its dots meanwhile. The first step that finds it shown begins it, resting from then; a rebuild under its key carries the mark. A cover under 1 s is not seen. A cover never begins a control's line.
- **The tests** hold it `[HOST]`.
  - The marquee test, on its fake clock: three passes, then no move in 30 s. Other words, a covered screen shown again and a page
    built again under its key begin it; the same words do not.
  - The deck screens test: a session's page, a record, a page turned, home entered again.
- **What it costs:** after three passes a line draws nothing, so a page at rest makes no refresh of its own (STICKY_AGENT_DECK §4.11).

### 14.8 The stepped line's keys, covered screens and kit

The lines of §14.6 keep a pass across a rebuild, stand still when covered and are built by the kit's calls.

**A screen built again keeps a pass under way, and a line standing, by a key.**

- **The case:** a screen that builds itself from its model deletes its lines and makes them again in the one call. Home does so on every record, the list every `motion.self-update`.
- **The key** is `MarqueeOptions::key`, which names the field: `MarqueeKey(scope, owner, field)` — the screen, an agent's or a request's id, which line.
- **A deleted line keeps its place** for the one rebuild, until LVGL's next `lv_async_call`: its rest's start, its step, whether it stopped, whether its control is held.
- **A line made again** under the same key with the same words takes the place over, a pass under way or a stop, which it draws in dots.
- **Other words** under the key start from their start, and so does a line made later.
- **`SetMarquee`** does the same for a line kept: the same words do nothing.
- **`carry`:** the list's facts count the wait (`Bash • 24s`, new every rebuild), so the facts line is made `carry`. New words under the same key carry the pass on, which restarting each 5 s would never let reach its end `[HOST]`.

**A covered line stands still.**

- **At each step** a line on a screen that is not LVGL's active one, or while any router overlay stands (`router::OverlayOpen`), keeps its place. That holds for a pass under way too.
- **It counts its rest again** from when it is uncovered (DESIGN → Motion → Waits and things that update on their own).
- **A standing reading** is begun again by the cover (§14.7); the line made again by the screen's entering takes over its place, begun.

**A full clock degrades, never breaks.**

- **The limits:** the clock holds 24 lines (a page of the deck's holds at most about ten) and 24 keeps for a rebuild, both tables
  in PSRAM (`lvgl_display::InPsram`, APP_ARCHITECTURE §2): only the LVGL task's timer and events touch them.
- **A line past the limit ends in dots**, as a plain label does, and the widget logs a warning; so does one whose words it
  has no memory to copy. Its words are never cut off with nothing to say so `[HOST]`.

**`SetMarquee`, the key and the options are the kit's.**

- **A screen builds a stepped line** with `layout::Step` (`agent_deck/deck_ui.h`).
- **A row** takes `MakeListRow(…, ListRowFit::kStepped` or `kSteppedCounting, key)` and `MakeNetworkRow(…, NetworkName::kStepped, key)`.
- **Every other caller** keeps the dotted default, its code unchanged.

### 14.9 The settings' one left edge

The deck's screens stand the title, the words that take no ring and every card's box on the content's edges, 24 and 456 (the person,
2026-11-26; DESIGN → Layout → Spacing).

- **A row whose ring goes round it whole is a card** — `layout::CardFace(row)` (`agent_deck/deck_ui.h`) gives it the question cards' light dither and `rounded.lg`, its words `spacing.md` inside by its own padding, so its ring stands in the margin round a box, 8 px out, never on bare words. The settings' Time zone, `Network` / `Not joined` and Hardware test, the Wi-Fi page's networks to pick and the needs-you rows take it. The connected network is the network row's card look (§14.3), and the Wi-Fi page's facts take the face on each page's stop, the box round the page's facts, behind them, on the network card's edges.
- **Words that take no ring stand on the edge with the title** — Sound's and Wi-Fi's names (the toggle takes the ring), Auto sleep's (the slider does), and Battery, a reading out of the walk: its value row gives up the edge it reserves and never draws (`FlushWords` in `agent_deck/settings_screen.cpp`), so its words stand at 24 and its value ends at 456.
- **The scroll indicator's lane** stands in the right margin, `spacing.scroll-x`, clear of the rows' right edge on both pages.
- **The host test** reads, on page 1 and page 2, the x of each bare name, 24, and of each card and its words, 24 and 40, and that the lane stands right of the rows `[HOST]`. The kit's own rows are unchanged: the hardware test's rows keep their padding.

### 14.10 The stepped line standing in dots

A line that has made its passes holds its words once, ending in LVGL's three full stops, where it held them twice to step (the person, 2026-11-26: "Stopped readings end in …").

- **The swap:** the last step of the last pass sets `stopped` and sets the label to the words once, `LV_LABEL_LONG_MODE_DOTS`, the room wide and one line box high. This is the one frame the stop draws. At every step after it the clock changes no object.
- **The pixels:** the line draws the dotted label's pixels, the last dot inside the box. The host test compares them with a dotted label of the same role and room: none differ `[HOST]`.
- **The way back:** a restart holds the words twice again, in the wrapping mode, at the label's own width, and the line rests and steps as before. Four things restart it:
  - other words, `SetMarquee` or a page built again under the line's key;
  - a reading's page turned to or shown again;
  - a control's focus;
  - a control's press.
- **A key keeps it:** a page built again with the same words takes the stop over, and the new line is made in dots (§14.8).
- **The marquee test** reads the label's mode, its text and its width, the pixels, that no frame follows the stop in 20 s, and the words twice again after each of the four restarts `[HOST]`.
- **The deck screens test** reads a session's name and doing line standing in dots, and a list row's name and facts going back to stepping on focus `[HOST]`.
