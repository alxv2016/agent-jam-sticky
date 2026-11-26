// Every token in DESIGN.md's front matter, by role, as the firmware uses it (STICKY_DESIGN_TOKENS §6). A value changes
// in DESIGN.md first, then here — never only here. A token's path is its symbol: colors.text-primary →
// kColorTextPrimary. A colour is its panel level, 0 to 3, with the hex it stands for in a comment
// (STICKY_DESIGN_TOKENS §2.1).

#pragma once

#include <cstddef>
#include <cstdint>

namespace design_tokens {

// --- Colours: levels 0 (black) to 3 (white); a partial refresh shows 0 and 1 black, 2 and 3 white ------------------
// Levels 4 and 5 are no panel level: the two sparse dithers, one black pixel in sixteen and in eight, which only a
// disabled control's words are drawn in; `ui::Fill` gives their codes (STICKY_DESIGN_TOKENS §2.1, §4).

inline constexpr uint8_t kColorScalePrimary0 = 0;  // #000000
inline constexpr uint8_t kColorScalePrimary1 = 1;  // #555555
inline constexpr uint8_t kColorScalePrimary2 = 2;  // #AAAAAA
inline constexpr uint8_t kColorScalePrimary3 = 3;  // #FFFFFF
inline constexpr uint8_t kColorScaleSparseSixteenth = 4;  // #F8F8F8 — one black pixel in sixteen, in linear light
inline constexpr uint8_t kColorScaleSparseEighth = 5;     // #F0F0F0 — one black pixel in eight, in linear light
inline constexpr uint8_t kColorBlack = kColorScalePrimary0;
inline constexpr uint8_t kColorWhite = kColorScalePrimary3;

inline constexpr uint8_t kColorBackground = kColorWhite;       // #FFFFFF
inline constexpr uint8_t kColorSurfaceAction = kColorBlack;    // #000000
inline constexpr uint8_t kColorSurfaceChosen = kColorBlack;    // #000000
inline constexpr uint8_t kColorSurfaceOn = kColorBlack;        // #000000
inline constexpr uint8_t kColorTextPrimary = kColorBlack;      // #000000
inline constexpr uint8_t kColorTextSecondary = kColorScalePrimary1;  // #555555
inline constexpr uint8_t kColorTextOnFill = kColorWhite;       // #FFFFFF
inline constexpr uint8_t kColorOutline = kColorBlack;          // #000000
inline constexpr uint8_t kColorDivider = kColorScalePrimary1;  // #555555
inline constexpr uint8_t kColorFocusRing = kColorBlack;        // #000000
inline constexpr uint8_t kColorBand = kColorScalePrimary2;     // #AAAAAA
inline constexpr uint8_t kColorShadow1 = kColorScaleSparseEighth;  // #F0F0F0 — level 1's offset: one black pixel in eight
inline constexpr uint8_t kColorShadow2 = kColorScalePrimary2;      // #AAAAAA — level 2's offset: the light dither
inline constexpr uint8_t kColorShadow3 = kColorScalePrimary1;      // #555555 — level 3's offset: the dark dither
inline constexpr uint8_t kColorKeyline = kColorWhite;              // #FFFFFF — the line round an offset's outside
// Claude's mascot's roles, the agent deck's in the panel's greys: its shade, body and tint a level apart, set toward the
// ink for the white glass — provisional, the person's "looks good", refined after seeing it on the glass (DESIGN → Colors).
inline constexpr uint8_t kColorMascotBody = kColorScalePrimary1; // #555555 — the mascot's body, hands and legs: a level lighter than its shade, a level darker than its tint
inline constexpr uint8_t kColorMascotShade = kColorScalePrimary0; // #000000 — a side of the mascot turned away
inline constexpr uint8_t kColorMascotTint = kColorScalePrimary2; // #AAAAAA — a hand in front of the mascot's body
inline constexpr uint8_t kColorMascotLaptop = kColorScalePrimary2; // #AAAAAA — the work's laptop
inline constexpr uint8_t kColorMascotFlag = kColorScalePrimary2; // #AAAAAA — the finish's flag
inline constexpr uint8_t kColorMascotFlagCheck = kColorScalePrimary0; // #000000 — the flag's checks
inline constexpr uint8_t kColorMascotFlagPole = kColorScalePrimary0; // #000000 — the flag's pole
inline constexpr uint8_t kColorMascotDust = kColorScalePrimary2; // #AAAAAA — a puff of dust at the mascot's feet
inline constexpr uint8_t kColorMascotDustFaint = kColorScaleSparseEighth; // #F0F0F0 — the puff's fainter half
inline constexpr uint8_t kColorMascotSweat = kColorScalePrimary2; // #AAAAAA — the sweat's beads
inline constexpr uint8_t kColorMascotConfettiA = kColorScalePrimary0; // #000000 — the celebrate's confetti, every other piece
inline constexpr uint8_t kColorMascotConfettiB = kColorScalePrimary2; // #AAAAAA — and the pieces between
inline constexpr uint8_t kColorMascotFadedBody = kColorScalePrimary2; // #AAAAAA — a previous chat's mascot: its body
inline constexpr uint8_t kColorMascotFadedShade = kColorScalePrimary1; // #555555 — its shade
inline constexpr uint8_t kColorMascotFadedTint = kColorScaleSparseEighth; // #F0F0F0 — its tint
inline constexpr uint8_t kColorThumb = kColorBlack;            // #000000
inline constexpr uint8_t kColorThumbOn = kColorWhite;          // #FFFFFF
inline constexpr uint8_t kColorKnob = kColorWhite;             // #FFFFFF
inline constexpr uint8_t kColorTextDisabled = kColorScaleSparseSixteenth;      // #F8F8F8
inline constexpr uint8_t kColorTextDisabledPage = kColorScaleSparseEighth;     // #F0F0F0
inline constexpr uint8_t kColorTextQuiet = kColorScalePrimary1;                // #555555 — the dark dither as words: the deck's small supporting lines, an argued exception under the text floor

// --- Type: Bitcount Single at one bit a pixel (STICKY_DESIGN_TOKENS §3) -----------------------------------------------

struct TypeRole {
    int16_t size_px;
    int16_t line_px;
    int16_t weight;
};

// The primitives: the scale, each size a multiple of 10 px so the font's cells — a tenth of the em — fall on whole
// pixels. Its line is 1.2 em to 40 px, then tapers, each step on the 4 px grid and never under the font's own height:
// 56 at 50, 64 at 60, 72 at 70, 80 at 80. 100 px and up hold figures only, on the height of their digits
// (STICKY_DESIGN_TOKENS §3; tests/host/line_floor_lvgl_test.cpp holds the floor).
inline constexpr TypeRole kTypographyScale20Semibold = {20, 24, 600};
inline constexpr TypeRole kTypographyScale20Bold = {20, 24, 700};
inline constexpr TypeRole kTypographyScale20Regular = {20, 24, 400};  // the second regular weight
inline constexpr TypeRole kTypographyScale30Regular = {30, 36, 400};  // the first regular weight
inline constexpr TypeRole kTypographyScale30Semibold = {30, 36, 600};
inline constexpr TypeRole kTypographyScale30Bold = {30, 36, 700};
inline constexpr TypeRole kTypographyScale40Semibold = {40, 48, 600};
inline constexpr TypeRole kTypographyScale40Bold = {40, 48, 700};
inline constexpr TypeRole kTypographyScale50Semibold = {50, 56, 600};
inline constexpr TypeRole kTypographyScale50Bold = {50, 56, 700};
inline constexpr TypeRole kTypographyScale60Semibold = {60, 64, 600};
inline constexpr TypeRole kTypographyScale60Bold = {60, 64, 700};
inline constexpr TypeRole kTypographyScale70Semibold = {70, 72, 600};
inline constexpr TypeRole kTypographyScale70Bold = {70, 72, 700};
inline constexpr TypeRole kTypographyScale80Semibold = {80, 80, 600};
inline constexpr TypeRole kTypographyScale80Bold = {80, 80, 700};
inline constexpr TypeRole kTypographyScale100Semibold = {100, 68, 600};
inline constexpr TypeRole kTypographyScale100Bold = {100, 68, 700};
inline constexpr TypeRole kTypographyScale120Semibold = {120, 80, 600};
inline constexpr TypeRole kTypographyScale120Bold = {120, 80, 700};
inline constexpr TypeRole kTypographyScale160Semibold = {160, 108, 600};
inline constexpr TypeRole kTypographyScale160Bold = {160, 108, 700};

// The roles: each one primitive (DESIGN → Typography → Scale and roles).
inline constexpr TypeRole kTypographyHero = kTypographyScale60Bold;
inline constexpr TypeRole kTypographyDisplay = kTypographyScale50Bold;
inline constexpr TypeRole kTypographyHeadline = kTypographyScale40Bold;
inline constexpr TypeRole kTypographyBody = {kTypographyScale30Regular.size_px, 40, kTypographyScale30Regular.weight};  // scale-30-regular on 40
inline constexpr TypeRole kTypographyLabel = kTypographyScale30Bold;
inline constexpr TypeRole kTypographyCaption = kTypographyScale20Semibold;
// Caption on a loose line, 1.5 em: a text view's lines, the deck's airy 24 on 16 (DESIGN → Typography → Scale and roles).
inline constexpr TypeRole kTypographyCaptionLoose = {20, 30, 600};
// The agent deck's three: `title`, 30 px semibold, its screens' names and headings and a small chip's words; `caption-strong`, 20 px
// bold, a choice card's name, a detail row's label and a network row's name one size under `label`; `caption-regular`, 20 px regular,
// the needs-you list's facts line (DESIGN → Typography → Scale and roles).
inline constexpr TypeRole kTypographyTitle = kTypographyScale30Semibold;
inline constexpr TypeRole kTypographyCaptionStrong = kTypographyScale20Bold;
inline constexpr TypeRole kTypographyCaptionRegular = kTypographyScale20Regular;

// --- Radius -------------------------------------------------------------------------------------------------------

inline constexpr int16_t kRoundedNone = 0;
inline constexpr int16_t kRoundedSm = 4;
inline constexpr int16_t kRoundedMd = 8;
inline constexpr int16_t kRoundedLg = 12;
inline constexpr int16_t kRoundedXl = 16;
inline constexpr int16_t kRounded2xl = 32;
inline constexpr int16_t kRoundedFull = 9999;

// --- Elevation: how high a surface stands off the page (STICKY_DESIGN_TOKENS §2.1; DESIGN → Elevation & Depth) ----------------

struct ElevationLevel {
    int16_t outline_px;  // the edge of a raised surface
    uint8_t shadow;      // the colour level of its offset's dither; kNoShadow where it has none
    int16_t offset_px;   // how far the offset stands from the box
    int16_t keyline_px;  // the white line round the offset's outer edge
};
inline constexpr uint8_t kNoShadow = 0xFF;

// --- Spacing, in panel pixels ------------------------------------------------------------------------------------------

inline constexpr int16_t kSpacingXs = 4;
inline constexpr int16_t kSpacingSm = 8;
inline constexpr int16_t kSpacingMd = 16;
inline constexpr int16_t kSpacingLg = 24;
inline constexpr int16_t kSpacingXl = 32;
inline constexpr int16_t kSpacing2xl = 48;
inline constexpr int16_t kSpacingEdge = kSpacingLg;
inline constexpr int16_t kSpacingFocusOffset = 4;
inline constexpr int16_t kSpacingTarget = 64;
inline constexpr int16_t kSpacingContent = 432;
inline constexpr int16_t kSpacingStatusBar = 44;
inline constexpr int16_t kSpacingFooter = 96;
inline constexpr int16_t kSpacingListRowHeight = 80;
inline constexpr int16_t kSpacingToggleHeight = 44;
inline constexpr int16_t kSpacingToggleWidth = 96;
inline constexpr int16_t kSpacingToggleThumbWidth = 52;
inline constexpr int16_t kSpacingSliderHeight = 52;
inline constexpr int16_t kSpacingButtonWidth = 212;
inline constexpr int16_t kSpacingButtonBack = 160;  // Back in words, the outline button alone at a footer's left
inline constexpr int16_t kSpacingCardInset = 20;
inline constexpr int16_t kSpacingCheckSize = 44;
inline constexpr int16_t kSpacingCheckDot = 16;
inline constexpr int16_t kSpacingIcon = 40;
inline constexpr int16_t kSpacingMark = 42;
inline constexpr int16_t kSpacingMarkDot = 26;
inline constexpr int16_t kSpacingProgress = 12;
inline constexpr int16_t kSpacingProgressLead = 16;
inline constexpr int16_t kSpacingStepGap = 12;
inline constexpr int16_t kSpacingPageDot = 8;
inline constexpr int16_t kSpacingPagePill = 28;
inline constexpr int16_t kSpacingScrollThumb = 8;
inline constexpr int16_t kSpacingScrollLength = 248;
inline constexpr int16_t kSpacingScrollX = 467;
inline constexpr int16_t kSpacingStatusMark = 85;
inline constexpr int16_t kSpacingStatusMarkSlot = 73;  // the widest compiled mark: every block's words stand at one x
inline constexpr int16_t kSpacingKeyGap = 12;
inline constexpr int16_t kSpacingChipSmall = 36;  // the small chip drawn; its hit area is padded to kSpacingTarget
inline constexpr int16_t kSpacingMascotCell = 24;  // the mascot's cell: two of its pixels, 12 px each, three whole dither tiles
inline constexpr int16_t kSpacingMarqueeStep = 48;  // how far a stepped line moves each step (DESIGN → Exceptions → Stepped lines on the deck's screens)
// The role tokens: the gaps between texts and between whole components, each one step of the scale (DESIGN → Layout → Spacing).
inline constexpr int16_t kSpacingPairSm = kSpacingSm;   // a row's label over its one line
inline constexpr int16_t kSpacingPairMd = kSpacingMd;   // a title over a wrapped description; a headline over body
inline constexpr int16_t kSpacingPairLg = kSpacingLg;   // a display or a hero over body
inline constexpr int16_t kSpacingPairXl = kSpacingXl;   // a modal's title over its message
inline constexpr int16_t kSpacingItem = kSpacingMd;     // row to row, button to button
inline constexpr int16_t kSpacingGroup = kSpacingLg;    // a dialog's message to its buttons; a group's name under a heading
inline constexpr int16_t kSpacingSection = kSpacing2xl; // above a section heading
inline constexpr int16_t kSpacingOverlayOffset = 6;
inline constexpr int16_t kSpacingStrokeHairline = 2;
inline constexpr int16_t kSpacingStrokeDefault = 3;
inline constexpr int16_t kSpacingStrokeFocus = 4;
inline constexpr int16_t kSpacingStrokeStrong = 4;
inline constexpr int16_t kSpacingStrokeEdit = 6;

// The scale: a level points at roles and spacing, a surface at a level. 0 flat — on the page; 1 raised — floats over the page and
// never takes it over (no surface yet); 2 anchored — a bottom sheet; 3 floating — a dialog. Denser is higher; one distance for all.
inline constexpr ElevationLevel kElevation0 = {kSpacingStrokeDefault, kNoShadow, 0, 0};
inline constexpr ElevationLevel kElevation1 = {kSpacingStrokeStrong, kColorShadow1, kSpacingOverlayOffset, kSpacingStrokeHairline};
inline constexpr ElevationLevel kElevation2 = {kSpacingStrokeStrong, kColorShadow2, kSpacingOverlayOffset, kSpacingStrokeHairline};
inline constexpr ElevationLevel kElevation3 = {kSpacingStrokeStrong, kColorShadow3, kSpacingOverlayOffset, kSpacingStrokeHairline};

// --- Touch: how a swipe turns a page (DESIGN → Focus and input → Buttons and touch; STICKY_DESIGN_TOKENS §10 owns the
// distances and the 100 ms window, held under a finger on a V1.0 and a V2.0) -------------------------------------------

inline constexpr int16_t kTouchSwipeDistance = kSpacingTarget;  // px of travel that commits a page turn: one target, 64
inline constexpr int16_t kTouchFlickDistance = 24;       // px a flick must have travelled at least
inline constexpr int16_t kTouchFlickMovement = 12;       // px moved in the last kTouchFlickWindowMs that make it a flick
inline constexpr uint32_t kTouchFlickWindowMs = 100;     // the end of the drag a flick is read from

// --- Motion: nothing animates; the delays are what the code waits (DESIGN → Motion) --------------------------------

inline constexpr uint32_t kMotionStateChangeMs = 0;
inline constexpr uint32_t kMotionSurfaceEnterMs = 0;
inline constexpr uint32_t kMotionSurfaceExitMs = 0;
inline constexpr uint32_t kMotionWaitIndicatorDelayMs = 1000;
inline constexpr uint32_t kMotionSelfUpdateDelayMs = 5000;
inline constexpr uint32_t kMotionClockTickDelayMs = 60000;
inline constexpr uint32_t kMotionPasswordRevealDelayMs = 1500;  // a password's last typed character shows this long, then a bullet
// The agent deck's three timed leaves (DESIGN → Exceptions → Timed leaves): the startup screen's stay, and "Answered" and "Timed out".
inline constexpr uint32_t kMotionStartupHoldDelayMs = 2000;
inline constexpr uint32_t kMotionAnsweredHoldDelayMs = 2300;
inline constexpr uint32_t kMotionTimedOutHoldDelayMs = 1800;
inline constexpr uint32_t kMotionKeyRepeatDelayMs = 600;   // a held UP or DOWN steps again once held this long
inline constexpr uint32_t kMotionKeyRepeatPeriodMs = 500;  // then this often while held, each step's frame landed first
inline constexpr uint32_t kMotionMarqueeRestDelayMs = 4000;  // a stepped line rests this long with its start in view before its passes, each time it begins
inline constexpr uint32_t kMotionMarqueeStepDelayMs = 1000;  // then moves kSpacingMarqueeStep this often, every line on one clock

// --- Refresh and feedback (STICKY_DESIGN_TOKENS §5) ------------------------------------------------------------------

inline constexpr int kRefreshPartialLimit = 20;
// A stepped line's counts, which are neither a size nor a time (DESIGN → Exceptions → Stepped lines on the deck's screens): the passes it
// makes before it rests, and the spaces between the end of its words and their start coming round.
inline constexpr int kMarqueePasses = 3;
inline constexpr int kMarqueeGapSpaces = 3;
inline constexpr uint32_t kFeedbackTickMs = 100;
inline constexpr uint32_t kWaitIndicatorDelayMs = kMotionWaitIndicatorDelayMs;
inline constexpr uint32_t kSelfUpdateMinMs = kMotionSelfUpdateDelayMs;
inline constexpr uint32_t kClockTickMs = kMotionClockTickDelayMs;

// --- Sound: the buzzer's words, in the agent deck's note format (DESIGN → Accessibility; STICKY_DESIGN_TOKENS §5) ------
// A note is {wave, hz, hz_end, ms, volume}: where it starts (0 a rest), where it slides to by its end (0 for none), its
// length, and on (255) or off (0) — the buzzer has no level but its duty. devices' note player plays them
// (STICKY_HARDWARE_SPECS §4.2).

enum class SoundWave : uint8_t {
    kPulse50,   // a square: the loudest at a pitch
    kPulse25,   // brighter, about 3 dB quieter
    kPulse12,   // the thinnest, about 8 dB quieter
    kTriangle,  // played as kPulse50: a switched coil has no triangle
    kNoise,     // the pitch hopping at random about `hz`
};

struct SoundNote {
    SoundWave wave;
    uint16_t hz;
    uint16_t hz_end;
    uint16_t ms;
    uint8_t volume;
};

inline constexpr uint32_t kSoundBandLowHz = 2000;   // sound.band-low: every sound is written at or above it
inline constexpr uint32_t kSoundBandHighHz = 6000;  // sound.band-high: and at or below it — round the 4 kHz resonance
inline constexpr uint32_t kSoundTickMaxMs = 40;     // sound.tick-max: a tick, on every press and every repeated step
inline constexpr uint32_t kSoundMaxMs = 600;        // sound.max: any other sound
inline constexpr uint32_t kSoundMomentsMaxMs = 460;  // sound.moments-max: the mascot's moments, in turn, within one flipbook frame

// sound.tick: every press, and every repeated step of a held key — the full square at the resonance, 30 ms.
inline constexpr SoundNote kSoundTick[] = {{SoundWave::kPulse50, 4000, 0, 30, 255}};
// sound.at-end: a step that can go no further — two ticks, lower than the press's (E7).
inline constexpr SoundNote kSoundAtEnd[] = {
    {SoundWave::kPulse50, 2637, 0, 25, 255}, {SoundWave::kPulse50, 0, 0, 45, 0}, {SoundWave::kPulse50, 2637, 0, 25, 255}};
// sound.error: that did not work — the agent deck's "womp womp" in C (C8, A♭7, F7 sliding to C7).
inline constexpr SoundNote kSoundError[] = {
    {SoundWave::kPulse50, 4186, 0, 150, 255}, {SoundWave::kPulse50, 0, 0, 50, 0},
    {SoundWave::kPulse50, 3322, 0, 150, 255}, {SoundWave::kPulse50, 0, 0, 50, 0},
    {SoundWave::kPulse50, 2794, 2093, 200, 255}};
// sound.success: only once the action succeeded — a rise of a fourth (G7, C8).
inline constexpr SoundNote kSoundSuccess[] = {{SoundWave::kPulse25, 3136, 0, 80, 255}, {SoundWave::kPulse25, 4186, 0, 180, 255}};
// sound.recording-start: BYTE-90's nudge moved up — a bip and a higher bip a fifth up (A7, E8).
inline constexpr SoundNote kSoundRecordingStart[] = {
    {SoundWave::kPulse25, 3520, 0, 70, 255}, {SoundWave::kPulse25, 0, 0, 45, 0}, {SoundWave::kPulse25, 5274, 0, 120, 255}};
// sound.recording-stop: the same pair falling (E8, A7).
inline constexpr SoundNote kSoundRecordingStop[] = {
    {SoundWave::kPulse25, 5274, 0, 70, 255}, {SoundWave::kPulse25, 0, 0, 45, 0}, {SoundWave::kPulse25, 3520, 0, 120, 255}};

// The mascot's moments (widgets::MascotSfx, in its order): the agent deck's kFx* sounds at the deck's own pitches — the
// person's ear, 2026-11-24, over the octaves moved into the band — on the note player, which takes any volume but 0 as on
// (DESIGN → Motion → Sound; STICKY_DESIGN_TOKENS §5).
inline constexpr SoundNote kSoundMascotBlink[] = {{SoundWave::kPulse12, 2093, 0, 18, 110}};
inline constexpr SoundNote kSoundMascotHappy[] = {{SoundWave::kPulse25, 2093, 0, 40, 150}, {SoundWave::kPulse25, 2637, 0, 60, 160}};
inline constexpr SoundNote kSoundMascotChevron[] = {{SoundWave::kPulse50, 520, 260, 60, 150}};
inline constexpr SoundNote kSoundMascotWink[] = {{SoundWave::kPulse12, 2637, 0, 25, 140}, {SoundWave::kPulse12, 0, 0, 20, 0}, {SoundWave::kPulse12, 3136, 0, 35, 150}};
inline constexpr SoundNote kSoundMascotLook[] = {{SoundWave::kPulse12, 2600, 0, 10, 80}};
inline constexpr SoundNote kSoundMascotHop[] = {{SoundWave::kPulse12, 420, 820, 60, 160}};
inline constexpr SoundNote kSoundMascotLand[] = {{SoundWave::kNoise, 3500, 0, 22, 130}};
inline constexpr SoundNote kSoundMascotSway[] = {{SoundWave::kNoise, 5000, 0, 80, 60}};
inline constexpr SoundNote kSoundMascotStretch[] = {{SoundWave::kPulse50, 300, 500, 120, 110}};
inline constexpr SoundNote kSoundMascotHand[] = {{SoundWave::kPulse25, 1319, 0, 22, 120}};
inline constexpr SoundNote kSoundMascotWipe[] = {{SoundWave::kNoise, 7000, 0, 70, 90}};
inline constexpr SoundNote kSoundMascotLaptopDown[] = {{SoundWave::kNoise, 1200, 0, 40, 170}};
inline constexpr SoundNote kSoundMascotLidOpen[] = {{SoundWave::kPulse25, 700, 1100, 35, 130}};
inline constexpr SoundNote kSoundMascotLidShut[] = {{SoundWave::kNoise, 2000, 0, 30, 170}};
inline constexpr SoundNote kSoundMascotFlag[] = {{SoundWave::kPulse12, 600, 1500, 70, 150}};
inline constexpr SoundNote kSoundMascotBurst[] = {{SoundWave::kPulse12, 2093, 0, 15, 130}, {SoundWave::kPulse12, 2637, 0, 15, 130}, {SoundWave::kPulse12, 3136, 0, 15, 135}, {SoundWave::kPulse12, 4186, 0, 25, 140}};
inline constexpr SoundNote kSoundMascotPuff[] = {{SoundWave::kNoise, 2000, 0, 40, 150}};
inline constexpr SoundNote kSoundMascotBead[] = {{SoundWave::kPulse12, 1800, 1400, 30, 100}};
inline constexpr SoundNote kSoundMascotPop[] = {{SoundWave::kPulse12, 900, 2400, 25, 170}};
inline constexpr SoundNote kSoundMascotStep[] = {{SoundWave::kNoise, 2600, 0, 25, 115}};
inline constexpr SoundNote kSoundMascotTurn[] = {{SoundWave::kNoise, 6000, 0, 45, 90}};
inline constexpr SoundNote kSoundMascotHandDown[] = {{SoundWave::kNoise, 2500, 0, 40, 160}};
inline constexpr SoundNote kSoundMascotCrouch[] = {{SoundWave::kPulse50, 220, 160, 40, 110}};
inline constexpr SoundNote kSoundMascotTopple[] = {{SoundWave::kPulse25, 640, 160, 110, 150}};
inline constexpr SoundNote kSoundMascotFlop[] = {{SoundWave::kNoise, 1800, 0, 30, 235}, {SoundWave::kPulse50, 140, 70, 75, 225}};
// The mascot's music (DESIGN → Motion → Sound), kept in time with a move, the deck's own: the typing looped while he types,
// the spin's, the success over the finish and the celebrate, the wave's "hi"s and the idle's thinking.
inline constexpr SoundNote kSoundMascotTyping[] = {{SoundWave::kNoise, 15000, 0, 22, 150}, {SoundWave::kNoise, 0, 0, 95, 0}, {SoundWave::kNoise, 15000, 0, 22, 140}, {SoundWave::kNoise, 0, 0, 120, 0}, {SoundWave::kNoise, 5000, 0, 38, 200}, {SoundWave::kNoise, 0, 0, 80, 0}, {SoundWave::kNoise, 5000, 0, 38, 190}, {SoundWave::kNoise, 0, 0, 160, 0}, {SoundWave::kNoise, 15000, 0, 22, 150}, {SoundWave::kNoise, 0, 0, 70, 0}, {SoundWave::kNoise, 15000, 0, 22, 135}, {SoundWave::kNoise, 0, 0, 85, 0}, {SoundWave::kNoise, 15000, 0, 22, 145}, {SoundWave::kNoise, 0, 0, 110, 0}, {SoundWave::kNoise, 2600, 0, 50, 190}, {SoundWave::kNoise, 0, 0, 260, 0}, {SoundWave::kNoise, 15000, 0, 22, 150}, {SoundWave::kNoise, 0, 0, 100, 0}, {SoundWave::kNoise, 5000, 0, 38, 195}, {SoundWave::kNoise, 0, 0, 90, 0}, {SoundWave::kNoise, 15000, 0, 22, 140}, {SoundWave::kNoise, 0, 0, 75, 0}, {SoundWave::kNoise, 15000, 0, 22, 150}, {SoundWave::kNoise, 0, 0, 95, 0}, {SoundWave::kNoise, 5000, 0, 38, 200}, {SoundWave::kNoise, 0, 0, 85, 0}, {SoundWave::kNoise, 5000, 0, 38, 185}, {SoundWave::kNoise, 0, 0, 520, 0}};
inline constexpr SoundNote kSoundMascotSpin[] = {{SoundWave::kPulse25, 0, 0, 490, 0}, {SoundWave::kPulse25, 1568, 0, 30, 170}, {SoundWave::kPulse25, 1319, 0, 30, 160}, {SoundWave::kPulse25, 1568, 0, 30, 170}, {SoundWave::kPulse25, 1319, 0, 30, 160}, {SoundWave::kPulse25, 1568, 0, 30, 170}, {SoundWave::kPulse25, 1319, 0, 30, 160}, {SoundWave::kPulse25, 1568, 0, 30, 170}, {SoundWave::kPulse25, 0, 0, 100, 0}, {SoundWave::kPulse25, 1047, 0, 70, 210}, {SoundWave::kPulse50, 1568, 0, 290, 230}};
inline constexpr SoundNote kSoundMascotSuccess[] = {{SoundWave::kPulse25, 784, 0, 80, 210}, {SoundWave::kPulse25, 1047, 0, 80, 210}, {SoundWave::kPulse25, 1319, 0, 80, 220}, {SoundWave::kPulse25, 1568, 0, 160, 230}, {SoundWave::kPulse25, 0, 0, 40, 0}, {SoundWave::kPulse25, 1319, 0, 90, 210}, {SoundWave::kPulse50, 1568, 0, 380, 230}};
inline constexpr SoundNote kSoundMascotWave[] = {{SoundWave::kPulse25, 0, 0, 330, 0}, {SoundWave::kPulse25, 1760, 0, 40, 150}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1568, 0, 40, 145}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1760, 0, 40, 150}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1568, 0, 40, 145}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1760, 0, 40, 150}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1568, 0, 40, 145}, {SoundWave::kPulse25, 0, 0, 1160, 0}, {SoundWave::kPulse25, 1760, 0, 40, 150}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1568, 0, 40, 145}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1760, 0, 40, 150}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1568, 0, 40, 145}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1760, 0, 40, 150}, {SoundWave::kPulse25, 0, 0, 160, 0}, {SoundWave::kPulse25, 1568, 0, 40, 145}};
inline constexpr SoundNote kSoundMascotThink[] = {{SoundWave::kTriangle, 0, 0, 700, 0}, {SoundWave::kTriangle, 196, 0, 110, 200}, {SoundWave::kTriangle, 0, 0, 90, 0}, {SoundWave::kTriangle, 165, 0, 110, 190}, {SoundWave::kTriangle, 0, 0, 90, 0}, {SoundWave::kTriangle, 196, 0, 130, 200}, {SoundWave::kTriangle, 0, 0, 1670, 0}, {SoundWave::kTriangle, 220, 0, 110, 200}, {SoundWave::kTriangle, 0, 0, 90, 0}, {SoundWave::kTriangle, 196, 0, 110, 190}, {SoundWave::kTriangle, 0, 0, 90, 0}, {SoundWave::kTriangle, 220, 0, 130, 200}};

