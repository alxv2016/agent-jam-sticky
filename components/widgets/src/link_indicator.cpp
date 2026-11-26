#include "widgets/link_indicator.h"

#include "design_tokens.h"
#include "internal/base.h"
#include "internal/marks.h"

namespace widgets {
namespace dt = design_tokens;

lv_obj_t *MakeLinkIndicator(lv_obj_t *parent, bool up)
{
    // Not clickable and never in the group: NewMark makes an image that takes no press (DESIGN → Exceptions → The link indicator).
    lv_obj_t *indicator = detail::NewMark(parent, up ? &widgets_mark_glyph_link_on : &widgets_mark_glyph_link_off,
                                          dt::kColorTextPrimary, dt::kColorTextPrimary);
    return indicator;
}

void SetLinkIndicator(lv_obj_t *indicator, bool up)
{
    const lv_image_dsc_t *face = up ? &widgets_mark_glyph_link_on : &widgets_mark_glyph_link_off;
    if (lv_image_get_src(indicator) != face) {
        lv_image_set_src(indicator, face);
    }
}

}  // namespace widgets
