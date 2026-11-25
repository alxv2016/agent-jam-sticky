// Host test that every header of the firmware's own compiles on the computer: each is included here, against LVGL's
// real headers and the stand-ins for the ESP-IDF ones (tests/host/stubs), so a header that no longer compiles off the
// board — a missing type, an include it needs and does not make before the ones above it — fails here.
// Build and run: bash tests/host/run.sh

#include <cstdio>

#include "../../components/app_hooks/include/app_hooks.h"
#include "../../components/app_shell/include/app_shell.h"
#include "../../components/chiptune/include/chiptune.h"
#include "../../components/hw_test/mascot_page.h"
#include "../../components/hw_test/sound_ab.h"
#include "../../components/lvgl_display/lvgl_display_priv.h"
#include "../../components/router/include/router/route.h"
#include "../../components/router/include/router/screen.h"
#include "../../components/router/router_internal.h"
#include "../../components/widgets/src/internal/base.h"
#include "../../components/widgets/src/internal/button_face.h"
#include "../../components/widgets/src/internal/icon_button_sized.h"
#include "../../components/widgets/src/internal/mascot_frames.h"
#include "../../components/widgets/src/internal/secure_zero.h"
#include "../../components/widgets/src/internal/setting_row.h"
#include "widgets/button.h"
#include "widgets/carousel.h"
#include "widgets/checkbox.h"
#include "widgets/checkbox_row.h"
#include "widgets/chip.h"
#include "widgets/choice_card.h"
#include "widgets/detail_row.h"
#include "widgets/dialog.h"
#include "widgets/dropdown.h"
#include "widgets/glyph.h"
#include "widgets/horizontal_pager.h"
#include "widgets/icon_button.h"
#include "widgets/keyboard.h"
#include "widgets/list_row.h"
#include "widgets/list_view.h"
#include "widgets/network_row.h"
#include "widgets/page_dots.h"
#include "widgets/progress_bar.h"
#include "widgets/radio.h"
#include "widgets/radio_button_row.h"
#include "widgets/radio_row.h"
#include "widgets/range_slider.h"
#include "widgets/roller.h"
#include "widgets/scroll_indicator.h"
#include "widgets/section_heading.h"
#include "widgets/segment.h"
#include "widgets/select_sheet.h"
#include "widgets/status_block.h"
#include "widgets/status_line.h"
#include "widgets/step_bar.h"
#include "widgets/text_input.h"
#include "widgets/text_view.h"
#include "widgets/toggle.h"
#include "widgets/toggle_icon_button.h"
#include "widgets/toggle_icon_row.h"
#include "widgets/toggle_row.h"
#include "widgets/value_row.h"

int main()
{
    std::printf("headers: the firmware's headers compile on the host\n");
    return 0;
}