// --- What the tokens' arithmetic keeps true, checked as the header compiles (STICKY_DESIGN_TOKENS §6) -------------
// A value changed alone that breaks one of these fails the build, not the glass.

inline constexpr int16_t kFocusRingReach = kSpacingFocusOffset + kSpacingStrokeFocus;  // how far a focus ring stands out: 8
static_assert(kSpacingItem >= 2 * kFocusRingReach, "two neighbours' rings never touch: the item gap holds both reaches");
static_assert((kSpacingFooter - kSpacingTarget) / 2 >= kFocusRingReach, "the footer holds its buttons' rings");
static_assert(kSpacingListRowHeight >= kSpacingTarget, "a list row is at least one target tall");
// A flick's last movement is part of its distance, so the movement threshold is never past it.
static_assert(kTouchFlickMovement <= kTouchFlickDistance, "a flick's last movement is part of its distance");

namespace detail {
// Every primitive above, kept by hand: a new one joins this list, or the grid check below does not see it.
inline constexpr TypeRole kScale[] = {
    kTypographyScale20Semibold,  kTypographyScale20Bold,     kTypographyScale20Regular, kTypographyScale30Regular,
    kTypographyScale30Semibold,
    kTypographyScale30Bold,      kTypographyScale40Semibold, kTypographyScale40Bold,    kTypographyScale50Semibold,
    kTypographyScale50Bold,      kTypographyScale60Semibold, kTypographyScale60Bold,    kTypographyScale70Semibold,
    kTypographyScale70Bold,      kTypographyScale80Semibold, kTypographyScale80Bold,    kTypographyScale100Semibold,
    kTypographyScale100Bold,     kTypographyScale120Semibold, kTypographyScale120Bold,  kTypographyScale160Semibold,
    kTypographyScale160Bold,
};
constexpr bool ScaleOnGrid()
{
    for (const TypeRole &r : kScale) {
        // Lines sit on the 4 px grid up to 80 px; figures (100 px and up) sit on their digits' height instead.
        if (r.size_px % 10 != 0 || (r.size_px <= 80 && r.line_px % 4 != 0)) {
            return false;
        }
    }
    return true;
}
// The mascot's sounds are the deck's at its own pitches, outside the band (DESIGN → Motion → Sound): only their length.
template <size_t N> constexpr bool LastsAtMost(const SoundNote (&notes)[N], uint32_t max_ms)
{
    uint32_t ms = 0;
    for (const SoundNote &n : notes) {
        ms += n.ms;
    }
    return ms <= max_ms;
}
template <size_t N> constexpr bool InBand(const SoundNote (&notes)[N], uint32_t max_ms)
{
    uint32_t ms = 0;
    for (const SoundNote &n : notes) {
        const bool rest = n.hz == 0 || n.volume == 0;  // silent, as the player takes it (sound_rules.h)
        const bool in_band = n.hz >= kSoundBandLowHz && n.hz <= kSoundBandHighHz &&
                             (n.hz_end == 0 || (n.hz_end >= kSoundBandLowHz && n.hz_end <= kSoundBandHighHz));
        if (!rest && !in_band) {
            return false;
        }
        ms += n.ms;
    }
    return ms <= max_ms;
}
}  // namespace detail

