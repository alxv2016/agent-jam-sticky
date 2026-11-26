#include "off_frame.h"

#include "deck_ui.h"

namespace agent_deck {

void BuildOffFrame(lv_obj_t *screen, power_leave::How)
{
    using namespace layout;
    ui::StyleScreen(screen);
    lv_obj_t *page = Bare(screen);
    lv_obj_set_ignore_layout(page, true);
    lv_obj_set_size(page, kGlassW, kGlassH);
    lv_obj_set_pos(page, 0, 0);
    ui::LetRingOut(page);
    // The state chip where every page's chip stands, the mascot at rest in its place and the line under it, as the standby page has them.
    StateChip(page, "Off", false);
    const widgets::Mascot mascot = widgets::make_mascot(page, kMascotCell);
    lv_obj_set_pos(mascot.root, kMascotX, kMascotY);
    lv_obj_t *column = Column(page, dt::kSpacingContent);
    lv_obj_set_pos(column, dt::kSpacingEdge, kFactsTop);
    lv_obj_t *words = Words(column, ui::Type::kCaption, "Hold OK to turn on.", Fit::kWrap);
    lv_obj_set_style_text_align(words, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_move_foreground(mascot.root);  // in front of every element of its page, as everywhere (DESIGN → Exceptions)
}

}  // namespace agent_deck
