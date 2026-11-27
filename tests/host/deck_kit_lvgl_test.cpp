// Host test of what the agent deck's screens needed of the kit, on LVGL's real layout with the real widgets and fonts, on the
// computer with no board: the three type roles (`title`, `caption-strong`, `caption-regular`), a control's words one size down (SetWordsRole) with its row
// still centred on its height, the small chip's height, hit area and stop, the stacked status block's centring and gaps, the link
// indicator's faces and the foot's two icon buttons, a list row's body title over its `caption-regular` line, the network row's name in
// `caption-strong`, the joined network's row a card on the light dither, and a detail row's label in `caption-strong` over its value in `caption` (DESIGN → Components → The kit; STICKY_DESIGN_TOKENS §14).
// run.sh builds LVGL's sources from managed_components once, into build/host/lvgl, and the sources named below, against stand-ins
// for the ESP-IDF headers (tests/host/stubs).
// host-sources: components/ui/ui.cpp components/ui/line_box.cpp components/ui/fonts/bitcount/*.c components/widgets/marks/marks.c components/widgets/src/*.cpp components/widgets/src/internal/*.cpp components/router/router.cpp components/router/overlays.cpp components/router/focus.cpp components/router/touch.cpp components/router/routes.cpp components/router/screen.cpp components/router/status.cpp
// Build and run: bash tests/host/run.sh deck_kit

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "design_tokens.h"
#include "internal/marks.h"
#include "lvgl.h"
#include "lvgl_display.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/link_indicator.h"
#include "widgets/widgets.h"
#include "widgets/words_role.h"

namespace dt = design_tokens;

// What the widgets take from lvgl_display, which is not linked: the focus group, and the few calls the keyboard and the sheet make.
namespace {
lv_group_t *s_group = nullptr;
void (*s_back)() = nullptr;
}  // namespace
namespace lvgl_display {
lv_group_t *Group() { return s_group; }
void ClearGroup()
{
    lv_group_set_editing(s_group, false);
    lv_group_remove_all_objs(s_group);
}
// The router binds a screen's or an overlay's group here: the buttons and LVGL's default group follow it, as on the board.
void SetGroup(lv_group_t *group)
{
    if (group == nullptr) {
        return;
    }
    s_group = group;
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr; indev = lv_indev_get_next(indev)) {
        lv_indev_set_group(indev, group);  // a keypad's only: LVGL ignores it for a pointer
    }
    lv_group_set_default(group);
}
void SetBackHandler(void (*back)()) { s_back = back; }
// The router's read hooks, kept so a test can run a key read through the focus rules as LVGL's read would.
void (*s_key_hook)(lv_indev_t *, lv_indev_data_t *) = nullptr;
void (*s_touch_hook)(lv_indev_t *, lv_indev_data_t *) = nullptr;
void SetKeyReadHook(void (*hook)(lv_indev_t *, lv_indev_data_t *)) { s_key_hook = hook; }
void SetTouchReadHook(void (*hook)(lv_indev_t *, lv_indev_data_t *)) { s_touch_hook = hook; }
void SetPartialCounting(PartialCounting) {}
void NoteTyping() {}
void RequestFull() {}
void PlayCue(Cue) {}
}  // namespace lvgl_display