static_assert(detail::ScaleOnGrid(), "every type size is a multiple of 10 px and every line to 80 px a multiple of 4");
static_assert(detail::InBand(kSoundTick, kSoundTickMaxMs), "the tick is in the buzzer's band and at most tick-max");
static_assert(detail::InBand(kSoundAtEnd, kSoundMaxMs) && detail::InBand(kSoundError, kSoundMaxMs) &&
                  detail::InBand(kSoundSuccess, kSoundMaxMs) && detail::InBand(kSoundRecordingStart, kSoundMaxMs) &&
                  detail::InBand(kSoundRecordingStop, kSoundMaxMs),
              "every sound is in the buzzer's band and at most sound.max long");
static_assert(detail::LastsAtMost(kSoundMascotBlink, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotHappy, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotChevron, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotWink, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotLook, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotHop, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotLand, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotSway, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotStretch, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotHand, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotWipe, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotLaptopDown, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotLidOpen, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotLidShut, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotFlag, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotBurst, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotPuff, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotBead, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotPop, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotStep, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotTurn, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotHandDown, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotCrouch, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotTopple, kSoundMaxMs) &&
                  detail::LastsAtMost(kSoundMascotFlop, kSoundMaxMs),
              "every mascot moment is at most sound.max long, at the deck's own pitches");

}  // namespace design_tokens
