#include "deck_clock.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "deck_rules.h"
#include "deck_ui.h"
#include "devices.h"

namespace agent_deck {

void Clock::Build(lv_obj_t *parent)
{
    label_ = layout::Words(parent, ui::Type::kCaption, "");
    lv_obj_align(label_, LV_ALIGN_TOP_RIGHT, -layout::dt::kSpacingEdge, layout::kClockTop);
    Update();
}

void Clock::Update()
{
    if (label_ == nullptr) {
        return;
    }
    std::tm now{};
    char text[32] = {};
    if (devices::ReadClock(&now) == ESP_OK) {
        rules::FormatClock(text, sizeof text, now);
    }
    if (std::strcmp(text, text_) != 0) {
        std::snprintf(text_, sizeof text_, "%s", text);
        lv_label_set_text(label_, text);
    }
}

}  // namespace agent_deck
