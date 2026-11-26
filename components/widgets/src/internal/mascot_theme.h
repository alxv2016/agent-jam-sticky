// The mascot's side of the theme, under the names the agent deck's mascot.cpp reads them by (its deck/theme.h and
// deck/theme_layout.h), so the widget is ported with its numbers unchanged: the cell and the colours from DESIGN.md's tokens
// (design_tokens.h `kSpacingMascotCell`, `kColorMascot*`), the glass's portrait size (board_config.h's, asserted in
// mascot.cpp, which the host tests build without it), and the moves' lengths — the
// deck's own, the mascot's animation data like its frames, played at the deck's pace and seen at the glass's, about two
// frames a second on either board (MASCOT §6; STICKY_DESIGN_TOKENS §13). Not a token header: the moves' lengths are no
// design value of this glass; where the moves play, "Nothing animates" gives way to DESIGN → Exceptions → The
// hardware-test demo.

#pragma once

#include <cstdint>

#include "design_tokens.h"
#include "lvgl.h"
#include "ui.h"
#include "widgets/mascot.h"

namespace widgets::theme {

namespace dt = design_tokens;

// --- The grid and where the mascot stands (the deck's landscape theme_layout.h, centred on this portrait glass) -------

inline constexpr int32_t kMascotCols = 10;
inline constexpr int32_t kMascotRows = 8;
inline constexpr int32_t kMascotCell = dt::kSpacingMascotCell;
static_assert(kMascotCell % 2 == 0, "a mascot pixel is half a cell");
inline constexpr int32_t kMascotJumpRise = 2 * kMascotCell;
// The room above the mascot its hops and the finish's flag rise into; a page that shows it leaves this much clear.
inline constexpr int32_t kMascotHeadroom = 5 * kMascotCell / 2;
static_assert(kMascotJumpRise <= kMascotHeadroom, "the jump rises past the room above the mascot");
// The idle's skip trips now and then, anywhere on its band; the round glass's trip only homeward is not this glass's.
inline constexpr bool kMascotTrips = true;
inline constexpr bool kMascotTripsHomeward = false;

inline constexpr int32_t kScreenWidth = dt::kSpacingContent + 2 * dt::kSpacingEdge;  // 480
inline constexpr int32_t kScreenHeight = 800;
// Centred across the portrait glass, no column (the person's ask, 2026-11-24), a move free to go off its edge; its
// height the page's — where the hardware test's mascot page stands it (mascot.h kMascotPage*,
// STICKY_DESIGN_TOKENS §13).
inline constexpr int32_t kMascotCentreX = kMascotPageCentreX;
inline constexpr int32_t kMascotCentreY = kMascotPageCentreY;
static_assert(kMascotCentreX == kScreenWidth / 2, "centred across the portrait glass; a move may go off it (mascot.h)");
// No blink at rest: on this glass a page at rest changes nothing, so it sends no refresh (MASCOT §6, no idle loop).
inline constexpr bool kMascotBlinksAtRest = false;
inline constexpr int32_t kMascotTripTop = kMascotCentreY - kMascotRows * kMascotCell / 2;

// --- The moves' lengths: the deck's (its theme.h), unchanged but where this glass's route at the 24 px cell is fewer
// steps — the walk, the skip and its trip, each the widget's own arithmetic, no key or pace touched (mock 4, 2026-11-24) --

inline constexpr uint32_t kMascotJumpMs = 64 * 1000 / 30;
inline constexpr uint32_t kMascotWorkMs = 3140;
inline constexpr uint32_t kMascotSweatMs = 2540;
inline constexpr uint32_t kMascotFaceplantMs = 4452;
inline constexpr uint32_t kMascotFinishMs = 5602;
inline constexpr uint32_t kMascotCelebrateMs = 2143;
inline constexpr uint32_t kMascotIdleMs = kMascotIdleCycleMs;  // mascot.h: a page that plays one cycle reads it there
inline constexpr uint32_t kMascotWalkMs = 11300;     // the deck's 13,700, its route fewer of this cell's steps
inline constexpr uint32_t kMascotLookMs = kMascotLookCycleMs;
inline constexpr uint32_t kMascotSkipMs = 7370;      // the deck's 9,270, likewise
inline constexpr uint32_t kMascotSkipTripMs = 10660;  // the deck's 12,560, likewise
inline constexpr uint32_t kMascotSpinMs = 1800;
inline constexpr uint32_t kMascotDustMs = 1700;
inline constexpr uint32_t kMascotWaveMs = 5400;

// --- The colours: DESIGN → Colors → The mascot's greys, each a fill so a partial refresh draws its dither -------------

// The glass behind the mascot, which its eyes are holes to.
inline lv_color_t background() { return ui::Fill(dt::kColorBackground); }
inline lv_color_t mascot_body() { return ui::Fill(dt::kColorMascotBody); }
inline lv_color_t mascot_shade() { return ui::Fill(dt::kColorMascotShade); }
inline lv_color_t mascot_tint() { return ui::Fill(dt::kColorMascotTint); }
inline lv_color_t mascot_laptop() { return ui::Fill(dt::kColorMascotLaptop); }
inline lv_color_t mascot_faded_body() { return ui::Fill(dt::kColorMascotFadedBody); }
inline lv_color_t mascot_faded_shade() { return ui::Fill(dt::kColorMascotFadedShade); }
inline lv_color_t mascot_faded_tint() { return ui::Fill(dt::kColorMascotFadedTint); }
inline lv_color_t mascot_flag() { return ui::Fill(dt::kColorMascotFlag); }
inline lv_color_t mascot_flag_check() { return ui::Fill(dt::kColorMascotFlagCheck); }
inline lv_color_t mascot_flag_pole() { return ui::Fill(dt::kColorMascotFlagPole); }
inline lv_color_t mascot_dust() { return ui::Fill(dt::kColorMascotDust); }
inline lv_color_t mascot_dust_faint() { return ui::Fill(dt::kColorMascotDustFaint); }
inline lv_color_t mascot_sweat() { return ui::Fill(dt::kColorMascotSweat); }
// The deck's three confetti hues as two greys, in turn; `i` any integer.
inline lv_color_t mascot_confetti(int i)
{
    return ui::Fill(((i % 2) + 2) % 2 == 0 ? dt::kColorMascotConfettiA : dt::kColorMascotConfettiB);
}

}  // namespace widgets::theme
