// A swipe page: a container that turns a page when the finger sweeps across it — sideways for the agent deck's carousel, up or down for a
// band of whole items it pages itself (DESIGN → Layout → Paging; STICKY_DESIGN_TOKENS §10). Nothing follows the finger: the page behind never moves, and one swipe,
// decided when the finger lifts, turns one page by its distance or a flick at its end (the swipe the list view, the text view and
// the pager share). It is no control and no focus stop; the keys turn the pages their own way.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace widgets {

// Which way the pages run: a carousel's across the glass, a list's down it.
enum class SwipeAxis : uint8_t { kHorizontal, kVertical };

// A swipe left (`dir` +1) turns to the next page, a swipe right (-1) to the one before; on a vertical band a swipe up is +1 and down -1.
// Runs under the lock, from the lift.
using SwipePageTurn = void (*)(int32_t dir, void *user);

// A `width` by `height` container in `parent`, bare — no fill, no border, no hit of its own beyond the swipe — that claims a drag along
// its axis begun anywhere on it or on a child that does not take it (a button takes a tap, and its press is the swipe's too: a finger that
// slides off a control past its hit area presses nothing, and the swipe it became turns the page). It ignores its parent's layout: the
// caller places it. A drag across its axis chains to the page behind. Its children are the page's own, placed by the caller, and a
// vertical band scrolls them to a page itself (lv_obj_scroll_to_y, no animation); let the rings of a control inside it out
// (`ui::LetRingOut`).
lv_obj_t *MakeSwipePage(lv_obj_t *parent, int32_t width, int32_t height, SwipePageTurn turn, void *user,
                        SwipeAxis axis = SwipeAxis::kHorizontal);

}  // namespace widgets
