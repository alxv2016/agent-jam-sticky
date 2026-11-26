#include "widgets/words_role.h"

#include <initializer_list>
#include <vector>

#include "design_tokens.h"

namespace widgets {
namespace dt = design_tokens;

namespace {
// The size, in px, of one of ui's compiled Bitcount fonts: 0 for any other font.
int32_t SizeOf(const lv_font_t *font)
{
    for (const int16_t size : {20, 30, 40, 50, 60, 70, 80, 100, 120, 160}) {
        for (const int16_t weight : {400, 600, 700}) {
            if (ui::Bitcount(size, weight) == font) {
                return size;
            }
        }
    }
    return 0;
}

bool IsWords(lv_obj_t *obj)
{
    return lv_obj_check_type(obj, &lv_label_class) && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Sets one label to `role`: its font, its line space and its line box.
void Refit(lv_obj_t *label, ui::Type role)
{
    // Negative margins are a gap held between two texts' own boxes (internal/glue.h): they follow the new line box.
    const bool glued_top = lv_obj_get_style_margin_top(label, LV_PART_MAIN) < 0;
    const bool glued_bottom = lv_obj_get_style_margin_bottom(label, LV_PART_MAIN) < 0;
    const lv_font_t *font = ui::Font(role);
    const int32_t spare = ui::LineBox(role) - lv_font_get_line_height(font);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_line_space(label, spare > 0 ? spare : 0, 0);
    ui::FitLineBox(label, role);
    if (glued_top) {
        lv_obj_set_style_margin_top(label, -lv_obj_get_style_pad_top(label, LV_PART_MAIN), 0);
    }
    if (glued_bottom) {
        lv_obj_set_style_margin_bottom(label, -lv_obj_get_style_pad_bottom(label, LV_PART_MAIN), 0);
    }
}

// Brings `obj` and everything under it down to `role`; says whether any label came down.
bool Bring(lv_obj_t *obj, ui::Type role, int32_t role_size)
{
    bool brought = false;
    if (lv_obj_check_type(obj, &lv_label_class) && SizeOf(lv_obj_get_style_text_font(obj, LV_PART_MAIN)) > role_size) {
        brought = true;
        Refit(obj, role);
    }
    const uint32_t n = lv_obj_get_child_count(obj);
    std::vector<bool> child_brought;
    for (uint32_t i = 0; i < n; ++i) {
        child_brought.push_back(Bring(lv_obj_get_child(obj, static_cast<int32_t>(i)), role, role_size));
        brought = brought || child_brought.back();
    }
    // A name over its line in a column — a choice card's, a list row's, a detail row's — stands `pair-sm` between their own boxes
    // (DESIGN → Layout → Spacing), whichever role either has come to: the two line boxes' leading is made up in the name's margin.
    // A column with a gap of its own, or a gap held by a negative margin, keeps it.
    const bool column = (lv_obj_get_style_flex_flow(obj, LV_PART_MAIN) & LV_FLEX_COLUMN) != 0 &&
                        lv_obj_get_style_pad_row(obj, LV_PART_MAIN) == 0;
    for (uint32_t i = 0; column && i + 1 < n; ++i) {
        lv_obj_t *name = lv_obj_get_child(obj, static_cast<int32_t>(i));
        lv_obj_t *line = lv_obj_get_child(obj, static_cast<int32_t>(i + 1));
        if (IsWords(name) && IsWords(line) && (child_brought[i] || child_brought[i + 1]) &&
            lv_obj_get_style_margin_bottom(name, LV_PART_MAIN) >= 0 && lv_obj_get_style_margin_top(line, LV_PART_MAIN) >= 0) {
            const int32_t leading = lv_obj_get_style_pad_bottom(name, LV_PART_MAIN) + lv_obj_get_style_pad_top(line, LV_PART_MAIN);
            lv_obj_set_style_margin_bottom(name, leading < dt::kSpacingPairSm ? dt::kSpacingPairSm - leading : 0, 0);
        }
    }
    return brought;
}
}  // namespace

void SetWordsRole(lv_obj_t *obj, ui::Type role)
{
    Bring(obj, role, SizeOf(ui::Font(role)));
}

void SetLabelRole(lv_obj_t *label, ui::Type role)
{
    Refit(label, role);
}

}  // namespace widgets
