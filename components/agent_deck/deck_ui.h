// What the deck's screens share when they place things: the glass's size, where the mascot stands, a bare container and a line of words,
// and the mascot's greys. Layout only — every control is the kit's (DESIGN → The kit); every value a token (STICKY_AGENT_DECK §4).

#pragma once

#include <cstdint>

#include "design_tokens.h"
#include "lvgl.h"
#include "ui.h"
#include "widgets/chip.h"
#include "widgets/marquee.h"
#include "widgets/mascot.h"

#ifdef ESP_PLATFORM
#include "board_config.h"
#endif

namespace agent_deck::layout {

namespace dt = design_tokens;

inline constexpr int32_t kGlassW = 480;
inline constexpr int32_t kGlassH = 800;
#ifdef ESP_PLATFORM
static_assert(kGlassW == board_config::kScreenWidth && kGlassH == board_config::kScreenHeight, "the deck draws the board's portrait glass");
#endif

// The mascot: 10 x 8 cells of `spacing.mascot-cell`, centred across the glass and on its height, raised one target — the person's
// place for it on every page, 2026-11-25 — so the home view, an agent's page, the startup screen and a closed request hold it where
// the carousel does.
inline constexpr int32_t kMascotCell = dt::kSpacingMascotCell;
inline constexpr int32_t kMascotW = 10 * kMascotCell;
inline constexpr int32_t kMascotH = 8 * kMascotCell;
inline constexpr int32_t kMascotX = (kGlassW - kMascotW) / 2;
inline constexpr int32_t kMascotY = (kGlassH - kMascotH) / 2 - dt::kSpacingTarget;
// The jump rises 2.5 cells over the mascot's root (widgets::theme::kMascotHeadroom, the mascot header's).
inline constexpr int32_t kMascotHeadroom = 5 * kMascotCell / 2;

// The foot: the page dots over the icon buttons, `spacing.md` between, the foot `spacing.md` over the glass's edge.
inline constexpr int32_t kFootBottom = dt::kSpacingMd;
inline constexpr int32_t kFootTop = kGlassH - kFootBottom - dt::kSpacingTarget;          // 720
inline constexpr int32_t kDotsTop = kFootTop - dt::kSpacingMd - dt::kSpacingPageDot;     // 696
// The buttons of a screen's foot — Dismiss, Deny and Allow, Send, Back — stand `spacing.md` over the glass's edge too, and a ring `spacing.md`
// over them leaves the room the answers' rings need: the person's place for them, 2026-11-25.
inline constexpr int32_t kAnswersTop = kGlassH - dt::kSpacingMd - dt::kSpacingTarget - dt::kSpacingMd;  // 704
// Review: centred, its ring `spacing.md` over the page dots, whatever the facts above it hold.
inline constexpr int32_t kReviewTop = kDotsTop - dt::kSpacingMd - dt::kFocusRingReach - dt::kSpacingTarget;  // 608
// The facts column, under the mascot.
inline constexpr int32_t kFactsTop = kMascotY + kMascotH + dt::kSpacingMd;
// The chip, and a name above it: the chip's bottom `spacing.xs` over the jump's headroom, so a hop rises past it, the mascot in front
// (the person's: 24 px lower than the first mock, 2026-11-25, then 12 px up, 2026-11-26). The chip's top is y 140.
inline constexpr int32_t kChipTop = kMascotY - kMascotHeadroom - dt::kSpacingXs - dt::kSpacingChipSmall;
static_assert(kChipTop == 140, "the chip's top, as the person placed it");
// The clock, top-right, and the heading of the list pages.
inline constexpr int32_t kClockTop = dt::kSpacingMd;
inline constexpr int32_t kHeadingTop = dt::kSpacingTarget;  // the needs-you list's, the request's and the question's first line: one target down
inline constexpr int32_t kHalfW = (dt::kSpacingContent - dt::kSpacingMd) / 2;  // two answers, `spacing.md` apart
inline constexpr int32_t kHalfRightX = dt::kSpacingEdge + kHalfW + dt::kSpacingMd;  // the second of two halves

// A container that is nothing but a place for its children: no style, no scroll, no tap.
inline lv_obj_t *Bare(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    return o;
}

inline lv_obj_t *Column(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *o = Bare(parent);
    lv_obj_set_size(o, width, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    ui::LetRingOut(o);
    return o;
}

inline lv_obj_t *Row(lv_obj_t *parent, int32_t width = lv_pct(100))
{
    lv_obj_t *o = Bare(parent);
    lv_obj_set_size(o, width, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return o;
}

// One line of `role` as wide as its words, on the role's line box; `wrap` for words that may take lines, whole width. A one-line field that
// may be too long for its room is `Step`'s, below, never a line that ends in dots (DESIGN → Exceptions → Stepped lines on the deck's screens).
enum class Fit : uint8_t { kWords, kWrap };
inline lv_obj_t *Words(lv_obj_t *parent, ui::Type role, const char *text, Fit fit = Fit::kWords, bool secondary = false)
{
    lv_obj_t *label = ui::NewLabel(parent, role, text, secondary);
    switch (fit) {
    case Fit::kWords:
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
        break;
    case Fit::kWrap:
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(label, lv_pct(100));
        break;
    }
    ui::FitLineBox(label, role);
    return label;
}

// The screens that build themselves again from the model, for `widgets::MarqueeKey`: a line is the same field across a rebuild, so a pass
// under way goes on (widgets/marquee.h).
inline constexpr uint8_t kScopeHome = 1;
inline constexpr uint8_t kScopeList = 2;
inline constexpr uint8_t kScopeRequest = 3;
inline constexpr uint8_t kScopeWifi = 4;
inline constexpr uint8_t kScopeSettings = 5;

// A one-line field that may be too long for its room, as wide as its parent and on the role's line box: it stands still when it fits, aligned
// as asked, and steps along its words when it does not (widgets::MakeMarquee). `key` is the same field on the next rebuild; 0 for none.
inline lv_obj_t *Step(lv_obj_t *parent, ui::Type role, const char *text, uint32_t key = 0,
                      widgets::MarqueeAlign align = widgets::MarqueeAlign::kStart)
{
    return widgets::MakeMarquee(parent, role, text, {align, key, false});
}

// Supporting words in `text-quiet`, `caption` unless the screen gives another role, as wide as the line they stand under (DESIGN → Exceptions →
// Quiet words): a reset line, a sub-agent's tokens, a request's agent, and, in `caption-regular`, "4 sessions" under the needs-you heading.
inline lv_obj_t *Quiet(lv_obj_t *parent, const char *text, Fit fit = Fit::kWords, ui::Type role = ui::Type::kCaption)
{
    lv_obj_t *label = Words(parent, role, text, fit);
    ui::QuietWords(label);
    return label;
}

inline void MarginTop(lv_obj_t *o, int32_t px)
{
    lv_obj_set_style_margin_top(o, px, 0);
}

// A card's face for a row whose ring goes round it whole: the question cards' light dither, `rounded.lg`, so the box stands on the title's
// edge with its words `spacing.md` inside, and its ring stands in the margin, never on bare words (the person, 2026-11-26; DESIGN → Layout →
// Spacing). A chosen row's fill, a state's own style, still outranks it.
inline void CardFace(lv_obj_t *row)
{
    lv_obj_set_style_bg_color(row, ui::Fill(dt::kColorBand), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, dt::kRoundedLg, 0);
}

// The first words in `o`, itself when it is a label: the label a stepped line or a row of words holds.
inline lv_obj_t *WordsIn(lv_obj_t *o)
{
    if (o == nullptr || lv_obj_check_type(o, &lv_label_class)) {
        return o;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        if (lv_obj_t *found = WordsIn(lv_obj_get_child(o, static_cast<int32_t>(i)))) {
            return found;
        }
    }
    return nullptr;
}

// A line tight under its heading: `spacing.xs` between the two words' own boxes, the line boxes' leading taken back in the line's margin,
// so the tuck holds at any role's size (DESIGN → Layout → Spacing). Either may be a box round its words: a stepped line, a row.
inline void TuckUnder(lv_obj_t *heading, lv_obj_t *line)
{
    lv_obj_t *over = WordsIn(heading);
    lv_obj_t *under = WordsIn(line);
    const int32_t leading = (over != nullptr ? lv_obj_get_style_pad_bottom(over, LV_PART_MAIN) : 0) +
                            (under != nullptr ? lv_obj_get_style_pad_top(under, LV_PART_MAIN) : 0);
    MarginTop(line, dt::kSpacingXs - leading);
}

// The state chip, centred across the glass at `kChipTop`: a row the glass's width holds it, so its words changing keeps it centred. Home's
// every page and the frame the glass keeps while the deck is off stand it in this one place.
inline lv_obj_t *StateChip(lv_obj_t *parent, const char *text, bool control)
{
    lv_obj_t *row = Bare(parent);
    lv_obj_set_size(row, kGlassW, dt::kSpacingChipSmall);
    lv_obj_set_pos(row, 0, kChipTop);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui::LetRingOut(row);
    return widgets::MakeSmallChip(row, text, control);
}

// The mascot's greys: the body, shade and tint of a current chat, or of a previous one (DESIGN → Colors → The mascot's greys) — fills, so a
// grey draws as its dither.
inline void ColourMascot(const widgets::Mascot &m, bool previous_chat)
{
    if (previous_chat) {
        widgets::set_mascot_colour(m, ui::Fill(dt::kColorMascotFadedBody), ui::Fill(dt::kColorMascotFadedShade),
                                   ui::Fill(dt::kColorMascotFadedTint));
    } else {
        widgets::set_mascot_colour(m, ui::Fill(dt::kColorMascotBody), ui::Fill(dt::kColorMascotShade), ui::Fill(dt::kColorMascotTint));
    }
}

}  // namespace agent_deck::layout