namespace {

int s_failures = 0;
int s_checks = 0;

void Check(bool ok, const std::string &what)
{
    ++s_checks;
    if (!ok) {
        ++s_failures;
        std::printf("FAIL  %s\n", what.c_str());
    }
}

void Flush(lv_display_t *display, const lv_area_t *, uint8_t *)
{
    lv_display_flush_ready(display);
}

uint32_t Ticks()
{
    return 0;
}

// --- Measuring ---------------------------------------------------------------------------------------------------------

struct Span {
    int32_t y1 = 1 << 20, y2 = -(1 << 20);
    bool any = false;
};

// The visible leaves under `obj`: an object with no child and a size is content — a label, an image, a bar, a thumb.
void Leaves(lv_obj_t *obj, Span &span, bool root)
{
    if (!root && lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    const uint32_t n = lv_obj_get_child_count(obj);
    if (n == 0 && !root) {
        lv_area_t a;
        lv_obj_get_coords(obj, &a);
        if (lv_area_get_width(&a) > 0 && lv_area_get_height(&a) > 0) {
            span.y1 = std::min(span.y1, a.y1);
            span.y2 = std::max(span.y2, a.y2);
            span.any = true;
        }
        return;
    }
    for (uint32_t i = 0; i < n; ++i) {
        Leaves(lv_obj_get_child(obj, static_cast<int32_t>(i)), span, false);
    }
}

// The offset, in px, of the content block's centre from the row's: twice the difference of the sums, halved with the odd pixel kept.
int Offset(lv_obj_t *row)
{
    lv_obj_update_layout(row);
    lv_area_t r;
    lv_obj_get_coords(row, &r);
    Span span;
    Leaves(row, span, true);
    if (!span.any) {
        return 1 << 20;
    }
    // Centres in half pixels, so an odd box is not rounded away: (y1 + y2 + 1) / 2 is the centre of an inclusive span.
    const int twice_content = span.y1 + span.y2 + 1;
    const int twice_row = r.y1 + r.y2 + 1;
    return (twice_content - twice_row) / 2;
}

// --- A page of rows ----------------------------------------------------------------------------------------------------

lv_obj_t *s_screen = nullptr;
lv_obj_t *s_column = nullptr;

void NewPage()
{
    if (s_screen != nullptr) {
        lvgl_display::ClearGroup();
        lv_obj_t *blank = lv_obj_create(nullptr);
        lv_screen_load(blank);
        lv_obj_delete(s_screen);  // LVGL's pool is 64 KB by default: a page goes before the next is built
    }
    s_screen = ui::NewScreen();
    s_column = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_column);
    lv_obj_set_width(s_column, lv_pct(100));
    lv_obj_set_flex_grow(s_column, 1);
    lv_obj_set_flex_flow(s_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(s_column, dt::kSpacingEdge, 0);
    lv_obj_set_style_pad_row(s_column, dt::kSpacingMd, 0);
    lv_obj_set_scrollable(s_column, false);
    lv_screen_load(s_screen);
}

void Measure(const std::string &what, lv_obj_t *row, int expected_height = -1)
{
    lv_obj_update_layout(s_screen);
    const int offset = Offset(row);
    const int height = lv_obj_get_height(row);
    std::printf("  %-58s row %3d px  offset %+d px\n", what.c_str(), height, offset);
    Check(std::abs(offset) <= 1, what + ": content centre within 1 px of the row's");
    if (expected_height > 0) {
        Check(height == expected_height, what + ": the row is " + std::to_string(expected_height) + " px high");
    }
}


constexpr const char *kLongName = "Keep my follow-ups in step with my phone, and tell me when they are done";

// --- Helpers -----------------------------------------------------------------------------------------------------------

// Every label under `obj`, itself included.
void Labels(lv_obj_t *obj, std::vector<lv_obj_t *> &out)
{
    if (lv_obj_check_type(obj, &lv_label_class)) {
        out.push_back(obj);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) {
        Labels(lv_obj_get_child(obj, static_cast<int32_t>(i)), out);
    }
}

std::vector<lv_obj_t *> LabelsOf(lv_obj_t *obj)
{
    std::vector<lv_obj_t *> out;
    Labels(obj, out);
    return out;
}

const lv_font_t *FontOf(lv_obj_t *label)
{
    return lv_obj_get_style_text_font(label, LV_PART_MAIN);
}

// A label is whole line boxes tall: the role's line height token, a line each (DESIGN → Components → The kit; STICKY_DESIGN_TOKENS §9).
bool OnLineBoxes(lv_obj_t *label, ui::Type role)
{
    lv_obj_update_layout(label);
    return lv_obj_get_height(label) % ui::LineBox(role) == 0 && lv_obj_get_height(label) > 0;
}

bool Same(lv_color_t a, lv_color_t b)
{
    return lv_color_eq(a, b);
}

// The glass as the display draws it: the L8 frame, read back after a full refresh.
uint8_t *s_frame = nullptr;
void Render()
{
    lv_obj_update_layout(s_screen);
    lv_obj_invalidate(s_screen);
    lv_refr_now(nullptr);
}

// The ink of a box, in the frame: the extent of every pixel darker than the light dither inside `area`.
struct Ink {
    int x1 = 1 << 20, x2 = -(1 << 20), y1 = 1 << 20, y2 = -(1 << 20);
    bool any = false;
};
Ink InkIn(const lv_area_t &area)
{
    Ink ink;
    for (int y = std::max(0, area.y1); y <= std::min(799, area.y2); ++y) {
        for (int x = std::max(0, area.x1); x <= std::min(479, area.x2); ++x) {
            if (s_frame[y * 480 + x] < 0x80) {
                ink.x1 = std::min(ink.x1, x);
                ink.x2 = std::max(ink.x2, x);
                ink.y1 = std::min(ink.y1, y);
                ink.y2 = std::max(ink.y2, y);
                ink.any = true;
            }
        }
    }
    return ink;
}

// --- The three roles -----------------------------------------------------------------------------------------------------

void TestRoles()
{
    std::printf("roles\n");
    Check(dt::kTypographyTitle.size_px == 30 && dt::kTypographyTitle.weight == 600 && dt::kTypographyTitle.line_px == 36,
          "title is scale-30-semibold on 36");
    Check(dt::kTypographyCaptionStrong.size_px == 26 && dt::kTypographyCaptionStrong.weight == 700 &&
              dt::kTypographyCaptionStrong.line_px == 32,
          "caption-strong is scale-26-bold on 32");
    Check(dt::kTypographyCaptionRegular.size_px == 26 && dt::kTypographyCaptionRegular.weight == 400 &&
              dt::kTypographyCaptionRegular.line_px == 32,
          "caption-regular is scale-26-regular on 32");
    Check(ui::Font(ui::Type::kCaptionRegular) == ui::Bitcount(26, 400), "caption-regular takes the compiled Bitcount 26 at 400");
    Check(ui::LineBox(ui::Type::kCaptionRegular) == 32 &&
              lv_font_get_line_height(ui::Font(ui::Type::kCaptionRegular)) <= ui::LineBox(ui::Type::kCaptionRegular),
          "caption-regular's line box is 32 and its font no taller");
    Check(ui::Font(ui::Type::kTitle) == ui::Bitcount(30, 600), "title takes the compiled Bitcount 30 at 600");
    Check(ui::Font(ui::Type::kCaptionStrong) == ui::Bitcount(26, 700), "caption-strong takes the compiled Bitcount 26 at 700");
    Check(ui::LineBox(ui::Type::kTitle) == 36 && ui::LineBox(ui::Type::kCaptionStrong) == 32, "the roles' line boxes");
    Check(lv_font_get_line_height(ui::Font(ui::Type::kTitle)) <= ui::LineBox(ui::Type::kTitle), "title's font is no taller than its line");
    Check(lv_font_get_line_height(ui::Font(ui::Type::kCaptionStrong)) <= ui::LineBox(ui::Type::kCaptionStrong),
          "caption-strong's font is no taller than its line");
    Check(dt::kSpacingChipSmall == 36, "chip-small is 36");
    Check(dt::kMotionStartupHoldDelayMs == 2000 && dt::kMotionAnsweredHoldDelayMs == 2300 && dt::kMotionTimedOutHoldDelayMs == 1800,
          "the three holds are 2 s, 2.3 s and 1.8 s");
}

// --- Words one size down -----------------------------------------------------------------------------------------------

// Every label that was larger than `role` is `role` on its line box; every label at or under its size is untouched, and the row is
// as high as it was and still centred.
void Down(const std::string &what, lv_obj_t *row, ui::Type role, int expected_height)
{
    const std::vector<lv_obj_t *> before = LabelsOf(row);
    std::vector<const lv_font_t *> fonts;
    for (lv_obj_t *l : before) {
        fonts.push_back(FontOf(l));
    }
    lv_obj_update_layout(s_screen);
    const int height_before = lv_obj_get_height(row);
    widgets::SetWordsRole(row, role);
    lv_obj_update_layout(s_screen);
    int changed = 0;
    for (size_t i = 0; i < before.size(); ++i) {
        const lv_font_t *font = FontOf(before[i]);
        if (fonts[i] == ui::Font(ui::Type::kLabel) || fonts[i] == ui::Font(ui::Type::kBody)) {
            Check(font == ui::Font(role), what + ": a 30 px label comes down to the role");
            Check(OnLineBoxes(before[i], role), what + ": and stands on the role's line boxes");
            ++changed;
        } else {
            Check(font == fonts[i], what + ": a label already at or under the role's size is left alone");
        }
    }
    Check(changed > 0, what + ": at least one label came down");
    Measure(what, row, expected_height);
    (void)height_before;
}

// A name over its line stands `pair-sm` between their own boxes, the font's height with no leading.
void CheckPair(const std::string &what, lv_obj_t *name, lv_obj_t *line)
{
    lv_obj_update_layout(s_screen);
    lv_area_t n, l;
    lv_obj_get_coords(name, &n);
    lv_obj_get_coords(line, &l);
    const int gap = (l.y1 + lv_obj_get_style_pad_top(line, LV_PART_MAIN)) - (n.y2 + 1 - lv_obj_get_style_pad_bottom(name, LV_PART_MAIN));
    Check(gap == dt::kSpacingPairSm, what + ": the name stands pair-sm over its line between their own boxes: " + std::to_string(gap));
}

void TestWordsDown()
{
    std::printf("words one size down\n");
    NewPage();
    widgets::ToggleRow toggle = widgets::MakeToggleRow(s_column, "Sound", true);
    Down("toggle row at caption", toggle.root, ui::Type::kCaption, 64);
    Check(FontOf(toggle.state) == ui::Font(ui::Type::kCaption), "the toggle row's state word is caption too");
    Check(lv_obj_get_height(toggle.state) == ui::LineBox(ui::Type::kCaption), "a dotted word is one line box high");
    widgets::ToggleRow two = widgets::MakeToggleRow(s_column, kLongName, true);
    Down("toggle row, two lines, at caption", two.root, ui::Type::kCaption, -1);
    widgets::ToggleRow reason = widgets::MakeToggleRow(s_column, "Background", false);
    widgets::SetToggleRowEnabled(reason, false, "Turn Wi-Fi on first");
    Down("toggle row, disabled, at caption", reason.root, ui::Type::kCaption, 96);  // the name, pair-sm, its reason in two lines at 26 px
    Check(FontOf(reason.reason) == ui::Font(ui::Type::kCaption), "the reason stays caption");
    Down("toggle row at caption-strong", widgets::MakeToggleRow(s_column, "Alerts", false).root, ui::Type::kCaptionStrong, 64);
    NewPage();
    widgets::ValueRow value = widgets::MakeValueRow(s_column, "Battery", "Charged \xE2\x80\xA2 84%");
    Down("value row at caption", value.root, ui::Type::kCaption, 64);
    lv_obj_t *disabled = widgets::MakeValueRow(s_column, "Backup", "Off").root;
    lv_obj_add_state(disabled, LV_STATE_DISABLED);
    Down("value row, disabled, at caption", disabled, ui::Type::kCaption, 64);
    static const char *const kZones[] = {"Pacific", "Mountain", "Central"};
    widgets::Dropdown *dropdown = widgets::MakeDropdown(s_column, "Time zone", kZones, 3, 0);
    Down("dropdown at caption", dropdown->frame, ui::Type::kCaption, -1);
    Measure("dropdown field", dropdown->field, 64);
    NewPage();
    lv_obj_t *radio = widgets::MakeRadioRow(s_column, "Pacific");
    Down("radio row at caption", radio, ui::Type::kCaption, 80);
    lv_obj_t *radio_long = widgets::MakeRadioRow(s_column, kLongName);
    Down("radio row, a long name, at caption", radio_long, ui::Type::kCaption, -1);
    lv_obj_t *list_row = widgets::MakeListRow(s_column, "Studio", "Joined", nullptr, nullptr);
    Down("list row at caption", list_row, ui::Type::kCaption, 80);
    CheckPair("list row at caption", lv_obj_get_child(list_row, 0), lv_obj_get_child(list_row, 1));
    lv_obj_t *card = widgets::MakeChoiceCard(s_column, "Allow once", false, "Only this request");
    Down("choice card at caption-strong", card, ui::Type::kCaptionStrong, -1);
    CheckPair("choice card at caption-strong", LabelsOf(card)[0], LabelsOf(card)[1]);
    std::vector<lv_obj_t *> card_words = LabelsOf(card);
    Check(card_words.size() == 2 && FontOf(card_words[0]) == ui::Bitcount(26, 700), "the card's name is 26 px bold");
    Check(card_words.size() == 2 && FontOf(card_words[1]) == ui::Font(ui::Type::kCaption), "and its detail line stays caption");
    NewPage();
    lv_obj_t *wrapped = widgets::MakeChoiceCard(s_column, "Keep the recording after it is transcribed, and share it", true, "Only on your Wi-Fi");
    Down("choice card, a long name, at caption-strong", wrapped, ui::Type::kCaptionStrong, -1);
    // A label set in the role already does not move, and asking again changes nothing.
    const int32_t y = lv_obj_get_y(lv_obj_get_child(card, 0));
    widgets::SetWordsRole(card, ui::Type::kCaptionStrong);
    lv_obj_update_layout(s_screen);
    Check(lv_obj_get_y(lv_obj_get_child(card, 0)) == y, "a second call changes nothing");
}

// A list row's title in another role: 80 px high, centred, the title on its role's line box.
void TestListRowTitle()
{
    std::printf("list row title\n");
    NewPage();
    lv_obj_t *plain = widgets::MakeListRow(s_column, "Claude", "Needs your review", nullptr, nullptr);
    lv_obj_t *body = widgets::MakeListRow(s_column, "Claude", "Needs your review", nullptr, nullptr, ui::Type::kBody);
    lv_obj_t *body_alone = widgets::MakeListRow(s_column, "Claude", nullptr, nullptr, nullptr, ui::Type::kBody);
    lv_obj_t *regular = widgets::MakeListRow(s_column, "Claude", "Needs your review", nullptr, nullptr, ui::Type::kBody,
                                             widgets::ListRowFit::kDots, 0, ui::Type::kCaptionRegular);
    lv_obj_t *stepped = widgets::MakeListRow(s_column, "Claude", "Bash \xE2\x80\xA2 24s", nullptr, nullptr, ui::Type::kBody,
                                             widgets::ListRowFit::kSteppedCounting, 0, ui::Type::kCaptionRegular);
    Check(FontOf(lv_obj_get_child(plain, 0)) == ui::Font(ui::Type::kLabel), "the default title is label");
    Check(FontOf(lv_obj_get_child(plain, 1)) == ui::Font(ui::Type::kCaption), "the default supporting line is caption");
    Check(FontOf(lv_obj_get_child(body, 0)) == ui::Bitcount(30, 400), "the title in body is 30 px regular");
    Check(FontOf(lv_obj_get_child(body, 1)) == ui::Font(ui::Type::kCaption), "its supporting line stays caption unless a role is asked");
    Check(FontOf(lv_obj_get_child(regular, 0)) == ui::Bitcount(30, 400), "the title over a caption-regular line is still body");
    Check(FontOf(lv_obj_get_child(regular, 1)) == ui::Bitcount(26, 400), "the supporting line in caption-regular is 26 px regular");
    Check(OnLineBoxes(lv_obj_get_child(regular, 1), ui::Type::kCaptionRegular), "and stands on caption-regular's 32 px line");
    Check(OnLineBoxes(lv_obj_get_child(body, 0), ui::Type::kBody), "the body title stands on body's 40 px line");
    lv_obj_t *stepped_line = lv_obj_get_child(lv_obj_get_child(lv_obj_get_child(stepped, 1), 0), 0);
    Check(FontOf(stepped_line) == ui::Bitcount(26, 400), "a stepped supporting line is in caption-regular too");
    Measure("list row, a body title and a line", body, 80);
    Measure("list row, a body title and a caption-regular line", regular, 80);
    Measure("list row, a stepped body title and caption-regular line", stepped, 80);
    Measure("list row, a body title alone", body_alone, 80);
    Measure("list row, the default", plain, 80);
}

// A detail row one size down: the label in caption-strong over its value in caption, the two `pair-sm` apart between their own boxes,
// and the row left alone is unchanged.
void TestDetailRow()
{
    std::printf("detail row one size down\n");
    NewPage();
    widgets::DetailRow plain = widgets::MakeDetailRow(s_column, "Host", "agentdeck.local");
    widgets::DetailRow small = widgets::MakeDetailRow(s_column, "Host", "agentdeck.local");
    widgets::SetDetailRowOneSizeDown(small);
    lv_obj_update_layout(s_screen);
    Check(lv_obj_get_height(plain.root) == 84, "the default detail row is 84 px high: 8 + 32 + 36 + 8");
    Check(FontOf(plain.label) == ui::Font(ui::Type::kCaption) && FontOf(plain.value) == ui::Font(ui::Type::kLabel),
          "its name is caption and its value label");
    Check(FontOf(small.label) == ui::Bitcount(26, 700), "the smaller one's label is 26 px bold");
    Check(FontOf(small.value) == ui::Bitcount(26, 600), "and its value is 26 px semibold");
    for (const widgets::DetailRow &row : {plain, small}) {
        lv_area_t name, value;
        lv_obj_get_coords(row.label, &name);
        lv_obj_get_coords(row.value, &value);
        const int gap = (value.y1 + lv_obj_get_style_pad_top(row.value, LV_PART_MAIN)) -
                        (name.y2 + 1 - lv_obj_get_style_pad_bottom(row.label, LV_PART_MAIN));
        const int leading = lv_obj_get_style_pad_bottom(row.label, LV_PART_MAIN) + lv_obj_get_style_pad_top(row.value, LV_PART_MAIN);
        Check(gap == std::max<int>(dt::kSpacingPairSm, leading),
              "the name stands pair-sm over its value between their own boxes, or their leading where more: " + std::to_string(gap));
    }
    Check(OnLineBoxes(small.label, ui::Type::kCaptionStrong), "the smaller label stands on caption-strong's line");
    Check(OnLineBoxes(small.value, ui::Type::kCaption), "the smaller value stands on caption's line");
    std::printf("  detail row %d px, smaller %d px\n", lv_obj_get_height(plain.root), lv_obj_get_height(small.root));
    Check(lv_obj_get_height(small.root) == 80, "the smaller row is 80 px: 8 + 32 + 32 + 8");
    widgets::SetDetailRowOneSizeDown(small);
    lv_obj_update_layout(s_screen);
    Check(lv_obj_get_height(small.root) == 80, "a second call changes nothing");
    widgets::DetailRow two = widgets::MakeDetailRow(s_column, "Host", "agentdeck-3f2a.local and its long name wraps to a second line");
    widgets::SetDetailRowOneSizeDown(two);
    lv_obj_update_layout(s_screen);
    Check(OnLineBoxes(two.value, ui::Type::kCaption), "a wrapped value stands on whole line boxes");
    Measure("detail row, a wrapped smaller value", two.root);
    Measure("detail row, the smaller value", small.root);
    widgets::SetDetailRow(small, "agentdeck.lan");
    Check(FontOf(small.label) == ui::Bitcount(26, 700) && FontOf(small.value) == ui::Bitcount(26, 600),
          "a value set again keeps the roles");
}

// --- The small chip ----------------------------------------------------------------------------------------------------

void TestSmallChip()
{
    std::printf("small chip\n");
    NewPage();
    lv_obj_t *row = lv_obj_create(s_column);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, dt::kSpacingLg, 0);
    ui::LetRingOut(row);
    const uint32_t stops = lv_group_get_obj_count(s_group);
    lv_obj_t *reading = widgets::MakeSmallChip(row, "2 working");
    Check(lv_group_get_obj_count(s_group) == stops, "a reading is no focus stop");
    Check(!lv_obj_has_flag(reading, LV_OBJ_FLAG_CLICKABLE), "and takes no press");
    lv_obj_t *control = widgets::MakeSmallChip(row, "1 needs you", true);
    Check(lv_group_get_obj_count(s_group) == stops + 1, "a control is one stop");
    Check(lv_obj_has_flag(control, LV_OBJ_FLAG_CLICKABLE), "and takes a press");
    lv_obj_update_layout(s_screen);
    for (lv_obj_t *chip : {reading, control}) {
        Check(lv_obj_get_height(chip) == dt::kSpacingChipSmall, "a small chip is 36 px high");
        Check(lv_obj_get_style_radius(chip, LV_PART_MAIN) == dt::kRoundedLg, "its corners are rounded.lg");
        Check(Same(lv_obj_get_style_bg_color(chip, LV_PART_MAIN), ui::Fill(dt::kColorBand)), "its fill is the light dither");
        lv_obj_t *label = lv_obj_get_child(chip, 0);
        Check(FontOf(label) == ui::Bitcount(30, 600), "its words are title, 30 px semibold");
        Check(Same(lv_obj_get_style_text_color(label, LV_PART_MAIN), ui::Level(dt::kColorTextPrimary)), "in black");
        Check(lv_obj_get_width(chip) == lv_obj_get_width(label) + 2 * dt::kSpacingMd, "spacing.md either side of the words");
        lv_area_t c, l;
        lv_obj_get_coords(chip, &c);
        lv_obj_get_coords(label, &l);
        Check(std::abs((l.y1 + l.y2) - (c.y1 + c.y2)) <= 1, "the words are centred on the chip's height");
        Check(std::abs((l.x1 + l.x2) - (c.x1 + c.x2)) <= 1, "and on its width");
        std::printf("  chip %d x %d px\n", lv_obj_get_width(chip), lv_obj_get_height(chip));
    }
    // The hit area of a control is a full target: 8 px more above and below the 48 drawn.
    lv_area_t c;
    lv_obj_get_coords(control, &c);
    const int32_t mid_x = (c.x1 + c.x2) / 2;
    lv_point_t above = {mid_x, c.y1 - (dt::kSpacingTarget - dt::kSpacingChipSmall) / 2};
    lv_point_t out = {mid_x, c.y1 - (dt::kSpacingTarget - dt::kSpacingChipSmall) / 2 - 1};
    lv_point_t below = {mid_x, c.y2 + (dt::kSpacingTarget - dt::kSpacingChipSmall) / 2};
    Check(lv_obj_hit_test(control, &above) && lv_obj_hit_test(control, &below), "a control is hit across spacing.target, 64 px");
    Check(!lv_obj_hit_test(control, &out), "and not past it");
    // The ring is the control's alone, outside it.
    lv_obj_add_state(control, LV_STATE_FOCUS_KEY);
    Check(lv_obj_get_style_outline_width(control, LV_PART_MAIN) == dt::kSpacingStrokeFocus, "a control draws the focus ring");
    lv_obj_remove_state(control, LV_STATE_FOCUS_KEY);
    Check(lv_obj_get_style_outline_width(reading, LV_PART_MAIN) == 0, "a reading never does");
    lv_obj_add_state(control, LV_STATE_DISABLED);
    Check(Same(lv_obj_get_style_text_color(control, LV_PART_MAIN), ui::Fill(dt::kColorTextDisabled)), "a disabled control's words are text-disabled");
    lv_obj_remove_state(control, LV_STATE_DISABLED);
}

// --- The stacked status block ------------------------------------------------------------------------------------------

void CheckStacked(const std::string &what, lv_obj_t *block, const lv_image_dsc_t *art, const char *heading, const char *line)
{
    Render();
    lv_area_t b;
    lv_obj_get_coords(block, &b);
    const int centre2 = b.x1 + b.x2 + 1;  // twice the block's centre, in half pixels
    lv_obj_t *mark = art != nullptr ? lv_obj_get_child(block, 0) : nullptr;
    lv_obj_t *title = lv_obj_get_child(block, art != nullptr ? 1 : 0);
    lv_obj_t *lines = lv_obj_get_child(block, art != nullptr ? 2 : 1);
    Check(lv_obj_get_child_count(block) == (art != nullptr ? 3u : 2u), what + ": the mark, the heading and the line");
    Check(lv_label_get_text(title) == std::string(heading) && lv_label_get_text(lines) == std::string(line), what + ": the words");
    Check(FontOf(title) == ui::Font(ui::Type::kLabel), what + ": the heading is label");
    Check(FontOf(lines) == ui::Font(ui::Type::kBody), what + ": the line is body");
    if (mark != nullptr) {
        lv_area_t m;
        lv_obj_get_coords(mark, &m);
        Check(lv_image_get_src(mark) == art, what + ": the mark's art");
        Check(std::abs((m.x1 + m.x2 + 1) - centre2) <= 1, what + ": the mark is centred across the block");
        lv_area_t t;
        lv_obj_get_coords(title, &t);
        Check(t.y1 - (m.y2 + 1) == dt::kSpacingItem, what + ": spacing.item between the mark and the heading's box");
    }
    for (lv_obj_t *words : {title, lines}) {
        Check(lv_obj_get_style_text_align(words, LV_PART_MAIN) == LV_TEXT_ALIGN_CENTER, what + ": the words are centred");
        lv_area_t w;
        lv_obj_get_coords(words, &w);
        Check(std::abs((w.x1 + w.x2 + 1) - centre2) <= 1, what + ": and their box is centred across the block");
        const Ink ink = InkIn(w);
        Check(ink.any && std::abs((ink.x1 + ink.x2 + 1) - centre2) <= 12, what + ": and so is their ink, to a glyph's side bearing (a full stop's ink stands left in its cell)");
        if (ink.any) {
            std::printf("  %s: ink %d..%d, block centre %d.%d\n", what.c_str(), ink.x1, ink.x2, centre2 / 2, (centre2 % 2) * 5);
        }
    }
    lv_area_t t, l;
    lv_obj_get_coords(title, &t);
    lv_obj_get_coords(lines, &l);
    const int gap = (l.y1 + lv_obj_get_style_pad_top(lines, LV_PART_MAIN)) - (t.y2 + 1 - lv_obj_get_style_pad_bottom(title, LV_PART_MAIN));
    Check(gap == dt::kSpacingPairMd, what + ": pair-md between the heading's and the line's own boxes: " + std::to_string(gap));
}

void TestStackedStatus()
{
    std::printf("stacked status block\n");
    NewPage();
    const char *wifi_line = "Connect asks your Mac for its network.";
    lv_obj_t *wifi = widgets::MakeStackedStatusBlock(s_column, "Not connected", wifi_line, widgets::StatusMark::kWifi);
    CheckStacked("wifi", wifi, &widgets_mark_status_wifi, "Not connected", wifi_line);
    Check(lv_obj_get_width(lv_obj_get_child(wifi, 0)) == 81 && lv_obj_get_height(lv_obj_get_child(wifi, 0)) == 60, "the Wi-Fi mark is 81 x 60 px");
    lv_obj_t *check = widgets::MakeStackedStatusBlock(s_column, "Answered", "in the terminal", widgets::StatusMark::kCheck);
    CheckStacked("check", check, &widgets_mark_status_check, "Answered", "in the terminal");
    lv_obj_t *clock = widgets::MakeStackedStatusBlock(s_column, "Timed out", "before an answer came", widgets::StatusMark::kClock);
    CheckStacked("clock", clock, &widgets_mark_status_clock, "Timed out", "before an answer came");
    lv_obj_t *none = widgets::MakeStackedStatusBlock(s_column, "Nothing here", "Come back later.");
    CheckStacked("no mark", none, nullptr, "Nothing here", "Come back later.");
    lv_obj_t *longer = widgets::MakeStackedStatusBlock(
        s_column, "Wrong password", "The network refused your Mac's password. Check it and try again from the Mac.", widgets::StatusMark::kLinkOff);
    CheckStacked("long line", longer, &widgets_mark_status_link_off, "Wrong password",
                 "The network refused your Mac's password. Check it and try again from the Mac.");
    Check(lv_obj_get_height(lv_obj_get_child(longer, 2)) >= 2 * ui::LineBox(ui::Type::kBody), "a long line wraps to a second line");
    // The beside block is as it was, and shows no Wi-Fi: the art is wider than its slot.
    lv_obj_t *beside = widgets::MakeStatusBlock(s_column, "Allowed", "Your answer reached the agent.", widgets::StatusMark::kWifi);
    lv_obj_update_layout(s_screen);
    Check(lv_obj_get_child(beside, 0) != nullptr && !lv_obj_check_type(lv_obj_get_child(beside, 0), &lv_image_class),
          "a beside block with the Wi-Fi mark shows none, so its words stay at one x");
}

// --- The foot ----------------------------------------------------------------------------------------------------------

void TestFoot()
{
    std::printf("foot icons\n");
    NewPage();
    lv_obj_t *foot = lv_obj_create(s_column);
    lv_obj_remove_style_all(foot);
    lv_obj_set_size(foot, lv_pct(100), dt::kSpacingTarget);
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(foot, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const uint32_t stops = lv_group_get_obj_count(s_group);
    lv_obj_t *link = widgets::MakeLinkIndicator(foot, true);
    Check(lv_group_get_obj_count(s_group) == stops, "the link indicator is out of the focus group");
    Check(!lv_obj_has_flag(link, LV_OBJ_FLAG_CLICKABLE), "and takes no press");
    Check(lv_image_get_src(link) == &widgets_mark_glyph_link_on, "up is the whole chain");
    lv_obj_update_layout(s_screen);
    Check(lv_obj_get_width(link) == 34 && lv_obj_get_height(link) == 18, "34 x 18 px up");
    widgets::SetLinkIndicator(link, false);
    lv_obj_update_layout(s_screen);
    Check(lv_image_get_src(link) == &widgets_mark_glyph_link_off, "down is the broken chain");
    Check(lv_obj_get_width(link) == 34 && lv_obj_get_height(link) == 32, "34 x 32 px down");
    Check(Same(lv_obj_get_style_image_recolor(link, LV_PART_MAIN), ui::Level(dt::kColorTextPrimary)), "in text-primary, never a grey");
    lv_area_t f, l;
    lv_obj_get_coords(foot, &f);
    lv_obj_get_coords(link, &l);
    Check(std::abs((l.y1 + l.y2) - (f.y1 + f.y2)) <= 1, "either face is centred on the foot's height");
    widgets::SetLinkIndicator(link, true);
    lv_obj_update_layout(s_screen);
    lv_obj_get_coords(link, &l);
    Check(std::abs((l.y1 + l.y2) - (f.y1 + f.y2)) <= 1, "and back again");
    lv_obj_t *buttons = lv_obj_create(foot);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(buttons, dt::kSpacingSm, 0);
    ui::LetRingOut(buttons);
    lv_obj_t *wifi = widgets::MakeIconButton(buttons, widgets::IconAction::kWifi, widgets::ButtonVariant::kTonal, "Open Wi-Fi");
    lv_obj_t *settings = widgets::MakeIconButton(buttons, widgets::IconAction::kSettings, widgets::ButtonVariant::kTonal, "Open settings");
    lv_obj_update_layout(s_screen);
    Check(lv_group_get_obj_count(s_group) == stops + 2, "the two icon buttons are stops");
    Check(lv_image_get_src(lv_obj_get_child(wifi, 0)) == &widgets_mark_glyph_wifi, "Wi-Fi draws the deck's Wi-Fi");
    Check(lv_image_get_src(lv_obj_get_child(settings, 0)) == &widgets_mark_glyph_settings, "Settings draws the deck's cog");
    for (lv_obj_t *b : {wifi, settings}) {
        Check(lv_obj_get_width(b) == dt::kSpacingTarget && lv_obj_get_height(b) == dt::kSpacingTarget, "an icon button is 64 px square");
        Check(Same(lv_obj_get_style_bg_color(b, LV_PART_MAIN), ui::Fill(dt::kColorBand)), "tonal: the light dither");
    }
    Check(std::string(widgets::IconButtonWords(wifi)) == "Open Wi-Fi", "the hint's words are kept");
}

// --- The network row as a card --------------------------------------------------------------------------------

void CheckInk(const std::string &what, lv_obj_t *row, lv_color_t words, lv_color_t marks)
{
    for (lv_obj_t *l : LabelsOf(row)) {
        Check(Same(lv_obj_get_style_text_color(l, LV_PART_MAIN), words), what + ": a word's colour: " + lv_label_get_text(l));
    }
    std::vector<lv_obj_t *> stack = {row};
    while (!stack.empty()) {
        lv_obj_t *o = stack.back();
        stack.pop_back();
        if (lv_obj_check_type(o, &lv_image_class) && lv_image_get_src(o) != &widgets_mark_glyph_signal_strong) {
            Check(Same(lv_obj_get_style_image_recolor(o, LV_PART_MAIN), marks), what + ": a glyph's colour");
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
            stack.push_back(lv_obj_get_child(o, static_cast<int32_t>(i)));
        }
    }
}

// The look is the row's for good: a change of state keeps it, and rest is the ordinary row either way. The words stand where they stood.
void TestNetworkLookKept(lv_obj_t *light)
{
    const lv_color_t black = ui::Level(dt::kColorTextPrimary);
    lv_area_t joined_name, rest_name;
    lv_obj_update_layout(s_screen);
    lv_obj_get_coords(lv_obj_get_child(light, 0), &joined_name);
    widgets::SetNetworkRow(light, true, 3, widgets::NetworkState::kRest);
    lv_obj_update_layout(s_screen);
    lv_obj_get_coords(lv_obj_get_child(light, 0), &rest_name);
    Check(!lv_obj_has_state(light, LV_STATE_CHECKED), "at rest it is not chosen");
    Check(Same(lv_obj_get_style_bg_color(light, LV_PART_MAIN), ui::Level(dt::kColorBackground)) && lv_obj_get_style_border_width(light, LV_PART_MAIN) == 0,
          "at rest its fill is the white page's and it has no edge");
    Check(joined_name.x1 == rest_name.x1 && joined_name.y1 == rest_name.y1 && lv_obj_get_height(light) == dt::kSpacingListRowHeight,
          "the name stands where it stood at rest");
    widgets::SetNetworkRow(light, true, 3, widgets::NetworkState::kConnected);
    Check(Same(lv_obj_get_style_bg_color(light, LV_PART_MAIN), ui::Fill(dt::kColorBand)) && lv_obj_get_style_border_width(light, LV_PART_MAIN) == 0,
          "joined again, the card again: the light dither, no edge");
    CheckInk("the card again", lv_obj_get_child(light, 1), black, black);
}

void TestNetworkLook()
{
    std::printf("network row as a card\n");
    NewPage();
    const lv_color_t black = ui::Level(dt::kColorTextPrimary), white = ui::Level(dt::kColorTextOnFill);
    // The default: the chosen inversion, white words.
    lv_obj_t *inverted = widgets::MakeNetworkRow(s_column, "Studio", true, 3, widgets::NetworkState::kConnected);
    Check(lv_obj_has_state(inverted, LV_STATE_CHECKED), "the default connected row is the inversion");
    CheckInk("inverted", lv_obj_get_child(inverted, 1), white, white);
    // The card look: the row on the light dither with no edge, every word and glyph black, the row not chosen.
    lv_obj_t *light = widgets::MakeNetworkRow(s_column, "Studio", true, 3, widgets::NetworkState::kConnected, widgets::NetworkLook::kCard);
    lv_obj_update_layout(s_screen);
    Check(!lv_obj_has_state(light, LV_STATE_CHECKED), "the card row is not the inversion");
    // The name is caption-strong over facts in caption, on every look and for a dotted or a stepped name.
    lv_obj_t *dotted_name = lv_obj_get_child(light, 0);
    Check(FontOf(dotted_name) == ui::Bitcount(26, 700), "a dotted network name is 26 px bold");
    Check(OnLineBoxes(dotted_name, ui::Type::kCaptionStrong), "and stands on caption-strong's line");
    Check(FontOf(lv_obj_get_child(inverted, 0)) == ui::Bitcount(26, 700), "the inverted row's name is 26 px bold");
    lv_obj_t *kept = widgets::MakeNetworkRow(s_column, "Studio", true, 3, widgets::NetworkState::kConnected, widgets::NetworkLook::kCard,
                                             widgets::NetworkName::kStepped, widgets::MarqueeKey(1, 1, 1));
    Check(FontOf(lv_obj_get_child(lv_obj_get_child(lv_obj_get_child(kept, 0), 0), 0)) == ui::Bitcount(26, 700), "a stepped network name is 26 px bold");
    lv_obj_update_layout(s_screen);
    Check(lv_obj_get_height(kept) == dt::kSpacingListRowHeight, "and its row is 80 px high");
    lv_obj_t *facts = lv_obj_get_child(light, 1);
    std::vector<lv_obj_t *> fact_words = LabelsOf(facts);
    Check(!fact_words.empty() && FontOf(fact_words[0]) == ui::Font(ui::Type::kCaptionRegular), "the facts are caption-regular: 26 px regular under the bold name");
    lv_obj_delete(kept);
    Check(Same(lv_obj_get_style_bg_color(light, LV_PART_MAIN), ui::Fill(dt::kColorBand)), "the card's fill is the light dither (the person, 2026-11-26)");
    Check(lv_obj_get_style_border_width(light, LV_PART_MAIN) == 0, "and it has no edge");
    Check(!Same(lv_obj_get_style_bg_color(light, LV_PART_MAIN), lv_obj_get_style_bg_color(inverted, LV_PART_MAIN)), "which is not the inversion's black fill");
    Check(Same(lv_obj_get_style_text_color(lv_obj_get_child(light, 0), LV_PART_MAIN), black), "its name is black");
    CheckInk("light", lv_obj_get_child(light, 1), black, black);
    Check(lv_obj_get_height(light) == dt::kSpacingListRowHeight && lv_obj_get_height(inverted) == dt::kSpacingListRowHeight, "both rows stay 80 px high");
    Measure("card row", light, 80);
    // A weaker signal: the unlit bars are the dark dither, which shows on the card's light ground.
    lv_obj_t *weak = widgets::MakeNetworkRow(s_column, "Office", true, 1, widgets::NetworkState::kConnecting, widgets::NetworkLook::kCard);
    lv_obj_t *canvas = lv_obj_get_child(weak, lv_obj_get_child_count(weak) - 1);
    Check(lv_obj_get_child_count(canvas) == 2, "an unlit and a lit glyph on the signal's canvas");
    Check(Same(lv_obj_get_style_image_recolor(lv_obj_get_child(canvas, 0), LV_PART_MAIN), ui::Fill(dt::kColorScalePrimary1)), "the unlit bars are the dark dither");
    Check(Same(lv_obj_get_style_image_recolor(lv_obj_get_child(canvas, 1), LV_PART_MAIN), black), "the lit bar is black");
    TestNetworkLookKept(light);
    widgets::SetNetworkRow(inverted, true, 3, widgets::NetworkState::kRest);
    widgets::SetNetworkRow(inverted, true, 3, widgets::NetworkState::kConnected);
    Check(lv_obj_has_state(inverted, LV_STATE_CHECKED), "an inverted row stays inverted");
}

// --- The quiet words ---------------------------------------------------------------------------------------------------

void TestQuietWords()
{
    std::printf("quiet words\n");
    NewPage();
    lv_obj_t *words = ui::NewLabel(s_column, ui::Type::kCaption, "Resets in 1h 12m");
    ui::QuietWords(words);
    Check(Same(lv_obj_get_style_text_color(words, LV_PART_MAIN), ui::Fill(dt::kColorScalePrimary1)), "quiet words are the dark dither's code");
    Check(!Same(lv_obj_get_style_text_color(words, LV_PART_MAIN), ui::Level(dt::kColorTextSecondary)), "which is not text-secondary's solid grey");
}

}  // namespace

int main()
{
    lv_init();
    lv_tick_set_cb(Ticks);
    lv_display_t *display = lv_display_create(480, 800);
    static uint8_t buffer[480 * 800];
    s_frame = buffer;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_L8);
    lv_display_set_buffers(display, buffer, nullptr, sizeof buffer, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, Flush);
    lv_display_set_theme(display, nullptr);  // as the firmware: no LVGL theme is compiled in
    s_group = lv_group_create();
    lv_group_set_default(s_group);
    if (ui::Start() != ESP_OK) {
        std::printf("ui::Start failed\n");
        return 1;
    }
    TestRoles();
    TestWordsDown();
    TestListRowTitle();
    TestDetailRow();
    TestSmallChip();
    TestStackedStatus();
    TestFoot();
    TestNetworkLook();
    TestQuietWords();
    std::printf("deck kit on LVGL: %d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
