#include "widgets/status_block.h"

#include "design_tokens.h"
#include "internal/base.h"
#include "internal/glue.h"
#include "internal/marks.h"

namespace widgets {
namespace dt = design_tokens;

namespace {
const lv_image_dsc_t *Art(StatusMark mark)
{
    switch (mark) {
    case StatusMark::kCheck:   return &widgets_mark_status_check;
    case StatusMark::kClock:   return &widgets_mark_status_clock;
    case StatusMark::kLinkOff: return &widgets_mark_status_link_off;
    case StatusMark::kWifi:    return &widgets_mark_status_wifi;
    case StatusMark::kNone:    break;
    }
    return nullptr;
}
}  // namespace

lv_obj_t *MakeStatusBlock(lv_obj_t *parent, const char *heading, const char *description, StatusMark mark)
{
    // The Wi-Fi art is 81 px wide and the slot 73: it would be cut, and the words' x must not move, so a beside block shows none.
    const lv_image_dsc_t *art = mark == StatusMark::kWifi ? nullptr : Art(mark);
    lv_obj_t *block = detail::Bare(parent);  // the mark's slot is `status-mark-slot`: one x for the words
    lv_obj_set_size(block, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(block, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(block, dt::kSpacingLg, 0);
    if (art == nullptr) {
        lv_obj_t *empty = detail::Bare(block);  // the slot stays empty: the words stand where a mark would put them
        lv_obj_set_size(empty, dt::kSpacingStatusMarkSlot, 1);
    } else {
        lv_obj_t *image = lv_image_create(block);
        lv_image_set_src(image, art);
        lv_obj_set_width(image, dt::kSpacingStatusMarkSlot);  // the art flush left in its slot
        lv_image_set_inner_align(image, LV_IMAGE_ALIGN_TOP_LEFT);
        lv_obj_set_style_image_recolor(image, ui::Level(dt::kColorTextPrimary), 0);
        lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
    }
    lv_obj_t *words = detail::Bare(block);
    lv_obj_set_flex_flow(words, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(words, 1);
    lv_obj_set_size(words, 1, LV_SIZE_CONTENT);  // the grow takes the room the mark leaves; the words wrap inside it
    lv_obj_set_style_pad_row(words, dt::kSpacingPairMd, 0);  // a title over a wrapped description, between their own boxes
    lv_obj_t *title = detail::Words(words, ui::Type::kLabel, heading);
    lv_obj_t *lines = detail::Words(words, ui::Type::kBody, description, true);
    detail::Glue(title, lines);
    return block;
}

lv_obj_t *MakeStackedStatusBlock(lv_obj_t *parent, const char *heading, const char *description, StatusMark mark)
{
    lv_obj_t *block = detail::Bare(parent);
    lv_obj_set_size(block, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(block, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // every part centred across
    lv_obj_set_style_pad_row(block, dt::kSpacingItem, 0);  // the mark over its heading; the heading's gap to its line is set below
    if (const lv_image_dsc_t *art = Art(mark)) {
        detail::NewMark(block, art, dt::kColorTextPrimary, dt::kColorTextPrimary);
    }
    lv_obj_t *title = detail::Words(block, ui::Type::kLabel, heading);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *lines = detail::Words(block, ui::Type::kBody, description, true);
    lv_obj_set_style_text_align(lines, LV_TEXT_ALIGN_CENTER, 0);
    detail::Glue(title, lines);
    lv_obj_set_style_margin_bottom(title, lv_obj_get_style_margin_bottom(title, LV_PART_MAIN) + dt::kSpacingPairMd - dt::kSpacingItem, 0);
    return block;
}

}  // namespace widgets
