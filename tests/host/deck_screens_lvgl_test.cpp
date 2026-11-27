// Host test of the agent deck's screens on LVGL's real layout, router, widgets and fonts, on the computer with no board (components/agent_deck):
// the startup screen and its 2 s hold; the carousel's overview and agent pages against the approved positions — the chip, the mascot, the
// figures, the bars, the page dots and the foot — and the mascot in front of every element; the walk, which turns a page on UP and DOWN
// through the guards and reaches the foot at the ends and from them; OK on a Needs you page opening its request; a sideways swipe; the
// needs-you list; a request, its focus and where each answer leaves to; a question, its Send, its pages; a request closed elsewhere,
// in place, and its timed leave; each route's factory as its screen's header declares it, the closed page and the clock built by
// themselves, and a page's mascot voice (STICKY_AGENT_DECK §4, §5). DECK_DUMP=<dir> writes each frame as the glass reads it, for a look.
// run.sh builds LVGL's sources from managed_components once, into build/host/lvgl, and the sources named below.
// host-sources: components/ui/ui.cpp components/ui/line_box.cpp components/ui/fonts/bitcount/*.c components/widgets/marks/marks.c components/widgets/src/*.cpp components/widgets/src/internal/*.cpp components/router/router.cpp components/router/overlays.cpp components/router/focus.cpp components/router/touch.cpp components/router/routes.cpp components/router/screen.cpp components/router/status.cpp components/app_hooks/app_hooks.cpp components/app_shell/screen_home.cpp components/mascot_voice/mascot_voice.cpp components/agent_deck/*.cpp components/agent_deck/logo.c
// Build and run: bash tests/host/run.sh deck_screens

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "app_hooks.h"
#include "src/misc/lv_text_private.h"  // lv_text_get_width: whether a line of words stands whole in its room
#include "../../components/agent_deck/answer_screens.h"
#include "../../components/agent_deck/closed_page.h"
#include "../../components/agent_deck/deck_clock.h"
#include "../../components/agent_deck/deck_flow.h"
#include "../../components/agent_deck/deck_mock.h"
#include "../../components/agent_deck/deck_model.h"
#include "../../components/agent_deck/deck_rules.h"
#include "../../components/agent_deck/deck_stage.h"
#include "../../components/agent_deck/deck_ui.h"
#include "../../components/agent_deck/home_screen.h"
#include "../../components/agent_deck/off_frame.h"
#include "../../components/agent_deck/settings_screen.h"
#include "../../components/agent_deck/waiting_screen.h"
#include "../../components/agent_deck/wifi_screen.h"
#include "../../components/app_shell/screen_home.h"
#include "design_tokens.h"
#include "devices.h"
#include "mascot_voice.h"
#include "power_leave.h"
#include "lvgl.h"
#include "lvgl_display.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/marquee.h"
#include "widgets/widgets.h"

namespace dt = design_tokens;
using namespace agent_deck;
namespace L = agent_deck::layout;

namespace agent_deck {
void Install();
}

// --- What the screens take from lvgl_display and devices, which are not linked -----------------------------------------------------

namespace {
lv_group_t *s_group = nullptr;
lv_indev_t *s_keys = nullptr;
int s_full_requests = 0;
int s_cues = 0;
int s_at_ends = 0;  // the at-end cues alone
int s_errors = 0;   // the error cues alone
int s_moments = 0;
int s_music_starts = 0;
int s_music_stops = 0;
std::vector<devices::sound::Word> s_words;  // the words devices::Play was asked, in order
void (*s_back)() = nullptr;
}  // namespace

namespace lvgl_display {
lv_group_t *Group() { return s_group; }
void ClearGroup()
{
    lv_group_set_editing(s_group, false);
    lv_group_remove_all_objs(s_group);
}
void SetGroup(lv_group_t *group)
{
    if (group == nullptr) {
        return;
    }
    s_group = group;
    lv_indev_set_group(s_keys, group);
    lv_group_set_default(group);
}
void SetBackHandler(void (*back)()) { s_back = back; }
void SetKeyReadHook(void (*)(lv_indev_t *, lv_indev_data_t *)) {}
void SetTouchReadHook(void (*)(lv_indev_t *, lv_indev_data_t *)) {}
void SetPartialCounting(PartialCounting) {}
void NoteTyping() {}
void RequestFull() { ++s_full_requests; }
void PlayCue(Cue cue)
{
    ++s_cues;
    s_at_ends += cue == Cue::kAtEnd;
    s_errors += cue == Cue::kError;
}
}  // namespace lvgl_display

// power_leave's sequence is the board's and runs on the board: here it is the call the Power button makes, kept for the test to read.
namespace power_leave {
int s_asks = 0;
How s_how = How::kSleep;
Frame s_frame = nullptr;
void Leave(How, Frame) {}
void LeaveSoon(How how, Frame frame)
{
    ++s_asks;
    s_how = how;
    s_frame = frame;
}
}  // namespace power_leave

namespace devices {
esp_err_t ReadClock(std::tm *out)
{
    *out = {};
    out->tm_year = 2025 - 1900;
    out->tm_mon = 10;
    out->tm_mday = 7;
    out->tm_wday = 5;
    out->tm_hour = 14;
    out->tm_min = 32;
    return ESP_OK;
}
void QueueMoment(sound::Moment) { ++s_moments; }
void Play(sound::Word word) { s_words.push_back(word); }
void PlayMusic(sound::Music, int64_t) { ++s_music_starts; }
void StopMusic() { ++s_music_stops; }
void DropMoments() {}
bool s_muted = false;
void SetMuted(bool muted) { s_muted = muted; }
bool Muted() { return s_muted; }
}  // namespace devices

// Every helper of this test stays in this anonymous namespace: the firmware's own sources are linked into the test, and a
// helper with outside linkage that shares a name with one of theirs is one function to the linker — seen as the startup
// screen never drawing, then a segfault (STICKY_AGENT_DECK §8).
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

// --- LVGL, driven by hand ------------------------------------------------------------------------------------------------------

constexpr int kW = 480, kH = 800;
uint8_t s_l8[kW * kH];
uint32_t s_ms = 100000;
uint32_t Ticks() { return s_ms; }
int s_x, s_y;
bool s_down;
uint32_t s_key = LV_KEY_ENTER;
bool s_key_down = false;

void Flush(lv_display_t *display, const lv_area_t *, uint8_t *)
{
    lv_display_flush_ready(display);
}

void ReadPointer(lv_indev_t *, lv_indev_data_t *data)
{
    data->point.x = s_x;
    data->point.y = s_y;
    data->state = s_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void ReadKeys(lv_indev_t *, lv_indev_data_t *data)
{
    data->key = s_key;
    data->state = s_key_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void Advance(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 20) {
        s_ms += 20;
        lv_tick_inc(0);
        lv_timer_handler();
    }
}

bool StartLvgl()
{
    lv_init();
    constexpr size_t kPool = 60 * 1024, kPools = 128;
    alignas(16) static uint8_t heap[kPool * kPools];
    for (size_t i = 0; i < kPools; ++i) {
        if (lv_mem_add_pool(heap + i * kPool, kPool) == nullptr) {
            return false;
        }
    }
    lv_tick_set_cb(Ticks);
    lv_display_t *display = lv_display_create(kW, kH);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_L8);
    lv_display_set_buffers(display, s_l8, nullptr, sizeof s_l8, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, Flush);
    lv_display_set_theme(display, nullptr);
    s_keys = lv_indev_create();
    lv_indev_set_type(s_keys, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(s_keys, ReadKeys);
    lv_indev_t *pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, ReadPointer);
    s_group = lv_group_create();
    lv_group_set_default(s_group);
    lv_indev_set_group(s_keys, s_group);
    return true;
}

void Press(uint32_t key)
{
    s_key = key;
    s_key_down = true;
    Advance(60);
    s_key_down = false;
    Advance(60);
}

// A swipe: the finger presses at (x0, y0), moves to (x1, y1) a few px a read, and lifts.
void Swipe(int x0, int y0, int x1, int y1)
{
    s_x = x0;
    s_y = y0;
    s_down = true;
    Advance(40);
    const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0)) / 6 + 1;
    for (int i = 1; i <= steps; ++i) {
        s_x = x0 + (x1 - x0) * i / steps;
        s_y = y0 + (y1 - y0) * i / steps;
        Advance(40);
    }
    s_down = false;
    Advance(200);
}

// The glass as the display reads it, one bit a pixel (white = true).
std::vector<uint8_t> Glass()
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(nullptr);
    std::vector<uint8_t> out(kW * kH);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            out[y * kW + x] = lvgl_display::OneBitWhite(s_l8[y * kW + x], kH - 1 - y, x) ? 255 : 0;
        }
    }
    return out;
}

// --- The spacing a frame holds, measured from the objects: margins (kind m) between neighbours, padding (kind p) inside a box ---------------------

// The ruler reads a box's padding alone: a value row (the Time zone field, the Not-joined row) reserves a 3 px edge it never draws and its
// padding gives that up (spacing.md − 3, spacing.sm − 3), so it prints 13 and 5 there, 16 and 8 to the edge; the screen audit counts it.
std::string TokenFor(int px)
{
    switch (px) {
    case 4: return " xs";
    case 8: return " sm";
    case 16: return " md";
    case 20: return " card-inset";
    case 24: return " lg";
    case 32: return " xl";
    case 48: return " 2xl";
    case 64: return " target";
    default: return " OFF-SCALE";
    }
}

struct Note {
    char kind;
    int x1, y1, x2, y2;
    std::string label;
};

void Measure(lv_obj_t *o, std::vector<Note> &notes)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const bool glass = lv_area_get_width(&a) >= kW && lv_area_get_height(&a) >= kH;
    // Padding: a box that draws something, inside its edge.
    const bool drawn = lv_obj_get_style_bg_opa(o, LV_PART_MAIN) > 0 || lv_obj_get_style_border_width(o, LV_PART_MAIN) > 0;
    if (drawn && !glass && !lv_obj_check_type(o, &lv_label_class) && lv_area_get_width(&a) < kW - 40) {
        const int l = lv_obj_get_style_pad_left(o, LV_PART_MAIN), r = lv_obj_get_style_pad_right(o, LV_PART_MAIN);
        const int t = lv_obj_get_style_pad_top(o, LV_PART_MAIN), b = lv_obj_get_style_pad_bottom(o, LV_PART_MAIN);
        if (l > 0) notes.push_back({'p', a.x1, a.y1 + t, a.x1 + l - 1, a.y2 - b, std::to_string(l) + TokenFor(l)});
        if (r > 0) notes.push_back({'p', a.x2 - r + 1, a.y1 + t, a.x2, a.y2 - b, std::to_string(r) + TokenFor(r)});
        if (t > 0) notes.push_back({'p', a.x1 + l, a.y1, a.x2 - r, a.y1 + t - 1, std::to_string(t) + TokenFor(t)});
        if (b > 0) notes.push_back({'p', a.x1 + l, a.y2 - b + 1, a.x2 - r, a.y2, std::to_string(b) + TokenFor(b)});
    }
    // Margins: the gap between stacked neighbours that share a column, and the glass's margin beside a column of the content's width.
    std::vector<lv_obj_t *> kids;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *c = lv_obj_get_child(o, static_cast<int32_t>(i));
        lv_area_t ca;
        lv_obj_get_coords(c, &ca);
        if (!lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN) && !lv_obj_has_flag(c, LV_OBJ_FLAG_FLOATING) && lv_area_get_width(&ca) > 4 &&
            lv_area_get_height(&ca) > 4 && lv_area_get_width(&ca) < kW) {
            kids.push_back(c);
        }
    }
    std::sort(kids.begin(), kids.end(), [](lv_obj_t *x, lv_obj_t *y) { return lv_obj_get_y(x) < lv_obj_get_y(y); });
    for (size_t i = 1; i < kids.size(); ++i) {
        lv_area_t p, q;
        lv_obj_get_coords(kids[i - 1], &p);
        lv_obj_get_coords(kids[i], &q);
        // Two words stacked are measured between their own line boxes, inside the padding that is their leading (DESIGN → Layout →
        // Spacing: `pair-sm` is between the boxes, the leading made up in the name's margin).
        if (lv_obj_check_type(kids[i - 1], &lv_label_class) && lv_obj_check_type(kids[i], &lv_label_class)) {
            p.y2 -= lv_obj_get_style_pad_bottom(kids[i - 1], LV_PART_MAIN);
            q.y1 += lv_obj_get_style_pad_top(kids[i], LV_PART_MAIN);
        }
        const int x1 = std::max(p.x1, q.x1), x2 = std::min(p.x2, q.x2);
        const int gap = q.y1 - p.y2 - 1;
        if (x2 - x1 > 12 && gap >= 2 && gap <= 64) {
            notes.push_back({'m', x1, p.y2 + 1, x2, q.y1 - 1, std::to_string(gap) + TokenFor(gap)});
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        Measure(lv_obj_get_child(o, static_cast<int32_t>(i)), notes);
    }
}

void Dump(const std::string &name)
{
    const char *dir = std::getenv("DECK_DUMP");
    if (dir == nullptr) {
        return;
    }
    const std::vector<uint8_t> glass = Glass();
    if (FILE *f = std::fopen((std::string(dir) + "/" + name + ".raw").c_str(), "wb")) {
        std::fwrite(glass.data(), 1, glass.size(), f);
        std::fclose(f);
    }
    std::vector<Note> notes;
    Measure(lv_screen_active(), notes);
    // The glass's margins: `spacing.edge` either side of the content's column, as wide as the objects that stand on it.
    if (FILE *f = std::fopen((std::string(dir) + "/" + name + ".notes").c_str(), "w")) {
        for (const Note &n : notes) {
            std::fprintf(f, "%c|%d|%d|%d|%d|%s\n", n.kind, n.x1, n.y1, n.x2, n.y2, n.label.c_str());
        }
        std::fclose(f);
    }
}

// --- Finding things ----------------------------------------------------------------------------------------------------------------

lv_obj_t *FindText(lv_obj_t *root, const std::string &text)
{
    if (lv_obj_check_type(root, &lv_label_class) && text == lv_label_get_text(root)) {
        return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (lv_obj_t *found = FindText(lv_obj_get_child(root, static_cast<int32_t>(i)), text)) {
            return found;
        }
    }
    return nullptr;
}

lv_obj_t *Text(const std::string &text)
{
    return FindText(lv_screen_active(), text);
}

lv_obj_t *Line(const std::string &words);
// Words on the glass whole, or a stepped line holding them: a name too long for its row at 26 px steps.
lv_obj_t *Shown(const std::string &words)
{
    lv_obj_t *whole = Text(words);
    return whole != nullptr ? whole : Line(words);
}

lv_area_t Area(lv_obj_t *o)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    return a;
}

int X(lv_obj_t *o) { return Area(o).x1; }
int Y(lv_obj_t *o) { return Area(o).y1; }

// The control a label stands in: the first parent in a focus group.
lv_obj_t *ControlOf(lv_obj_t *inner)
{
    for (lv_obj_t *o = inner; o != nullptr; o = lv_obj_get_parent(o)) {
        if (lv_obj_get_group(o) != nullptr) {
            return o;
        }
    }
    return nullptr;
}

bool Ringed(lv_obj_t *o) { return o != nullptr && lv_obj_has_state(o, LV_STATE_FOCUS_KEY); }
lv_obj_t *Focused() { return lv_group_get_focused(s_group); }
std::string Said(lv_obj_t *label) { return label != nullptr ? lv_label_get_text(label) : "(none)"; }

// --- The model's day, as a test sets it --------------------------------------------------------------------------------------------

model::Agent Agent(uint16_t id, uint16_t parent, const char *name, model::AgentState state, uint32_t since, const char *detail, int8_t ctx,
                   uint32_t tokens, const char *kind = "")
{
    model::Agent a{};
    a.id = id;
    a.parent = parent;
    std::snprintf(a.name, sizeof a.name, "%s", name);
    std::snprintf(a.kind, sizeof a.kind, "%s", kind);
    a.state = state;
    a.since_s = since;
    std::snprintf(a.detail, sizeof a.detail, "%s", detail);
    a.context_pct = ctx;
    a.tokens = tokens;
    return a;
}

void Ask(uint32_t id, uint16_t agent, const char *tool, const char *body)
{
    model::Request r{};
    r.id = id;
    r.agent = agent;
    std::snprintf(r.tool, sizeof r.tool, "%s", tool);
    r.body = body;
    model::OpenRequest(r);
}

constexpr const char *kDeploy = "cd web-shop && npm run build && npm run deploy -- --prod --region eu-west --confirm && curl -fsS https://shop.example.com/health";

struct Decided {
    int count = 0;
    uint32_t id = 0;
    bool allow = false;
} s_decided;
struct Answered {
    int count = 0;
    uint8_t chosen = 255;
} s_answered;

void OnDecision(uint32_t id, bool allow)
{
    ++s_decided.count;
    s_decided.id = id;
    s_decided.allow = allow;
}
void OnAnswer(uint32_t, uint8_t, uint8_t chosen)
{
    ++s_answered.count;
    s_answered.chosen = chosen;
}

// The deck at the approved frames' state: one session working, its sub-agent, and web-shop needing you with a request queued.
void DeckState()
{
    model::Reset();
    model::SetListeners(OnDecision, OnAnswer);
    model::SetLink(true);
    model::Usage u;
    u.five_hour_pct = 42;
    u.five_hour_resets_min = 72;
    u.seven_day_pct = 17;
    u.seven_day_resets_min = 5880;
    model::SetUsage(u);
    model::SetAgent(Agent(1, 0, "agent-deck-studio", model::AgentState::kWorking, 252, "Bash: npm test", 63, 126000));
    model::SetAgent(Agent(2, 1, "Explore", model::AgentState::kWorking, 31, "Grep: kSafeSquare", -1, 18400));
    model::SetAgent(Agent(3, 0, "web-shop", model::AgentState::kNeedsYou, 12, "Bash: npm run deploy", 33, 66000));
    Ask(1001, 3, "Bash", kDeploy);
}

void Settle() { Advance(120); }

// Back to home with nothing above it.
void Home()
{
    while (router::Depth() > 1) {
        router::Pop();
        Settle();
    }
}

// --- The startup screen --------------------------------------------------------------------------------------------------------------

void TestSplash()
{
    std::printf("the startup screen\n");
    model::Reset();
    const home::Probe p = home::Look();
    Check(!p.carousel && p.pages == 0, "home opens on the startup screen, not the carousel");
    lv_obj_t *by = Text("ALXV Labs");
    lv_obj_t *line = Text("Designed for Creators");
    Check(line != nullptr && by != nullptr, "the startup screen says its line and the maker");
    Check(lv_obj_get_style_text_font(by, LV_PART_MAIN) == ui::Font(ui::Type::kTitle), "the maker is in title");
    Check(lv_obj_get_style_text_font(line, LV_PART_MAIN) == ui::Font(ui::Type::kCaption), "the line is in caption");
    // Every part centred across the glass, in the order the logo, the line, the maker: 32 px between the line and the maker.
    const lv_area_t la = Area(line), ba = Area(by);
    Check(std::abs((la.x1 + la.x2) / 2 - kW / 2) <= 1 && std::abs((ba.x1 + ba.x2) / 2 - kW / 2) <= 1, "the line and the maker are centred on the glass");
    Check(ba.y1 - (la.y2 + 1) == dt::kSpacingXl, "32 px between the line and the maker, by their own boxes");
    lv_obj_t *logo = nullptr;
    for (lv_obj_t *c = lv_obj_get_parent(line); c != nullptr && logo == nullptr; c = nullptr) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(c); ++i) {
            if (lv_obj_check_type(lv_obj_get_child(c, static_cast<int32_t>(i)), &lv_image_class)) {
                logo = lv_obj_get_child(c, static_cast<int32_t>(i));
            }
        }
    }
    Check(logo != nullptr && lv_obj_get_width(logo) == 310 && lv_obj_get_height(logo) == 70, "the Agent Jam logo stands at twice its art's size: 310 x 70");
    if (logo != nullptr) {
        const lv_area_t a = Area(logo);
        Check(std::abs((a.x1 + a.x2) / 2 - kW / 2) <= 1, "the logo is centred");
        Check(la.y1 - (a.y2 + 1) == dt::kSpacingMd, "the logo stands a space over its line");
        Check(a.y1 == L::kMascotY + L::kMascotH + dt::kSpacingLg, "the logo stands under the mascot's place, a space clear of its feet");
    }
    DeckState();
    model::Reset();
    for (int i = 0; i < 6; ++i) {
        Advance(200);
        Dump("splash_" + std::to_string(i));
    }
    Dump("splash");
    Advance(600);
    Check(!home::Look().carousel, "the startup screen stays up for its 2 s, counted from its first frame on the glass, as the mascot says hello");
    Advance(300);
    Check(home::Look().carousel, "then, the hello done, hands over to the carousel");
    Check(s_full_requests >= 1, "and the hand-over is a full refresh, so the logo leaves no ghost");
    Check(Text("ALXV Labs") == nullptr, "the startup screen's words are gone");
}

// --- The overview and an agent's page -----------------------------------------------------------------------------------------------

home::Probe TestOverviewChip()
{
    std::printf("the overview\n");
    mock::Stop();
    DeckState();
    Advance(200);
    const home::Probe p = home::Look();
    Check(p.carousel && p.page == 0 && p.pages == 4, "the carousel is on the overview, with the overview and three agents");
    Check(std::string(lv_label_get_text(lv_obj_get_child(p.chip, 0))) == "1 needs you", "the count says 1 needs you");
    const lv_area_t chip = Area(p.chip);
    Check(chip.y1 == L::kChipTop && lv_area_get_height(&chip) == dt::kSpacingChipSmall, "the count's bottom stands spacing.xs over the jump's headroom, 36 high");
    Check(std::abs((chip.x1 + chip.x2) / 2 - kW / 2) <= 1, "and centred on the glass");
    Check(lv_obj_get_style_text_font(lv_obj_get_child(p.chip, 0), LV_PART_MAIN) == ui::Font(ui::Type::kTitle), "in title");
    // The mascot, centred on the glass and raised one target, in front of everything.
    const lv_area_t m = Area(p.mascot);
    Check(m.x1 == 120 && m.y1 == 240 && lv_area_get_width(&m) == 240 && lv_area_get_height(&m) == 192, "the mascot stands at 120, 240, 240 x 192");
    lv_obj_t *page = p.page_object;
    Check(lv_obj_get_child(page, static_cast<int32_t>(lv_obj_get_child_count(page)) - 1) == p.mascot,
          "the mascot is the last thing the page draws: in front of every element");
    // The facts column.
    lv_obj_t *usage = Text("Usage");
    lv_obj_t *share = Text("42%");
    Check(usage != nullptr && share != nullptr && X(usage) == 24 && Y(usage) == L::kFactsTop, "Usage and its share start at the margin, a space under the mascot");
    Check(lv_obj_get_style_text_font(usage, LV_PART_MAIN) == ui::Font(ui::Type::kTitle), "Usage in title, a size over Weekly");
    lv_obj_t *weekly = Text("Weekly");
    Check(weekly != nullptr && lv_obj_get_style_text_font(weekly, LV_PART_MAIN) == ui::Font(ui::Type::kCaption), "Weekly in caption");
    Check(X(share) - (Area(usage).x2 + 1) == dt::kSpacingSm, "the share stands 8 px after its word");
    lv_obj_t *reset = Text("Resets 1h 12m");
    lv_obj_t *reset2 = Text("Resets 4d 2h");
    Check(reset != nullptr && Area(reset).x2 + 1 == kW - dt::kSpacingEdge, "the first reset line ends on the right margin");
    Check(reset2 != nullptr && Area(reset2).x2 + 1 == kW - dt::kSpacingEdge, "so does the second");
    Check(lv_obj_get_style_text_color(reset, LV_PART_MAIN).blue == ui::Fill(dt::kColorTextQuiet).blue, "a reset line is quiet words");
    return p;
}

void TestOverviewFoot(const home::Probe &p)
{
    const lv_area_t chip = Area(p.chip);
    // The page dots over the foot, the foot's icons flush right and the link on the left.
    const lv_area_t dots = Area(p.dots);
    Check(dots.y1 == L::kDotsTop && std::abs((dots.x1 + dots.x2) / 2 - kW / 2) <= 1, "the page dots stand centred, a space over the foot");
    const lv_area_t wifi = Area(p.wifi), sound = Area(p.sound), settings = Area(p.settings), power = Area(p.power), link = Area(p.link);
    Check(wifi.y1 == L::kFootTop && lv_area_get_height(&wifi) == dt::kSpacingTarget && sound.y1 == wifi.y1 && settings.y1 == wifi.y1 && power.y1 == wifi.y1,
          "the icon buttons stand 16 px over the glass's edge, 64 high");
    Check(power.x2 + 1 == kW - dt::kSpacingEdge, "Power last, flush to the right margin");
    Check(lv_area_get_width(&power) == dt::kSpacingTarget && lv_area_get_height(&power) == dt::kSpacingTarget, "Power a square target, as the others");
    Check(sound.x1 - (wifi.x2 + 1) == dt::kSpacingSm && settings.x1 - (sound.x2 + 1) == dt::kSpacingSm && power.x1 - (settings.x2 + 1) == dt::kSpacingSm,
          "8 px apart");
    Check(wifi.x2 < sound.x1 && sound.x2 < settings.x1 && settings.x2 < power.x1, "Wi-Fi, sound, Settings, then Power");
    Check(link.x2 < wifi.x1, "the link indicator keeps the left edge, clear of the buttons");
    Check(link.x1 == dt::kSpacingEdge && std::abs((link.y1 + link.y2) / 2 - (wifi.y1 + wifi.y2) / 2) <= 1, "the link indicator on the left edge, centred on the buttons");
    Check(!lv_obj_has_flag(p.link, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_group(p.link) == nullptr, "an indicator: no hit area, out of the focus group");
    // The clock.
    lv_obj_t *clock = p.clock;
    Check(Said(clock) == "Fri 7 Nov  2:32 PM" && Area(clock).x2 + 1 == kW - dt::kSpacingEdge && Area(clock).y1 == L::kClockTop, "the clock, top-right");
    // The count is a control while a request waits, and its ring shows as it appears.
    Check(p.stop != nullptr && Area(p.stop).x1 == chip.x1 && Area(p.stop).y1 == chip.y1 && Area(p.stop).x2 == chip.x2 && Area(p.stop).y2 == chip.y2,
          "the page's one stop stands over the count");
    Check(Ringed(p.stop) && Focused() == p.stop, "ringed as it appears: one press of OK opens what waits");
    Check(lv_obj_get_group(p.chip) == nullptr, "the chip itself is out of the group, so the walk's order is the page's");
    Dump("home_needs_rest");
}

// Nothing needs you: the count is a reading, the page's stop goes round its facts.
void TestOverviewWorking()
{
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    model::SetAgent(Agent(3, 0, "web-shop", model::AgentState::kWorking, 0, "Bash: npm run deploy", 33, 66000));
    Settle();
    const home::Probe q = home::Look();
    Check(std::string(lv_label_get_text(lv_obj_get_child(q.chip, 0))) == "3 working", "three working: 3 working");
    const lv_area_t stop = Area(q.stop);
    Check(stop.x1 == dt::kSpacingEdge && lv_area_get_width(&stop) == dt::kSpacingContent && stop.y2 + 1 + dt::kSpacingMd == L::kDotsTop,
          "a page with no control has one stop round its facts, a space over the page dots");
    Check(Focused() == q.stop, "the focus is still on the page");
    Dump("home_working");
    lv_obj_add_state(q.stop, LV_STATE_FOCUS_KEY);  // a look at the page's own ring, which a press shows
    Dump("home_working_ringed");
    lv_obj_remove_state(q.stop, LV_STATE_FOCUS_KEY);
    DeckState();
    Settle();
}

void TestOverview()
{
    TestOverviewFoot(TestOverviewChip());
    TestOverviewWorking();
}

void TestAgentSession()
{
    std::printf("an agent's page\n");
    DeckState();
    home::Probe p = home::Look();
    // DOWN from the overview's stop turns to the first agent's page.
    lv_group_focus_next(s_group);
    Settle();
    p = home::Look();
    Check(p.page == 1, "DOWN turns to the first agent's page");
    lv_obj_t *name = Text("agent-deck-studio");
    Check(name != nullptr && lv_obj_get_style_text_font(name, LV_PART_MAIN) == ui::Font(ui::Type::kTitle), "the name in title");
    lv_obj_t *chip = p.chip;
    Check(std::string(lv_label_get_text(lv_obj_get_child(chip, 0))).rfind("Working for 4m", 0) == 0, "the state chip: Working for 4m");
    const lv_area_t n = Area(name), c = Area(chip);
    Check(n.y2 + 1 + dt::kSpacingSm == c.y1 && std::abs((n.x1 + n.x2) / 2 - kW / 2) <= 1, "the name stands 8 px over the chip, centred");
    Check(c.y1 == L::kChipTop, "the chip stands where the home view's does");
    Check(p.review == nullptr, "no Review on a page that is not Needs you");
    lv_obj_t *context = Text("Context");
    Check(context != nullptr && lv_obj_get_style_text_font(context, LV_PART_MAIN) == ui::Font(ui::Type::kTitle), "Context 63% in title, as Usage");
    Check(X(context) == dt::kSpacingEdge && Y(context) == L::kFactsTop, "Context stands where the overview's Usage does: no shift between pages");
    lv_obj_t *tokens = Text("126k tokens");
    Check(tokens != nullptr && Area(tokens).x2 + 1 == kW - dt::kSpacingEdge, "the tokens right-aligned under the bar");
    lv_obj_t *doing = Text("Bash: npm test");
    Check(doing != nullptr && X(doing) == dt::kSpacingEdge && Y(doing) == Area(tokens).y2 + 1 + dt::kSpacingSm,
          "the doing line after the tokens, 8 px on, where Weekly starts");
    Check(lv_obj_get_style_text_color(tokens, LV_PART_MAIN).blue == ui::Fill(dt::kColorTextQuiet).blue, "in quiet words");
    lv_obj_update_layout(lv_screen_active());
    {
        // The pill follows the page: the page's own dot is the wide one, every other 8 px.
        bool right = true;
        for (uint32_t i = 0; i < lv_obj_get_child_count(p.dots); ++i) {
            const int w = lv_obj_get_width(lv_obj_get_child(p.dots, static_cast<int32_t>(i)));
            right = right && w == (static_cast<int>(i) == p.page ? dt::kSpacingPagePill : dt::kSpacingPageDot);
        }
        Check(right, "the page dots: the current page's is the pill, the rest dots");
    }
    Dump("agent_session");
}

// The sub-agent: its session's line, and its type and tokens on one line, 8 px apart.
void TestAgentSubagent()
{
    lv_group_focus_next(s_group);
    Settle();
    const home::Probe p = home::Look();
    Check(p.page == 2 && Text("in agent-deck-studio") != nullptr, "the sub-agent follows its session and says whose it is");
    lv_obj_t *type = Text("Sub-agent");
    lv_obj_t *sub_tokens = Text("18.4k tokens");
    Check(type != nullptr && sub_tokens != nullptr && X(sub_tokens) - (Area(type).x2 + 1) == dt::kSpacingSm && Area(type).y1 == Area(sub_tokens).y1,
          "its type and its tokens on one line, 8 px apart");
    Check(Text("Context") == nullptr, "a sub-agent has no context bar");
    Dump("agent_subagent");
}

// A Needs you page: Review, ringed, always in the same place.
void TestAgentNeedsYou()
{
    lv_group_focus_next(s_group);
    Settle();
    const home::Probe p = home::Look();
    Check(p.page == 3 && p.review != nullptr, "web-shop's page is Needs you: Review");
    const lv_area_t r = Area(p.review);
    Check(r.x1 == (kW - dt::kSpacingButtonWidth) / 2 && r.y1 == L::kReviewTop && lv_area_get_width(&r) == dt::kSpacingButtonWidth,
          "Review is centred, its ring 16 px over the page dots");
    Check(r.y2 + 1 + dt::kSpacingMd + dt::kFocusRingReach == L::kDotsTop, "(16 px of white over the dots, past the ring)");
    Check(Area(p.stop).y1 == r.y1 && Area(p.stop).x1 == r.x1, "the page's stop stands over Review");
    Check(Ringed(p.stop) && Focused() == p.stop, "Review rings as it appears");
    Check(lv_obj_get_group(p.review) == nullptr, "Review is out of the group, so the walk is the page's");
    Dump("agent_needs_review");
    Check(Text("Needs you for 12s") != nullptr || Text("Needs you for 13s") != nullptr || Text("Needs you for 14s") != nullptr || Text("Needs you for 15s") != nullptr ||
              Text("Needs you for 16s") != nullptr || Text("Needs you for 17s") != nullptr || Text("Needs you for 18s") != nullptr,
          "the chip counts on from the record's 12 s");
}

void TestAgentPage()
{
    TestAgentSession();
    TestAgentSubagent();
    TestAgentNeedsYou();
}

// --- The walk, the swipe and OK -----------------------------------------------------------------------------------------------------------

void TestWalkDown()
{
    std::printf("the walk, the swipe\n");
    DeckState();
    // From the overview, DOWN through the three agents' pages, then the foot's four icons, then round.
    std::vector<int> pages;
    int empty_pages = 0, empty_unringed = 0, control_pages = 0, control_ringed = 0;
    for (int i = 0; i < 3; ++i) {
        lv_group_focus_next(s_group);
        Settle();
        pages.push_back(home::Look().page);
        Check(Focused() == home::Look().stop, "after each DOWN the focus is on the page's stop");
        lv_obj_t *stop = home::Look().stop;
        const bool had_key = lv_obj_has_state(stop, LV_STATE_FOCUS_KEY);
        lv_obj_add_state(stop, LV_STATE_FOCUS_KEY);
        const lv_opa_t ring = lv_obj_get_style_outline_opa(stop, LV_PART_MAIN);
        if (!had_key) {
            lv_obj_remove_state(stop, LV_STATE_FOCUS_KEY);
        }
        if (home::Look().review == nullptr) {
            ++empty_pages;
            empty_unringed += ring == LV_OPA_TRANSP ? 1 : 0;
        } else {
            ++control_pages;
            control_ringed += ring == LV_OPA_COVER ? 1 : 0;
        }
    }
    Check(pages == std::vector<int>({1, 2, 3}), "DOWN turns one page a press");
    {
        lv_obj_t *swipe = lv_obj_get_parent(home::Look().stop);
        Check(lv_obj_get_scroll_x(swipe) == 0 && lv_obj_get_scroll_y(swipe) == 0, "the swipe page never scrolls under UP and DOWN: a page turns, nothing slides");
    }
    Check(empty_pages > 0 && empty_unringed == empty_pages && control_pages > 0 && control_ringed == control_pages,
          "a page with nothing to press draws no ring (UP and DOWN turn it, the dots say where the person is); a page's control keeps its ring");
}

void TestWalkFoot()
{
    lv_group_focus_next(s_group);
    Settle();
    home::Probe p = home::Look();
    Check(Focused() == p.wifi && p.page == 3, "DOWN on the last page reaches the foot: Wi-Fi first");
    lv_group_focus_next(s_group);
    Check(Focused() == home::Look().sound, "then the sound toggle");
    lv_group_focus_next(s_group);
    Check(Focused() == home::Look().settings, "then Settings");
    lv_group_focus_next(s_group);
    Check(Focused() == home::Look().power, "then Power, last");
    lv_group_focus_next(s_group);
    Settle();
    p = home::Look();
    Check(Focused() == p.stop && p.page == 0, "DOWN from Power wraps to the overview");
    lv_group_focus_prev(s_group);
    Settle();
    Check(Focused() == home::Look().power, "UP from the overview reaches Power");
    lv_group_focus_prev(s_group);
    Check(Focused() == home::Look().settings, "UP from Power is Settings");
    lv_group_focus_prev(s_group);
    lv_group_focus_prev(s_group);
    lv_group_focus_prev(s_group);
    Settle();
    p = home::Look();
    Check(Focused() == p.stop && p.page == 3, "UP from Wi-Fi comes back to the last page");
    lv_group_focus_prev(s_group);
    Settle();
    Check(home::Look().page == 2 && Focused() == home::Look().stop, "and UP turns one page back");
}

// A swipe left turns forward, right back; at an end nothing wraps and the at-end cue says so.
void TestWalkSwipe()
{
    while (home::Look().page > 0) {
        lv_group_focus_prev(s_group);
        Settle();
    }
    s_cues = 0;
    Swipe(400, 300, 80, 300);
    Check(home::Look().page == 1, "a swipe left turns to the next page");
    Swipe(80, 300, 400, 300);
    Check(home::Look().page == 0, "a swipe right turns back");
    Swipe(80, 300, 400, 300);
    Check(home::Look().page == 0 && s_cues >= 1, "past the first page nothing wraps, and the at-end cue sounds");
    Swipe(300, 600, 300, 300);
    Check(home::Look().page == 0, "a vertical drag turns nothing");
    {
        // A finger turning onto a page with a control shows no ring: the ring is the buttons'.
        bool found = false, ringed = false;
        for (int i = 0; i < 4 && !found; ++i) {
            Swipe(400, 300, 80, 300);
            if (home::Look().review != nullptr) {
                found = true;
                ringed = Ringed(home::Look().stop);
            }
        }
        Check(found && !ringed, "a swipe onto a Needs you page focuses Review without a ring");
        while (home::Look().page > 0) {
            Swipe(80, 300, 400, 300);
        }
    }
}

// The foot's icons open their pages (TestSettings and TestWifi walk them).
void TestWalkIcons()
{
    Check(router::Depth() == 1, "home is the stack's one screen");
    lv_obj_send_event(home::Look().settings, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 2 && Text("Settings") != nullptr, "the Settings icon opens the settings");
    Home();
    Settle();
    Dump("home_after_walk");
}

void TestWalk()
{
    TestWalkDown();
    TestWalkFoot();
    TestWalkSwipe();
    TestWalkIcons();
}

void TestSound()
{
    std::printf("the sound toggle\n");
    DeckState();
    home::Probe p = home::Look();
    Check(lv_obj_has_state(p.sound, LV_STATE_CHECKED) && model::SoundOn(), "sound is on at the start, the toggle on");
    const lv_color_t on_fill = lv_obj_get_style_bg_color(p.sound, LV_PART_MAIN);
    lv_obj_remove_state(p.sound, LV_STATE_CHECKED);
    const lv_color_t off_fill = lv_obj_get_style_bg_color(p.sound, LV_PART_MAIN);
    lv_obj_add_state(p.sound, LV_STATE_CHECKED);
    Check(lv_color_eq(on_fill, off_fill) && lv_color_eq(on_fill, lv_obj_get_style_bg_color(p.settings, LV_PART_MAIN)),
          "its tile is the foot's tonal fill on and off: the glyph alone says the state (the person, 2026-11-26)");
    lv_obj_send_event(p.sound, LV_EVENT_VALUE_CHANGED, nullptr);  // the toggle flips on a click; here the state is set first
    lv_obj_remove_state(p.sound, LV_STATE_CHECKED);
    lv_obj_send_event(p.sound, LV_EVENT_VALUE_CHANGED, nullptr);
    Check(!model::SoundOn(), "the toggle turns the sound off");
    model::SetSoundOn(true);
    Settle();
    Check(lv_obj_has_state(home::Look().sound, LV_STATE_CHECKED), "and the setting turns the toggle on again");
}

// --- OK on Review, the list, the request -------------------------------------------------------------------------------------------------------

// A line tight under its heading: spacing.xs between the two words' own boxes (layout::TuckUnder).
void CheckTucked(lv_obj_t *heading, lv_obj_t *line, const std::string &what)
{
    Check(heading != nullptr && line != nullptr &&
              (Y(line) + lv_obj_get_style_pad_top(line, LV_PART_MAIN)) -
                      (Area(heading).y2 + 1 - lv_obj_get_style_pad_bottom(heading, LV_PART_MAIN)) ==
                  dt::kSpacingXs,
          what + " tight under its heading: spacing.xs between their own boxes");
}

void TestRequestOpen()
{
    std::printf("a request\n");
    DeckState();
    // The overview's count is a control: OK opens the one waiting agent's oldest request.
    home::Probe p = home::Look();
    Check(Focused() == p.stop, "the walk starts on the count's stop");
    Press(LV_KEY_ENTER);
    Check(router::Depth() == 2, "OK on the count opens the waiting request");
    Check(Text("Bash") != nullptr && Text("web-shop") != nullptr, "the tool and the agent");
    lv_obj_t *tool = Text("Bash");
    Check(lv_obj_get_style_text_font(tool, LV_PART_MAIN) == ui::Font(ui::Type::kTitle) && X(tool) == dt::kSpacingEdge && Y(tool) == L::kHeadingTop,
          "the tool in title at the margin, a target down");
    lv_obj_t *agent = Text("web-shop");
    Check(lv_obj_get_style_text_color(agent, LV_PART_MAIN).blue == ui::Fill(dt::kColorTextQuiet).blue, "the agent in quiet words");
    CheckTucked(tool, agent, "the agent");
    Check(Text("1 of 1") == nullptr, "a lone request says no count");
    lv_obj_t *deny = nullptr, *allow = nullptr;
    // Deny and Allow, two halves 16 px apart at 704.
    lv_obj_t *denyl = Text("Deny"), *allowl = Text("Allow");
    Check(denyl != nullptr && allowl != nullptr, "Deny and Allow");
    deny = lv_obj_get_parent(denyl);
    allow = lv_obj_get_parent(allowl);
    const lv_area_t d = Area(deny), a = Area(allow);
    Check(d.y1 == L::kAnswersTop && a.y1 == L::kAnswersTop && lv_area_get_width(&d) == L::kHalfW && lv_area_get_width(&a) == L::kHalfW,
          "two halves at y 704");
    Check(a.x1 - (d.x2 + 1) == dt::kSpacingMd && d.x1 == dt::kSpacingEdge && a.x2 + 1 == kW - dt::kSpacingEdge, "16 px apart, across the content");
    Check(Text("Dismiss") == nullptr, "no Dismiss button: holding OK is Dismiss");
    Check(Focused() == deny && !Ringed(deny), "it opens with no ring, on Deny: the first press rings it");
    Check(Text("Review") == nullptr, "nothing else on the page");
    Dump("request_opened");
    // Allow: the decision goes to the link, and with no other request the carousel comes back on the agent's page.
    s_decided = {};
    lv_obj_send_event(allow, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(s_decided.count == 1 && s_decided.id == 1001 && s_decided.allow, "Allow sends the decision");
    Check(router::Depth() == 1, "with no other request waiting home comes back");
    p = home::Look();
    Check(p.page == 3, "on the asking agent's page");
    Check(model::QueueCount() == 0, "and the request is gone from the queue");
}

// Deny from a request among several: the list opens in its place.
void TestRequestDeny()
{
    DeckState();
    Ask(1002, 1, "Edit", "a\nb\nc");
    model::SetAgent(Agent(1, 0, "agent-deck-studio", model::AgentState::kNeedsYou, 5, "Edit: a.cpp", 63, 126000));
    Settle();
    Check(router::PushRoute(flow::kRouteRequest, "1001") == ESP_OK, "a request opens by its route");
    Settle();
    Check(Text("1 of 2") != nullptr, "its place in the queue: 1 of 2");
    lv_obj_t *counted = Text("1 of 2");
    lv_obj_t *who = Text("web-shop");
    Check(X(counted) - (Area(who).x2 + 1) == dt::kSpacingMd, "16 px after the agent's name, in black");
    Check(lv_obj_get_style_text_color(counted, LV_PART_MAIN).blue == ui::Level(dt::kColorTextPrimary).blue, "the count is solid black");
    lv_obj_send_event(lv_obj_get_parent(Text("Deny")), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 2 && Text("Needs you") != nullptr, "denied with another waiting: the needs-you list opens over home");
    Check(model::QueueCount() == 1 && s_decided.id == 1001 && !s_decided.allow, "the denial reached the link");
    Home();
}

void TestRequestDismissAndLong()
{
    // Holding OK is Dismiss: the request stays queued, and it leaves the same way.
    DeckState();
    router::PushRoute(flow::kRouteRequest, "1001");
    Settle();
    router::Escape();
    Settle();
    Check(router::Depth() == 1 && model::QueueCount() == 1 && model::FindItem(1001) != nullptr, "holding OK leaves the request waiting and goes back");
    Check(home::Look().page == 3, "to the asking agent's page");
    // A param that names nothing opens nothing.
    Check(router::PushRoute(flow::kRouteRequest, "777") != ESP_OK && router::PushRoute(flow::kRouteRequest, "x") != ESP_OK, "a request that is not waiting opens nothing");

    // A long body: the text view pages it, its scroll indicator in the right margin.
    DeckState();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    std::string long_body;
    for (int i = 0; i < 30; ++i) {
        long_body += "components/app_ui/deck_home.cpp line " + std::to_string(i) + "\n";
    }
    Ask(1003, 3, "Edit", long_body.c_str());
    router::PushRoute(flow::kRouteRequest, "1003");
    Settle();
    Dump("request_long");
    Check(Text("Bash") == nullptr && Text("Edit") != nullptr, "the long request's tool");
    // A swipe up begun on Allow turns the text a page and answers nothing; one down from the heading turns it back.
    {
        lv_obj_t *first = nullptr;
        const std::string opening = long_body.substr(0, long_body.find('\n'));  // the body's first line
        std::function<void(lv_obj_t *)> find = [&](lv_obj_t *o) {
            if (first == nullptr && lv_obj_check_type(o, &lv_label_class) && std::string(lv_label_get_text(o)).rfind(opening, 0) == 0) {
                first = o;
            }
            for (uint32_t i = 0; i < lv_obj_get_child_count(o) && first == nullptr; ++i) {
                find(lv_obj_get_child(o, static_cast<int32_t>(i)));
            }
        };
        find(lv_screen_active());
        const int words_y = first != nullptr ? Y(first) : 0;
        lv_obj_t *allow = lv_obj_get_parent(Text("Allow"));
        const int at = Y(allow) + dt::kSpacingTarget / 2;
        s_decided = {};
        Swipe(300, at, 300, at - 2 * dt::kTouchSwipeDistance);
        Check(first != nullptr && Y(first) < words_y && router::Depth() == 2 && s_decided.id == 0,
              "request: a swipe up begun on Allow turns the text a page and answers nothing");
        const int heading = L::kHeadingTop + dt::kSpacingMd;
        Swipe(240, heading, 240, heading + 2 * dt::kTouchSwipeDistance);
        Check(Y(first) == words_y, "a swipe down begun on the heading turns it back");
        Check(lv_obj_get_scroll_y(lv_obj_get_child(lv_screen_active(), 0)) == 0, "and the page under the swipes never moved");
    }
    Home();
}

void TestRequest()
{
    TestRequestOpen();
    TestRequestDeny();
    TestRequestDismissAndLong();
}

void TestWaitingList()
{
    std::printf("the needs-you list\n");
    DeckState();
    model::SetAgent(Agent(4, 3, "Plan", model::AgentState::kNeedsYou, 2, "Edit: a", -1, 5000, "Plan"));
    model::SetAgent(Agent(5, 0, "docs-site", model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    model::SetAgent(Agent(6, 5, "Explore", model::AgentState::kNeedsYou, 2, "Bash", -1, 3000));
    Ask(1002, 5, "WebFetch", "https://docs.example.com/api");
    Ask(1003, 4, "Edit", "x");
    Ask(1004, 6, "Bash", "ls");
    Settle();
    const home::Probe p = home::Look();
    Check(std::string(lv_label_get_text(lv_obj_get_child(p.chip, 0))) == "4 need you", "four need you");
    Press(LV_KEY_ENTER);
    Check(router::Depth() == 2 && Text("Needs you") != nullptr, "several waiting: the count opens the list");
    lv_obj_t *title = Text("Needs you");
    Check(lv_obj_get_style_text_font(title, LV_PART_MAIN) == ui::Font(ui::Type::kTitle) && X(title) == dt::kSpacingEdge && Y(title) == L::kHeadingTop,
          "Needs you in title");
    lv_obj_t *count = Text("4 sessions");
    Check(count != nullptr, "four sessions (web-shop, docs-site, Plan, Explore), oldest first");
    CheckTucked(title, count, "the count");
    Check(lv_obj_get_style_text_color(count, LV_PART_MAIN).blue == ui::Fill(dt::kColorTextQuiet).blue, "in quiet words");
    Check(lv_obj_get_style_text_font(count, LV_PART_MAIN) == ui::Font(ui::Type::kCaptionRegular) && lv_obj_get_style_text_font(count, LV_PART_MAIN) == ui::Bitcount(26, 400),
          "the count line in caption-regular, 26 px regular, keeping its quiet colour");
    Check(lv_obj_get_style_text_font(title, LV_PART_MAIN) == ui::Bitcount(30, 600) && lv_obj_get_style_text_font(count, LV_PART_MAIN) != ui::Bitcount(26, 600),
          "so the heading over it differs in size and weight: 30 px semibold over 26 px regular, never 26 px semibold");
    Check(lv_obj_get_height(count) == ui::LineBox(ui::Type::kCaptionRegular) && ui::LineBox(ui::Type::kCaptionRegular) == ui::LineBox(ui::Type::kCaption),
          "on the same 32 px line the caption stood on");
    lv_obj_t *first = Text("web-shop");
    lv_obj_t *second = Text("docs-site");
    Check(first != nullptr && second != nullptr && Y(first) < Y(second), "the agent with the oldest request first");
    Check(lv_obj_get_style_text_font(first, LV_PART_MAIN) == ui::Font(ui::Type::kBody), "the names in body, regular, 30 px");
    lv_obj_t *facts = nullptr;
    for (uint32_t s_ = 0; s_ < 30 && facts == nullptr; ++s_) {
        facts = Text("Bash \xE2\x80\xA2 " + std::to_string(s_) + "s");
    }
    Check(facts != nullptr, "each row says what its agent asks and how long it has waited");
    Check(facts != nullptr && lv_obj_get_style_text_font(facts, LV_PART_MAIN) == ui::Font(ui::Type::kCaptionRegular) &&
              lv_obj_get_style_text_font(first, LV_PART_MAIN) == ui::Font(ui::Type::kBody),
          "the facts line in caption-regular, 26 px regular, under the names in body");
    lv_obj_t *dismiss = lv_obj_get_parent(Text("Dismiss"));
    const lv_area_t d = Area(dismiss);
    Check(d.y1 == L::kAnswersTop && std::abs((d.x1 + d.x2) / 2 - kW / 2) <= 1 && lv_area_get_width(&d) == dt::kSpacingButtonWidth, "Dismiss centred at y 704");
    Check(!Ringed(Focused()), "the list opens with no ring");
    Dump("waiting_opened");
    // A row opens its agent's oldest request over the list; answered with others waiting, back to the list.
    lv_obj_send_event(ControlOf(first), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 3 && Text("Deny") != nullptr, "a row opens its agent's request over the list");
    Check(Text("1 of 4") != nullptr || Text("1 of 3") != nullptr || Text("1 of 5") != nullptr, "at its place in the queue");
    lv_obj_send_event(lv_obj_get_parent(Text("Deny")), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 2 && Text("Needs you") != nullptr, "answered, back to the list");
    Check(Text("3 sessions") != nullptr, "now three sessions");
    // Dismiss leaves for the overview.
    lv_obj_send_event(lv_obj_get_parent(Text("Dismiss")), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 1 && home::Look().page == 0, "Dismiss goes to the overview");
}

// The list follows the queue: with none left it goes on.
void TestWaiting()
{
    TestWaitingList();
    DeckState();
    model::SetAgent(Agent(5, 0, "docs-site", model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    Ask(1002, 5, "WebFetch", "u");
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    model::CloseRequest(1002, model::CloseReason::kTimedOut);
    Settle();
    Check(router::Depth() == 1, "a list with nothing left goes on to home");
}

// --- A question ----------------------------------------------------------------------------------------------------------------------------

// A question card's name over its description: bold over regular at one size, pair-sm between their own boxes.
void CheckCardPair(lv_obj_t *name, lv_obj_t *detail)
{
    Check(detail != nullptr && lv_obj_get_style_text_font(detail, LV_PART_MAIN) == ui::Font(ui::Type::kCaptionRegular),
          "its description in caption-regular: bold over regular at one size");
    Check(detail != nullptr && (Y(detail) + lv_obj_get_style_pad_top(detail, LV_PART_MAIN)) -
                                       (Area(name).y2 + 1 - lv_obj_get_style_pad_bottom(name, LV_PART_MAIN)) ==
                                   dt::kSpacingPairSm,
          "pair-sm between the name's and the description's own boxes");
}

void TestQuestionOne(model::Question &q)
{
    std::printf("a question\n");
    DeckState();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    q.id = 2001;
    q.agent = 1;
    std::snprintf(q.tool, sizeof q.tool, "Branch");
    q.question = "Which base branch?";
    q.options[0] = "main: the release branch";
    q.options[1] = "develop: where the work lands";
    q.option_count = 2;
    model::OpenQuestion(q);
    Check(router::PushRoute(flow::kRouteQuestion, "2001") == ESP_OK, "a question opens by its route");
    Settle();
    lv_obj_t *title = Text("Branch");
    Check(title != nullptr && Y(title) == L::kHeadingTop, "the header in title");
    lv_obj_t *ask = Text("Which base branch?");
    Check(ask != nullptr && lv_obj_get_style_text_font(ask, LV_PART_MAIN) == ui::Font(ui::Type::kCaption), "the question in caption");
    Check(Y(ask) - (Area(title).y2 + 1) >= dt::kSpacingMd, "the question stands a group under the heading");
    lv_obj_t *name = Text("main");
    Check(name != nullptr && lv_obj_get_style_text_font(name, LV_PART_MAIN) == ui::Font(ui::Type::kCaptionStrong), "a card's name in caption-strong, one size down");
    lv_obj_t *card = nullptr;
    for (lv_obj_t *o = name; o != nullptr; o = lv_obj_get_parent(o)) {
        if (lv_obj_get_group(o) != nullptr) {
            card = o;
            break;
        }
    }
    Check(card != nullptr && Text("the release branch") != nullptr, "its description under it");
    CheckCardPair(name, Text("the release branch"));
    Check(Y(card) - (Area(ask).y2 + 1) == dt::kSpacingLg, "the question stands spacing.lg over its first card");
    lv_obj_t *dismiss = lv_obj_get_parent(Text("Dismiss"));
    lv_obj_t *send = lv_obj_get_parent(Text("Send"));
    Check(Y(dismiss) == L::kAnswersTop && Y(send) == L::kAnswersTop && X(send) - (Area(dismiss).x2 + 1) == dt::kSpacingMd, "Dismiss and Send, two halves 16 px apart");
    Check(!lv_obj_has_state(send, LV_STATE_DISABLED), "Send is never disabled");
    Dump("question_open");
    Check(!Ringed(Focused()) && Focused() == card, "it opens on the first card with no ring");
    s_answered = {};
    s_errors = 0;
    lv_group_focus_obj(send);
    lv_obj_send_event(send, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(s_answered.count == 0 && router::Depth() == 2 && s_errors == 1, "Send with no pick sends nothing and sounds sound.error");
    Check(Focused() == card && Ringed(card), "and rings the first answer");
    lv_obj_send_event(card, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(!lv_obj_has_state(send, LV_STATE_DISABLED) && widgets::ChoiceChosen(card), "a pick is not the answer: Send is on offer");
    Check(router::Depth() == 2, "the pick leaves the page where it is");
    Dump("question_picked");
    s_answered = {};
    lv_obj_send_event(send, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(s_answered.count == 1 && s_answered.chosen == 1 && router::Depth() == 1, "Send sends bit 0 for the first option and goes on");
}

// Several answers: a bit each.
void TestQuestionSeveral(model::Question &q)
{
    DeckState();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    q.id = 2002;
    std::snprintf(q.tool, sizeof q.tool, "Plan 2/2");
    q.question = "Which should it do?";
    q.options[0] = "Write the tests";
    q.options[1] = "Update the docs: README and the API page";
    q.options[2] = "Open a pull request";
    q.option_count = 3;
    q.multi = true;
    model::OpenQuestion(q);
    router::PushRoute(flow::kRouteQuestion, "2002");
    Settle();
    std::vector<lv_obj_t *> cards;
    for (const char *t : {"Write the tests", "Update the docs", "Open a pull request"}) {
        lv_obj_t *l = Text(t);
        for (lv_obj_t *o = l; o != nullptr; o = lv_obj_get_parent(o)) {
            if (lv_obj_get_group(o) != nullptr) {
                cards.push_back(o);
                break;
            }
        }
    }
    Check(cards.size() == 3, "three cards");
    lv_obj_send_event(cards[0], LV_EVENT_CLICKED, nullptr);
    lv_obj_send_event(cards[2], LV_EVENT_CLICKED, nullptr);
    lv_obj_t *send = lv_obj_get_parent(Text("Send"));
    lv_obj_send_event(send, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(s_answered.chosen == 5, "a bit for each option chosen: the first and the third");
}

// Dismiss, and holding OK: the question is left to the terminal.
void TestQuestionDismiss(model::Question &q)
{
    DeckState();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    q.id = 2003;
    q.multi = false;
    q.option_count = 2;
    model::OpenQuestion(q);
    router::PushRoute(flow::kRouteQuestion, "2003");
    Settle();
    s_answered = {};
    router::Escape();
    Settle();
    Check(s_answered.count == 1 && s_answered.chosen == 0 && router::Depth() == 1 && model::FindItem(2003) == nullptr,
          "holding OK does what Dismiss does: left to the terminal, 0 chosen");
    Check(router::PushRoute(flow::kRouteRequest, "2003") != ESP_OK, "and a question is not opened as a request");
}

// The scroll indicator's lane stands in the glass's right margin, where the other paged views put it.
void CheckScrollLane()
{
    bool lane = false;
    std::function<void(lv_obj_t *)> find = [&](lv_obj_t *o) {
        const lv_area_t at = Area(o);
        if (!lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) && lv_area_get_width(&at) == dt::kSpacingScrollThumb && at.x1 == dt::kSpacingScrollX) {
            lane = true;
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
            find(lv_obj_get_child(o, static_cast<int32_t>(i)));
        }
    };
    find(lv_screen_active());
    Check(lane, "the scroll indicator stands in the right margin while the question runs to a second page");
}

// The first card focused, its page swiped away: Send with no pick turns it back into view and rings it.
void CheckSendBringsFirstBack(lv_obj_t *first, int32_t y_first, int at)
{
    lv_group_focus_obj(first);
    Settle();
    Swipe(240, at + 2 * dt::kTouchSwipeDistance, 240, at);
    Check(Area(first).y1 != y_first && Focused() == first, "a swipe up turns off the first card's page, the focus left on it");
    s_answered = {};
    lv_obj_send_event(lv_obj_get_parent(Text("Send")), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(Area(first).y1 == y_first && Ringed(first) && s_answered.count == 0,
          "Send with no pick turns the first answer's page back into view and rings it");
}

// A long question: the question and its cards turn by page together, whole items a page, with the scroll indicator.
void TestQuestionLong(model::Question &q)
{
    DeckState();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    q.id = 2004;
    std::snprintf(q.tool, sizeof q.tool, "Deploy");
    q.question = "The deploy failed on the health check after the build passed, and production still serves the last release. How should it go on from here?";
    q.options[0] = "Roll back: Restore the last good release on shop.example.com and stop, so you can read the logs first";
    q.options[1] = "Retry the deploy: Run it again with the same build: the health check may have timed out on a cold start";
    q.options[2] = "Deploy to staging: Send this build to staging.shop.example.com and run the smoke tests there instead";
    q.options[3] = "Stop here: Leave production as it is and write up what failed in the pull request";
    q.option_count = 4;
    model::OpenQuestion(q);
    router::PushRoute(flow::kRouteQuestion, "2004");
    Settle();
    Dump("question_long_p1");
    lv_obj_t *last = nullptr;
    for (lv_obj_t *o = Text("Stop here"); o != nullptr; o = lv_obj_get_parent(o)) {
        if (lv_obj_get_group(o) != nullptr) {
            last = o;
            break;
        }
    }
    lv_obj_t *roll = nullptr;
    for (lv_obj_t *o = Text("Roll back"); o != nullptr; o = lv_obj_get_parent(o)) {
        if (lv_obj_get_group(o) != nullptr) {
            roll = o;
            break;
        }
    }
    Check(roll != nullptr && last != nullptr, "the first and the last card");
    // Walk DOWN from the first card to the last: the page turns as the focus leaves it.
    lv_group_focus_obj(roll);
    const int32_t y_first = Area(roll).y1;
    int steps = 0;
    while (Focused() != last && steps < 8) {
        lv_group_focus_next(s_group);
        Settle();
        ++steps;
    }
    Check(Focused() == last, "the walk reaches the last card");
    const lv_area_t la = Area(last);
    Check(la.y1 >= L::kHeadingTop && la.y2 + dt::kFocusRingReach < L::kAnswersTop, "and it stands on the glass above the answers with its ring clear");
    Check(Area(roll).y1 != y_first, "the band turned a page: the first card is no longer where it was");
    CheckScrollLane();
    Dump("question_long_p2");
    {
        const int at = L::kHeadingTop + dt::kSpacingMd;  // on the question's heading, not its band
        // A swipe down begun on the heading turns back a page; as many as the walk turned bring the first page back (26 px words page
        // more: one card a page here).
        for (int i = 0; i < steps && Area(roll).y1 != y_first; ++i) {
            Swipe(240, at, 240, at + 2 * dt::kTouchSwipeDistance);
        }
        Check(Area(roll).y1 == y_first, "swipes down begun on the heading turn the cards back to the first page");
        CheckSendBringsFirstBack(roll, y_first, at);
    }
    router::Escape();
    Settle();
    Home();
}

void TestQuestion()
{
    model::Question q{};
    TestQuestionOne(q);
    TestQuestionSeveral(q);
    TestQuestionDismiss(q);
    TestQuestionLong(q);
}

// --- A request closed elsewhere ----------------------------------------------------------------------------------------------------------

void TestClosedTimedOut()
{
    std::printf("a request closed elsewhere\n");
    DeckState();
    router::PushRoute(flow::kRouteRequest, "1001");
    Settle();
    const uint32_t before = static_cast<uint32_t>(s_ms);
    (void)before;
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    Settle();
    Check(Text("Timed out") != nullptr && Text("before an answer came") != nullptr, "timed out: the status block says so");
    Check(Text("Deny") == nullptr && lv_group_get_obj_count(s_group) == 0, "no control is left on the page: a press does nothing");
    lv_obj_t *heading = Text("Timed out");
    Check(std::abs((Area(heading).x1 + Area(heading).x2) / 2 - kW / 2) <= 1, "the block is centred across the glass");
    // The mascot in the home view's place, under it the block.
    lv_obj_t *mascot = nullptr;
    for (uint32_t i = 0; i < lv_obj_get_child_count(lv_screen_active()) && mascot == nullptr; ++i) {
        lv_obj_t *root = lv_obj_get_child(lv_screen_active(), static_cast<int32_t>(i));
        for (uint32_t j = 0; j < lv_obj_get_child_count(root); ++j) {
            lv_obj_t *c = lv_obj_get_child(root, static_cast<int32_t>(j));
            if (lv_obj_get_width(c) == L::kMascotW && lv_obj_get_height(c) == L::kMascotH) {
                mascot = c;
            }
        }
    }
    Check(mascot != nullptr && Area(mascot).x1 == L::kMascotX && Area(mascot).y1 == L::kMascotY, "the mascot stands in the home view's place");
    Dump("closed_timed_out");
    Advance(1500);
    Check(router::Depth() == 2, "the page stays its 1.8 s");
    Advance(400);
    Check(router::Depth() == 1, "then leaves, back to where the request came from");
    Check(home::Look().page == 3, "on the asking agent's page");
    Check(s_decided.count >= 0 && model::FindItem(1001) == nullptr, "the request is gone from the queue");
}

// Answered in the terminal: 2.3 s.
void TestClosed()
{
    TestClosedTimedOut();
    DeckState();
    router::PushRoute(flow::kRouteRequest, "1001");
    Settle();
    model::CloseRequest(1001, model::CloseReason::kAnsweredElsewhere);
    Settle();
    Check(Text("Answered") != nullptr && Text("in the terminal") != nullptr, "answered: the block says so");
    Dump("closed_answered");
    Advance(2000);
    Check(router::Depth() == 2, "the page stays its 2.3 s");
    Advance(500);
    Check(router::Depth() == 1, "then leaves");
    // Closed with another waiting: the list opens in its place.
    DeckState();
    model::SetAgent(Agent(5, 0, "docs-site", model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    Ask(1002, 5, "WebFetch", "u");
    router::PushRoute(flow::kRouteRequest, "1001");
    Settle();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    Advance(2000);
    Check(router::Depth() >= 1, "a close with another request waiting leaves");
    Home();
    // The route shows the page alone and leaves back.
    Check(router::PushRoute(flow::kRouteClosed, "answered") == ESP_OK && router::PushRoute(flow::kRouteClosed, "x") != ESP_OK, "the closed page has its route");
    Settle();
    Check(Text("Answered") != nullptr, "by itself");
    Advance(2500);
    Check(router::Depth() == 1, "and it leaves after its hold");
}

// --- A covered home's mascot waits ---------------------------------------------------------------------------------------------------------

// A stage whose screen LVGL deletes knows its mascot is gone, and stops without reading it: a screen's objects go before the
// screen object that owns its stage is freed (STICKY_AGENT_DECK §8.2).
void TestStageOutlivesItsMascot()
{
    std::printf("a stage whose mascot is deleted\n");
    lv_obj_t *parent = lv_obj_create(lv_screen_active());
    {
        agent_deck::Stage stage;
        stage.Build(parent, 0, 0, dt::kSpacingMascotCell / 4);
        Check(stage.Built(), "the stage holds its mascot while its screen does");
        lv_obj_delete(parent);
        Check(!stage.Built(), "the mascot's deletion clears the stage's hold on it");
        stage.Stop();  // reads nothing LVGL freed
        stage.ToFront();
    }
    Check(true, "the stage stops and goes with its mascot already deleted");
}

void TestCovered()
{
    std::printf("a covered home\n");
    DeckState();
    Settle();
    const uint32_t running = lv_anim_count_running();
    Check(running > 0, "the mascot plays on the overview");
    router::PushRoute(flow::kRouteRequest, "1001");
    Settle();
    const uint32_t over = lv_anim_count_running();
    (void)over;
    Home();
    Settle();
    Check(lv_anim_count_running() > 0, "and plays again when home is back on top");
}

// --- The settings, the Wi-Fi page and standby (STICKY_AGENT_DECK §4.7–§4.9) ------------------------------------------------------------

// The hardware test's home, which the settings' last row opens: a stand-in screen with a Back, since that feature is not linked here.
std::unique_ptr<router::Screen> HardwareStub()
{
    return router::MakeScreen(
        [](lv_obj_t *root, lv_group_t *, void *) {
            ui::StyleScreen(root);
            lv_obj_t *back = widgets::MakeButton(root, "Back", widgets::ButtonVariant::kOutline, 160);
            lv_obj_add_event_cb(back, [](lv_event_t *) { router::Pop(); }, LV_EVENT_CLICKED, nullptr);
        });
}

model::Wifi WifiRecord(model::WifiStage stage, const char *name = "Studio")
{
    model::Wifi w;
    w.stage = stage;
    std::snprintf(w.network.name, sizeof w.network.name, "%s", name);
    w.network.locked = true;
    w.network.signal = 3;
    const char *facts[][2] = {{"IP", "192.168.1.42"}, {"Host", "agentdeck-3f2a.local"}, {"Bridge", "Wi-Fi"},
                              {"Router", "192.168.1.1"}, {"Subnet", "255.255.255.0"}, {"MAC", "02:3F:2A:00:00:42"}};
    for (const auto &f : facts) {
        std::snprintf(w.facts[w.fact_count].name, sizeof w.facts[0].name, "%s", f[0]);
        std::snprintf(w.facts[w.fact_count].value, sizeof w.facts[0].value, "%s", f[1]);
        ++w.fact_count;
    }
    const struct { const char *name; bool locked; int signal; } nets[] = {
        {"Lab-2G-Upstairs-Annex", true, 3}, {"Studio", true, 3}, {"Studio-Guest", false, 2}, {"Studio-Upstairs-Guest-5G", true, 1}};
    for (const auto &n : nets) {
        model::Network &to = w.networks[w.network_count++];
        std::snprintf(to.name, sizeof to.name, "%s", n.name);
        to.locked = n.locked;
        to.signal = static_cast<uint8_t>(n.signal);
    }
    return w;
}

struct Asked {
    int count = 0;
    model::WifiAsk ask = model::WifiAsk::kConnect;
    int arg = -1;
} s_asked;
void OnWifiAsk(model::WifiAsk ask, int arg)
{
    ++s_asked.count;
    s_asked.ask = ask;
    s_asked.arg = arg;
}

void SettingsState()
{
    mock::Stop();
    DeckState();
    model::SetSoundOn(true);
    model::SetAutoSleepMinutes(30);
    model::SetTimeZone(0);
    model::SetWifiOn(true);
    model::SetBattery({84, false});
    model::SetWifiListener(OnWifiAsk);
    model::SetWifi(WifiRecord(model::WifiStage::kJoined));
    s_asked = {};
}

// The button that says `words`: its focus stop, or — for one out of the group, as Connect and Review are — the object holding the label.
lv_obj_t *Button(const std::string &words)
{
    lv_obj_t *label = Text(words);
    if (label == nullptr) {
        return nullptr;
    }
    lv_obj_t *control = ControlOf(label);
    return control != nullptr ? control : lv_obj_get_parent(label);
}

// Presses the control as OK does: focused, then OK.
void PressOn(lv_obj_t *control)
{
    router::Ring(control);
    Settle();
    Press(LV_KEY_ENTER);
    Settle();
}

bool Is(lv_obj_t *label, ui::Type role) { return label != nullptr && lv_obj_get_style_text_font(label, LV_PART_MAIN) == ui::Font(role); }

// The joined network's row: a card on the light dither with no edge (the person, 2026-11-26: "light grey background not outline"), its
// name and its facts black, and the row neither the chosen inversion nor a plain row at rest.
void CheckJoinedOnWhite(lv_obj_t *net, const std::string &what)
{
    const lv_color_t black = ui::Level(dt::kColorTextPrimary);
    Check(net != nullptr && lv_color_eq(lv_obj_get_style_bg_color(net, LV_PART_MAIN), ui::Fill(dt::kColorBand)), what + ": the card's light dither");
    Check(net != nullptr && lv_obj_get_style_border_width(net, LV_PART_MAIN) == 0, what + ": no outline");
    Check(net != nullptr && !lv_obj_has_state(net, LV_STATE_CHECKED), what + ": not the chosen inversion");
    Check(net != nullptr && lv_obj_get_height(net) == dt::kSpacingListRowHeight, what + ": still 80 px high");
    bool inked = net != nullptr;
    for (const char *words : {"Studio", "Connected", "Locked"}) {
        lv_obj_t *word = Text(words);
        inked = inked && word != nullptr && lv_color_eq(lv_obj_get_style_text_color(word, LV_PART_MAIN), black);
    }
    Check(inked, what + ": black words");
}

// The heading's edge holds the words that take no ring and every card's box; a card's words stand `spacing.md` inside it; and the scroll
// indicator's lane stands clear of the rows' right edge.
void CheckOneLeftEdge(std::initializer_list<const char *> bare, std::initializer_list<const char *> carded, lv_obj_t *lane, lv_obj_t *box,
                      const std::string &page)
{
    // Words that take no ring on the heading's edge; a card on that edge with its words `spacing.md` inside (the person, 2026-11-26).
    for (const char *w : bare) {
        lv_obj_t *label = Text(w);
        Check(label != nullptr && X(label) == dt::kSpacingEdge,
              page + ": " + w + " starts on the heading's edge, x 24" + (label != nullptr ? ", not " + std::to_string(X(label)) : ""));
    }
    for (const char *w : carded) {
        lv_obj_t *label = Text(w);
        lv_obj_t *card = label != nullptr ? ControlOf(label) : nullptr;
        Check(card != nullptr && X(card) == dt::kSpacingEdge && X(label) == dt::kSpacingEdge + dt::kSpacingMd,
              page + ": " + w + "'s card on the heading's edge, its words spacing.md inside" + (label != nullptr ? ", at " + std::to_string(X(label)) : ""));
    }
    Check(lane != nullptr && box != nullptr && Area(box).x2 < Area(lane).x1, page + ": the scroll indicator's lane stands clear of the rows' right edge");
}

// The settings' controls one step finds and the next ones press.
struct SettingsStops {
    lv_obj_t *sound = nullptr, *wifi_row = nullptr, *net = nullptr, *zone = nullptr, *back = nullptr, *hardware = nullptr;
    std::vector<lv_obj_t *> walk;
};

SettingsStops TestSettingsPage()
{
    std::printf("the settings\n");
    SettingsState();
    Home();
    Settle();
    const int home_y = Y(home::Look().chip);
    (void)home_y;
    // The foot's Settings icon opens it, and Back goes back to home.
    PressOn(home::Look().settings);
    Check(router::Depth() == 2, "the Settings icon opens the settings over home");
    Dump("settings_opened");

    // The page: heading and clock, no status bar; every control's words in caption, the Sound label's size.
    lv_obj_t *heading = Text("Settings");
    Check(heading != nullptr && X(heading) == dt::kSpacingEdge && Y(heading) == L::kHeadingTop && Is(heading, ui::Type::kTitle),
          "Settings in title at the heading's place");
    Check(Text("Fri 7 Nov  2:32 PM") != nullptr && Area(Text("Fri 7 Nov  2:32 PM")).y1 == L::kClockTop, "the clock top-right");
    Check(Text("On battery only") == nullptr, "no On battery only beside Auto sleep");
    for (const char *words : {"Sound", "On", "Auto sleep", "Wi-Fi", "Time zone", "Pacific", "Battery"}) {
        Check(Is(Text(words), ui::Type::kCaption), std::string("caption, the Sound label's size: ") + words);
    }
    lv_obj_t *charged = Text("Charged \xE2\x80\xA2 84%");
    Check(charged != nullptr && Is(charged, ui::Type::kCaption), "the battery row reads Charged \xE2\x80\xA2 84%, in caption");
    Check(Text("Studio") != nullptr && Text("Connected") != nullptr, "the joined network's row says Studio, Connected");
    Check(Is(Text("Studio"), ui::Type::kCaptionStrong) && Is(Text("Connected"), ui::Type::kCaptionRegular), "the settings' row: the name in caption-strong over its status in caption-regular");

    // The rows: the toggle, the slider, the toggle, the network, the dropdown, the reading — one column, each a target or more apart.
    lv_obj_t *sound = lv_group_get_obj_by_index(s_group, 0);  // the toggle, the walk's first stop
    lv_obj_t *wifi_row = Text("Wi-Fi");
    lv_obj_t *net = ControlOf(Text("Studio"));
    lv_obj_t *zone = ControlOf(Text("Time zone"));
    lv_obj_t *battery_row = Text("Battery");
    Check(Y(Text("Sound")) < Y(Text("Auto sleep")), "Sound stands over Auto sleep");
    Check(Y(Text("Auto sleep")) < Y(wifi_row) && Y(wifi_row) < Y(Text("Studio")) && Y(Text("Studio")) < Y(Text("Time zone")) &&
              Y(Text("Time zone")) < Y(battery_row),
          "in order: Sound, Auto sleep, Wi-Fi, the network, Time zone, Battery");
    Check(net != nullptr && lv_obj_get_height(net) == dt::kSpacingListRowHeight, "the network is the kit's 80 px row");
    CheckJoinedOnWhite(net, "the settings' joined network");
    Check(ControlOf(battery_row) == nullptr, "the battery is a reading: no place in the walk");
    Check(Text("Fri 7 Nov  2:32 PM") != nullptr && Text("Back") != nullptr, "Back is a word");
    lv_obj_t *back = Button("Back");
    Check(back != nullptr && X(back) == dt::kSpacingEdge && Y(back) == L::kAnswersTop && lv_obj_get_width(back) == dt::kSpacingButtonBack,
          "Back stands bottom-left at 704, 160 wide");
    Check(back != nullptr && lv_obj_get_style_border_width(back, LV_PART_MAIN) == dt::kSpacingStrokeDefault, "an outline button");
    Check(Text("Time zone") != nullptr && Text("Battery detail") == nullptr, "no battery page and no details");
    SettingsStops st;
    st.sound = sound;
    st.wifi_row = wifi_row;
    st.net = net;
    st.zone = zone;
    st.back = back;
    return st;
}

// It opens with no ring; the first press shows it on Sound. Then the walk, in the order the person reads the page.
void TestSettingsWalk(SettingsStops &st)
{
    lv_obj_t *const sound = st.sound;
    Check(Focused() == sound && !Ringed(Focused()), "it opens with no ring, on Sound");
    std::vector<lv_obj_t *> &walk = st.walk;
    walk.push_back(Focused());
    router::Ring(Focused());
    Settle();
    Dump("settings_ring_0");
    for (int i = 0; i < 6; ++i) {
        lv_group_focus_next(s_group);
        walk.push_back(Focused());
    }
    lv_obj_t *hardware = Button("Hardware test");
    Check(Area(walk[1]).y1 > Y(Text("Auto sleep")) && Area(walk[2]).y1 < Y(Text("Studio")), "the walk: Sound, the slider, the Wi-Fi switch");
    st.hardware = hardware;
    Check(walk[3] == st.net && walk[4] == st.zone && walk[5] == hardware && walk[6] == st.back, "then the network, Time zone, Hardware test and Back");
    Check(hardware != nullptr && lv_obj_get_height(hardware) == dt::kSpacingListRowHeight && Is(Text("Hardware test"), ui::Type::kCaption),
          "Hardware test is the kit's list row, its words caption");
    lv_group_focus_next(s_group);
    Check(Focused() == sound, "and wraps to Sound");
}

// The rows page, whole rows a page: page 1 the six with Hardware test hidden, page 2 from Wi-Fi to Hardware test.
void TestSettingsPages(const SettingsStops &st)
{
    lv_obj_t *const sound = st.sound, *const hardware = st.hardware, *const wifi_row = st.wifi_row;
    Settle();
    Check(!lv_obj_is_visible(hardware) && lv_obj_is_visible(sound), "page 1: the approved six, Hardware test on the next page");
    lv_obj_t *lane_1 = nullptr;
    {
        lv_obj_t *lane = nullptr;  // the kit's scroll indicator: the one object `spacing.scroll-thumb` wide on the page
        lv_obj_t *page = lv_obj_get_child(lv_screen_active(), 0);
        for (uint32_t i = 0; i < lv_obj_get_child_count(page) && lane == nullptr; ++i) {
            lv_obj_t *c = lv_obj_get_child(page, static_cast<int32_t>(i));
            if (lv_obj_get_child_count(c) == 1 && lv_obj_get_width(lv_obj_get_child(c, 0)) == dt::kSpacingScrollThumb) {
                lane = lv_obj_get_child(c, 0);
            }
        }
        Check(lane != nullptr && !lv_obj_has_flag(lane, LV_OBJ_FLAG_HIDDEN), "the scroll indicator shows there is more");
        lane_1 = lane;
    }
    CheckOneLeftEdge({"Sound", "Auto sleep", "Wi-Fi", "Battery"}, {"Studio", "Time zone"}, lane_1, st.net, "page 1");
    const int32_t wifi_on_page_1 = Y(wifi_row);
    router::Ring(hardware);
    Settle();
    Check(lv_obj_is_visible(hardware) && !lv_obj_is_visible(sound) && Y(wifi_row) < wifi_on_page_1,
          "the walk to Hardware test turns to page 2: Wi-Fi at its top, Hardware test last");
    Check(Area(hardware).y2 < L::kAnswersTop - dt::kSpacingMd, "Hardware test ends clear of Back's ring");
    Dump("settings_page_2");
    CheckOneLeftEdge({"Wi-Fi", "Battery"}, {"Studio", "Time zone", "Hardware test"}, lane_1, st.net, "page 2");
    router::Ring(st.walk[1]);
    Settle();
    Check(!lv_obj_is_visible(hardware) && Y(wifi_row) == wifi_on_page_1, "back to the slider is page 1 again");
    router::Ring(sound);
    Settle();
}

// The settings' own: Sound flips the model's setting the foot shares; the Wi-Fi switch tells the link.
void TestSettingsOwn(SettingsStops &st)
{
    lv_obj_t *const sound = st.sound;
    lv_obj_t *wifi_toggle = st.walk[2];
    PressOn(sound);
    Check(!model::SoundOn() && devices::Muted(), "Sound off turns the setting off, and mutes the whole buzzer");
    PressOn(sound);
    Check(model::SoundOn() && !devices::Muted(), "and on again, the buzzer with it");
    PressOn(wifi_toggle);
    Check(!model::WifiOn() && s_asked.ask == model::WifiAsk::kSwitch && s_asked.arg == 0, "the Wi-Fi switch off turns the setting off and tells the link");
    Settle();
    Check(!lv_obj_is_visible(Button("Hardware test")) && lv_obj_is_visible(Text("Sound")),
          "the page built again from page 1 stays on page 1: the old rows' refocus turns no page");
    Check(Text("Wi-Fi off") != nullptr && Text("Studio") == nullptr, "the network row says Wi-Fi off, greyed, never hidden");
    Check(lv_obj_has_state(lv_obj_get_parent(Text("Wi-Fi off")), LV_STATE_DISABLED), "the row is disabled: out of reach, never hidden");
    model::SetWifiOn(true);
    Settle();
    Check(Text("Studio") != nullptr, "switched on again with Studio joined, the row is Studio's");
    // The page was built again: find the controls anew.
    st.net = ControlOf(Text("Studio"));
    st.back = Button("Back");
}

// Auto sleep: a range slider in steps of five; OK opens it, UP steps it, OK closes it.
void TestSettingsSlider()
{
    lv_obj_t *sleep = nullptr;
    for (uint32_t i = 0; i < lv_group_get_obj_count(s_group); ++i) {
        lv_obj_t *o = lv_group_get_obj_by_index(s_group, i);
        lv_area_t a = Area(o);
        if (lv_area_get_height(&a) == dt::kSpacingSliderHeight) {
            sleep = o;
        }
    }
    Check(sleep != nullptr, "Auto sleep is the kit's range slider");
    if (sleep != nullptr) {
        const widgets::RangeSlider slider{sleep, nullptr, nullptr, nullptr};
        Check(widgets::RangeSliderValue(slider) == 30 && model::AutoSleepMinutes() == 30, "at 30 min");
        router::Focus(sleep);
        Press(LV_KEY_ENTER);  // shows the ring
        Press(LV_KEY_ENTER);  // opens it for editing
        Press(LV_KEY_UP);
        Press(LV_KEY_ENTER);
        Check(widgets::RangeSliderValue(slider) == 35 && model::AutoSleepMinutes() == 35, "UP steps it five minutes and the setting follows");
        Check(Text("35 min") != nullptr, "the value reads in the fill");
        model::SetAutoSleepMinutes(30);
    }
}

// Time zone: the kit's dropdown over the kit's select sheet of fifteen zones; no page of its own.
void TestSettingsSheetAndRows(const SettingsStops &st)
{
    Check(st.zone != nullptr, "Time zone is a stop");
    PressOn(ControlOf(Text("Time zone")));
    Check(widgets::SelectSheetOpen() && router::Depth() == 2, "OK opens the kit's select sheet, not a Time zone page");
    Dump("settings_zone_sheet");
    Check(Text("Mountain") != nullptr && Text("Central") != nullptr, "the sheet lists the zones, Pacific chosen");
    lv_group_focus_next(s_group);
    Settle();
    Press(LV_KEY_ENTER);
    Settle();
    Check(!widgets::SelectSheetOpen() && model::TimeZone() == 1 && Text("Mountain") != nullptr, "a choice commits at once: Mountain");
    model::SetTimeZone(0);
    Settle();

    // Battery: the word follows the charger and the percent, in place.
    model::SetBattery({62, true});
    Settle();
    Check(Text("Charging \xE2\x80\xA2 62%") != nullptr && Text("Charged \xE2\x80\xA2 84%") == nullptr, "on the charger the row reads Charging \xE2\x80\xA2 62%");
    Dump("settings_charging");
    // The deck's own words under 80 % and 20 %, approved by the ux-content-writer for the person: each stands whole in its row.
    for (const auto &[percent, words] : {std::pair<uint8_t, const char *>{72, "On battery \xE2\x80\xA2 72%"},
                                         std::pair<uint8_t, const char *>{15, "Low battery \xE2\x80\xA2 15%"}}) {
        model::SetBattery({percent, false});
        Settle();
        lv_obj_t *value = Text(words);
        const lv_font_t *font = value != nullptr ? lv_obj_get_style_text_font(value, LV_PART_MAIN) : nullptr;
        lv_text_attributes_t attributes{};
        attributes.max_width = LV_COORD_MAX;  // one line, unbroken
        const int32_t needs = font != nullptr ? lv_text_get_width(words, static_cast<uint32_t>(std::strlen(words)), font, &attributes) : 0;
        Check(value != nullptr && lv_obj_get_width(value) >= needs && Area(value).x2 <= dt::kSpacingEdge + dt::kSpacingContent,
              std::string("the battery row reads ") + words + ", whole, inside the content");
    }
    Dump("settings_low_battery");
    model::SetBattery({84, false});
    Settle();

    // The network opens the Wi-Fi page; Back from it returns here, on the row.
    PressOn(ControlOf(Text("Studio")));
    Check(router::Depth() == 3 && Text("Wi-Fi") != nullptr && Text("Disconnect") != nullptr, "the network row opens the Wi-Fi page");
    router::Pop();
    Settle();
    Check(router::Depth() == 2 && Focused() == ControlOf(Text("Studio")), "Back from it is the settings, on the network row");

    // Hardware test: its home in the settings' place — home, the hardware test, Display and one of Display's pages fit in four.
    PressOn(Button("Hardware test"));
    Settle();
    Check(router::Depth() == 2 && Text("Sound") == nullptr && lv_obj_get_child_count(lv_screen_active()) > 0,
          "Hardware test opens hw.home in the settings' place");
    Check(router::PushRoute("hw.display") == ESP_OK && router::PushRoute("hw.display.grays") == ESP_OK && router::Depth() == 4,
          "and Display's own pages open from it: the fourth screen, the stack's depth");
    router::Pop();
    router::Pop();
    router::Pop();
    Settle();
    Check(router::Depth() == 1, "Back from the hardware test's home is the deck's home");
    PressOn(home::Look().settings);
    Settle();
    Check(router::Depth() == 2 && Text("Sound") != nullptr, "and the settings open again from its foot");
}

// Not joined: the row says so and still opens the page.
void TestSettingsBack()
{
    model::SetWifi(WifiRecord(model::WifiStage::kNotJoined));
    Settle();
    Check(Text("Not joined") != nullptr && Text("Studio") == nullptr, "with no network joined the row says Not joined");

    // Back, and holding OK, go back to home on the Settings icon; the foot's Sound follows what the settings set while home was covered.
    PressOn(lv_group_get_obj_by_index(s_group, 0));  // the walk's first stop: Sound's toggle
    Check(!model::SoundOn(), "Sound off in the settings");
    lv_obj_t *back = Button("Back");
    PressOn(back);
    Check(router::Depth() == 1, "Back returns to home");
    Check(!lv_obj_has_state(home::Look().sound, LV_STATE_CHECKED), "home's foot Sound shows off: the one setting, followed as home comes back");
    PressOn(home::Look().sound);
    Check(model::SoundOn() && lv_obj_has_state(home::Look().sound, LV_STATE_CHECKED), "and the foot turns it on again");
    PressOn(home::Look().settings);
    Check(router::Escape() && router::Depth() == 1, "holding OK goes back too");
}

void TestSettings()
{
    SettingsStops st = TestSettingsPage();
    TestSettingsWalk(st);
    TestSettingsPages(st);
    TestSettingsOwn(st);
    TestSettingsSlider();
    TestSettingsSheetAndRows(st);
    TestSettingsBack();
}

lv_obj_t *TestWifiJoined()
{
    std::printf("the Wi-Fi page\n");
    SettingsState();
    Home();
    Settle();
    lv_obj_send_event(home::Look().wifi, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 2 && Text("Wi-Fi") != nullptr, "the foot's Wi-Fi icon opens the page");

    // Joined: the network a control, its facts as detail rows with their values one size down, Back and Disconnect in halves.
    lv_obj_t *title = Text("Wi-Fi");
    Check(Is(title, ui::Type::kTitle) && X(title) == dt::kSpacingEdge && Y(title) == L::kHeadingTop, "Wi-Fi in title at the heading's place");
    lv_obj_t *net = ControlOf(Text("Studio"));
    Check(net != nullptr && Focused() == net && !Ringed(net), "the network row is the page's first stop, with no ring on opening");
    CheckJoinedOnWhite(net, "the Wi-Fi page's joined network");
    Check(Text("Forget") == nullptr, "no Forget button on the page");
    lv_obj_t *ip = Text("192.168.1.42");
    Check(ip != nullptr && Is(ip, ui::Type::kCaption) && Is(Text("IP"), ui::Type::kCaptionStrong), "a fact's label in caption-strong over its value in caption, one size down");
    Check(Is(Text("Studio"), ui::Type::kCaptionStrong) && Is(Text("Connected"), ui::Type::kCaptionRegular), "the joined network's name in caption-strong over its status in caption-regular");
    lv_obj_t *back = Button("Back");
    lv_obj_t *disconnect = Button("Disconnect");
    Check(back != nullptr && disconnect != nullptr && X(back) == dt::kSpacingEdge && Y(back) == L::kAnswersTop && lv_obj_get_width(back) == L::kHalfW &&
              X(disconnect) == L::kHalfRightX && Y(disconnect) == L::kAnswersTop && lv_obj_get_width(disconnect) == L::kHalfW,
          "Back and Disconnect in two halves at 704");
    Check(X(disconnect) - (X(back) + L::kHalfW) == dt::kSpacingMd, "16 px apart");
    Dump("wifi_joined");
    router::Ring(net);
    Settle();
    Dump("wifi_joined_ringed");
    router::ClearRing();
    return net;
}

// The Forget dialog: the network opens it, destructive, focus on Cancel.
void TestWifiForgetAndWalk(lv_obj_t *net)
{
    PressOn(net);
    Check(widgets::DialogOpen() && Text("Forget Studio?") != nullptr && Text("Forget") != nullptr, "the network opens the Forget dialog");
    Check(Focused() == Button("Cancel"), "it opens on Cancel, as a destructive action does");
    Dump("wifi_forget");
    Press(LV_KEY_ENTER);
    Settle();
    Check(!widgets::DialogOpen() && s_asked.count == 0, "Cancel closes it and asks nothing");
    PressOn(net);
    router::Focus(Button("Forget"));
    Press(LV_KEY_ENTER);
    Settle();
    Check(!widgets::DialogOpen() && s_asked.ask == model::WifiAsk::kForget, "Forget asks the link to forget the network");

    // Disconnect asks; Back goes back.
    lv_obj_send_event(Button("Disconnect"), LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kDisconnect, "Disconnect asks the link");

    // The walk in order: the network, the facts' pages, Back, Disconnect.
    lv_group_focus_obj(net);
    std::vector<lv_obj_t *> walk;
    for (int i = 0; i < 6; ++i) {
        walk.push_back(Focused());
        lv_group_focus_next(s_group);
    }
    {
        size_t at_back = 0, at_disconnect = 0;
        for (size_t i = 0; i < walk.size(); ++i) {
            at_back = walk[i] == Button("Back") ? i : at_back;
            at_disconnect = walk[i] == Button("Disconnect") ? i : at_disconnect;
        }
        Check(walk[0] == net && at_back > 1 && at_disconnect == at_back + 1, "the walk: the network, the facts' stops, Back, Disconnect");
    }
}

// Each state, from the record: the stacked status centred on the glass's height and across it.
void CheckCentred(lv_obj_t *inside, lv_obj_t *, const std::string &what)
{
    const lv_area_t a = Area(lv_obj_get_parent(inside));  // the block, or the column, that holds the state's content
    const int middle = (a.y1 + a.y2) / 2;
    Check(std::abs(middle - kH / 2) <= 3, what + ": centred on the glass's height");
}

void TestWifiNotConnected()
{
    model::SetWifi(WifiRecord(model::WifiStage::kNotJoined));
    Settle();
    Dump("wifi_not_connected");
    lv_obj_t *head = Text("Not connected");
    Check(head != nullptr && Text("Connect asks your Mac for its network.") != nullptr, "not connected: the status says so");
    Check(std::abs((Area(head).x1 + Area(head).x2) / 2 - kW / 2) <= 1, "its heading centred across the glass");
    Check(Y(head) > L::kHeadingTop, "icon above the message: the mark stands over the heading");
    CheckCentred(head, Text("Connect asks your Mac for its network."), "not connected");
    lv_obj_t *connect = Button("Connect");
    Check(connect != nullptr && Focused() == connect && X(connect) == L::kHalfRightX && Y(connect) == L::kAnswersTop && X(Button("Back")) == dt::kSpacingEdge,
          "Back | Connect in halves, on Connect");
    lv_obj_send_event(connect, LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kConnect, "Connect asks the link");
}

void TestWifiChoosing()
{
    model::SetWifi(WifiRecord(model::WifiStage::kChoosing));
    Settle();
    Dump("wifi_choose");
    Check(Text("Networks") != nullptr && Text("Pick the Mac's network") != nullptr, "networks: the heading and its line");
    Check(Text("Lab-2G-Upstairs-Annex") != nullptr && Shown("Studio-Upstairs-Guest-5G") != nullptr, "the Mac's networks, four to a page");
    Check(Focused() == ControlOf(Text("Lab-2G-Upstairs-Annex")), "the first network has the focus");
    Check(Is(Text("Lab-2G-Upstairs-Annex"), ui::Type::kCaptionStrong) && Is(Text("Studio-Guest"), ui::Type::kCaptionStrong),
          "the networks' names in the list are caption-strong");
    lv_obj_send_event(ControlOf(Text("Studio-Guest")), LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kPick && s_asked.arg == 2, "a network asks the link to join it, by its place");
    lv_obj_t *scan = Button("Scan");
    Check(scan != nullptr && X(scan) == L::kHalfRightX && X(Button("Cancel")) == dt::kSpacingEdge, "Cancel | Scan in halves");
    lv_obj_send_event(scan, LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kScan, "Scan asks the link for the networks again");
    lv_obj_send_event(Button("Cancel"), LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kCancel, "Cancel asks the link to stop");
}

void TestWifiJoining()
{
    model::SetWifi(WifiRecord(model::WifiStage::kJoining));
    Settle();
    Dump("wifi_joining");
    Check(Text("Step 2 of 3") != nullptr && Text("Joining Studio") != nullptr && Text("This takes a few seconds.") != nullptr, "joining: step 2 of 3");
    CheckCentred(Text("Step 2 of 3"), Text("This takes a few seconds."), "joining");
    lv_obj_t *cancel = Button("Cancel");
    Check(cancel != nullptr && Focused() == cancel && std::abs(X(cancel) + lv_obj_get_width(cancel) / 2 - kW / 2) <= 1 && Y(cancel) == L::kAnswersTop,
          "Cancel centred at 704, focused");
    s_asked = {};
    Check(router::Escape() && s_asked.ask == model::WifiAsk::kCancel && router::Depth() == 2, "holding OK cancels the join, it does not leave");

    model::SetWifi(WifiRecord(model::WifiStage::kFailed));
    Settle();
    Dump("wifi_failed");
    Check(Text("Wrong password") != nullptr && Text("The network refused your Mac's password.") != nullptr, "wrong password: the status says so");
    Check(Focused() == Button("Retry") && X(Button("Cancel")) == dt::kSpacingEdge, "Cancel | Retry, on Retry");
    lv_obj_send_event(Button("Retry"), LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kRetry, "Retry asks the link");

    model::SetWifi(WifiRecord(model::WifiStage::kPaired));
    Settle();
    Dump("wifi_paired");
    Check(Text("Bridge paired") != nullptr && Focused() == Button("Done"), "paired: Done");
    lv_obj_send_event(Button("Done"), LV_EVENT_CLICKED, nullptr);
    Check(s_asked.ask == model::WifiAsk::kDone, "Done asks the link");
    Check(router::Escape() && s_asked.ask == model::WifiAsk::kDone, "holding OK is Done");
}

// Wi-Fi off: the page says so and Connect is out of reach.
void TestWifiOff()
{
    model::SetWifi(WifiRecord(model::WifiStage::kNotJoined));
    model::SetWifiOn(false);
    Settle();
    Check(Text("Wi-Fi off") != nullptr && Text("Turn Wi-Fi on in Settings to connect.") != nullptr, "Wi-Fi off: the page says so");
    Check(lv_obj_has_state(Button("Connect"), LV_STATE_DISABLED) && Focused() == Button("Back"), "Connect is disabled, Back has the focus");
    model::SetWifiOn(true);
    Settle();
    lv_obj_send_event(Button("Back"), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 1, "Back goes home");
}

void TestWifi()
{
    TestWifiForgetAndWalk(TestWifiJoined());
    TestWifiNotConnected();
    TestWifiChoosing();
    TestWifiJoining();
    TestWifiOff();
}

// A swipe up or down begun anywhere on a paged screen turns its pages, not only one begun on its rows (the person, 2026-11-26).
// The Wi-Fi page's facts run to a second page of one fact: a swipe up from Back turns to it and presses nothing, one down from the
// heading turns back. The settings turn from their heading too, and a ring on a stop the new page hides moves to one it shows.
void TestSwipeWifi()
{
    std::printf("a swipe anywhere on a paged screen\n");
    SettingsState();
    Home();
    Settle();
    lv_obj_send_event(home::Look().wifi, LV_EVENT_CLICKED, nullptr);
    Settle();
    lv_obj_t *ip = Text("192.168.1.42");
    lv_obj_t *mac = Text("02:3F:2A:00:00:42");
    const int ip_on_1 = Y(ip);
    Check(ip != nullptr && mac != nullptr && !lv_obj_is_visible(mac), "Wi-Fi: the last fact waits alone on the second page");
    lv_obj_t *back = Button("Back");
    const int back_y = Y(back) + dt::kSpacingTarget / 2;
    Swipe(120, back_y, 120, back_y - 2 * dt::kTouchSwipeDistance);
    Check(router::Depth() == 2 && lv_obj_is_visible(mac) && Y(ip) != ip_on_1, "a swipe up begun on Back turns to it, and Back is not pressed");
    lv_obj_t *page = lv_obj_get_child(lv_screen_active(), 0);
    Check(lv_obj_get_scroll_y(page) == 0, "and the page under the swipe never moves");
    const int heading_y = Y(Text("Wi-Fi")) + dt::kSpacingMd;
    Swipe(240, heading_y, 240, heading_y + 2 * dt::kTouchSwipeDistance);
    Check(Y(ip) == ip_on_1 && !lv_obj_is_visible(mac), "a swipe down begun on the heading turns back to the first page");
    Home();
}

void TestSwipeSettings()
{
    SettingsState();
    Home();
    Settle();
    lv_obj_send_event(home::Look().settings, LV_EVENT_CLICKED, nullptr);
    Settle();
    lv_obj_t *sound = lv_group_get_obj_by_index(s_group, 0);        // the walk: Sound's toggle, the slider, Wi-Fi's toggle, …
    lv_obj_t *wifi_toggle = lv_group_get_obj_by_index(s_group, 2);
    lv_obj_t *hardware = ControlOf(Text("Hardware test"));
    router::Ring(sound);
    Settle();
    lv_obj_t *page = lv_obj_get_child(lv_screen_active(), 0);
    const int settings_y = Y(Text("Settings")) + dt::kSpacingMd;
    Swipe(240, settings_y + 2 * dt::kTouchSwipeDistance, 240, settings_y);
    Check(lv_obj_is_visible(hardware) && !lv_obj_is_visible(Text("Sound")), "settings: a swipe up begun on the heading turns to the second page");
    Check(Focused() == wifi_toggle, "the focus leaves Sound, which the page hides, for Wi-Fi's toggle, the new page's first stop");
    Check(lv_obj_get_scroll_y(page) == 0, "and the page did not move with the focus the turn moved");
    Swipe(240, settings_y, 240, settings_y + 2 * dt::kTouchSwipeDistance);
    Check(!lv_obj_is_visible(hardware) && lv_obj_is_visible(Text("Sound")), "a swipe down turns back to the first");
    Check(Focused() == wifi_toggle, "Wi-Fi's toggle stands on both pages, so the focus stays");
    s_at_ends = 0;
    Swipe(240, settings_y, 240, settings_y + 2 * dt::kTouchSwipeDistance);
    Check(!lv_obj_is_visible(hardware) && s_at_ends == 1, "a swipe down on the first page turns nothing, and the at-end cue says so");
    router::Ring(hardware);  // the walk to a stop on the hidden page: LVGL scrolls to it, and the page puts itself back
    Settle();
    Check(lv_obj_is_visible(hardware) && lv_obj_get_scroll_y(page) == 0, "the walk turns the page and the page itself never scrolls");
    Swipe(240, settings_y, 240, settings_y + 2 * dt::kTouchSwipeDistance);
    Check(!lv_obj_is_visible(hardware) && Focused() == sound && lv_obj_get_scroll_y(page) == 0,
          "a swipe down from Hardware test, which the first page hides, turns back with the focus on Sound, the page unmoved");
    Home();
}

void TestSwipeWaiting()
{
    // The needs-you list never pages, four at most: a swipe down begun on its heading turns nothing, and the at-end cue says so.
    DeckState();
    model::SetAgent(Agent(5, 0, "docs-site", model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    Ask(1002, 5, "WebFetch", "u");
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    Check(Text("Needs you") != nullptr, "the needs-you list is open");
    s_at_ends = 0;
    const int list_y = Y(Text("Needs you")) + dt::kSpacingMd;
    Swipe(240, list_y, 240, list_y + 2 * dt::kTouchSwipeDistance);
    Check(s_at_ends == 1 && router::Depth() == 2 && Text("Needs you") != nullptr,
          "the list: a swipe down begun on the heading answers with the at-end cue and leaves nothing");
    Home();
}

void TestWifiMock()
{
    std::printf("the Wi-Fi mock\n");
    mock::Stop();
    DeckState();
    model::SetWifiOn(true);
    model::SetWifi(model::Wifi{});
    Home();
    Settle();
    Check(mock::Start(), "the mock starts");
    Check(model::GetWifi().stage == model::WifiStage::kJoined && std::string(model::GetWifi().network.name) == "Studio", "the deck starts joined to Studio");
    lv_obj_send_event(home::Look().wifi, LV_EVENT_CLICKED, nullptr);
    Settle();
    lv_obj_send_event(Button("Disconnect"), LV_EVENT_CLICKED, nullptr);
    Advance(300);
    Check(model::GetWifi().stage == model::WifiStage::kNotJoined && Text("Not connected") != nullptr, "Disconnect leaves the network: not connected");
    lv_obj_send_event(Button("Connect"), LV_EVENT_CLICKED, nullptr);
    Advance(300);
    Check(Text("Networks") != nullptr, "Connect brings the Mac's networks");
    lv_obj_send_event(ControlOf(Shown("Studio-Upstairs-Guest-5G")), LV_EVENT_CLICKED, nullptr);
    Advance(300);
    Check(Text("Step 2 of 3") != nullptr, "a network picked is joined: step 2 of 3");
    Advance(3200);
    Check(Text("Wrong password") != nullptr, "the first join of the 5 GHz network is refused: wrong password");
    lv_obj_send_event(Button("Retry"), LV_EVENT_CLICKED, nullptr);
    Advance(300);
    Check(Text("Step 2 of 3") != nullptr, "Retry joins again");
    Advance(3200);
    Check(Text("Bridge paired") != nullptr, "and the retry pairs");
    lv_obj_send_event(Button("Done"), LV_EVENT_CLICKED, nullptr);
    Advance(300);
    Check(model::GetWifi().stage == model::WifiStage::kJoined && Shown("Studio-Upstairs-Guest-5G") != nullptr && Text("Disconnect") != nullptr,
          "Done shows the joined network and its facts");
    // Forgetting it, from the dialog.
    PressOn(ControlOf(Shown("Studio-Upstairs-Guest-5G")));
    router::Focus(Button("Forget"));
    Press(LV_KEY_ENTER);
    Advance(300);
    Check(model::GetWifi().stage == model::WifiStage::kNotJoined, "Forget leaves it: not connected");
    Home();
    mock::Stop();
}

void TestStandbyLook()
{
    std::printf("standby\n");
    SettingsState();
    mock::Stop();
    Home();
    Settle();
    DeckState();
    Settle();
    const home::Probe overview = home::Look();
    const int chip_y = Y(overview.chip);
    model::SetStandby(true);
    Dump("standby");  // before the first frame of the mascot's move: its rest pose
    Settle();
    home::Probe p = home::Look();
    Check(model::Standby() == model::StandbyPage::kStandby && p.pages == 1 && model::AgentCount() == 0 && !model::UsageKnown(),
          "the bridge's standby: the one page, the agents and the usage forgotten");
    Check(p.chip != nullptr && Text("On standby") != nullptr && Y(p.chip) == chip_y && Y(p.chip) == L::kChipTop,
          "the chip stands where the overview's and every page's stands");
    const lv_area_t chip = Area(p.chip);
    Check(std::abs((chip.x1 + chip.x2) / 2 - kW / 2) <= 1 && lv_area_get_height(&chip) == dt::kSpacingChipSmall, "centred, 36 high");
    lv_obj_t *words = Text("Another deck is active.");
    Check(words != nullptr && Is(words, ui::Type::kCaption), "the line under the mascot in caption");
    Check(std::abs((Area(words).x1 + Area(words).x2) / 2 - kW / 2) <= 1 && Y(words) == L::kFactsTop, "centred under the mascot's place");
    Check(lv_color_eq(lv_obj_get_style_text_color(words, LV_PART_MAIN), ui::Level(dt::kColorTextPrimary)), "in black");
    const lv_area_t m = Area(p.mascot);
    Check(m.x1 == L::kMascotX && m.y1 == L::kMascotY, "the mascot stands in the home view's place");
    lv_obj_t *page = p.page_object;
    Check(lv_obj_get_child(page, static_cast<int32_t>(lv_obj_get_child_count(page)) - 1) == p.mascot, "drawn in front of every element");
    lv_obj_t *connect = Button("Connect");
    Check(connect != nullptr && Y(connect) == L::kReviewTop && lv_obj_get_width(connect) == dt::kSpacingButtonWidth &&
              std::abs(X(connect) + lv_obj_get_width(connect) / 2 - kW / 2) <= 1,
          "Connect where Review stands, 212 wide, centred");
    Check(lv_obj_get_style_border_width(connect, LV_PART_MAIN) == dt::kSpacingStrokeDefault, "an outline button");
    Check(Ringed(Focused()) && Focused() == p.stop, "Connect is ringed as it appears: one OK connects");
    Check(p.dots == nullptr || lv_obj_has_flag(p.dots, LV_OBJ_FLAG_HIDDEN), "the foot has no page dots");
    Check(p.link != nullptr && p.wifi != nullptr && p.sound != nullptr && p.settings != nullptr && p.power != nullptr, "the foot keeps the link and the four icons");
    // The walk has one page: DOWN from the stop reaches the foot's Wi-Fi at once.
    lv_group_focus_next(s_group);
    Settle();
    Check(Focused() == home::Look().wifi, "DOWN from the page reaches the foot: the one page turns nothing");
    lv_group_focus_prev(s_group);
    Settle();
}

// OK connects: connecting, Connect disabled, the chip and the line say so; the link is asked.
void TestStandbyConnect()
{
    int connects = 0;
    model::SetConnectListener([]() {});
    (void)connects;
    router::Ring(home::Look().stop);
    Press(LV_KEY_ENTER);
    Settle();
    Check(model::Standby() == model::StandbyPage::kConnecting && Text("Connecting") != nullptr &&
              Text("Asking your Mac to make this deck active.") != nullptr,
          "OK on Connect: Connecting, and the line says what is waited for");
    Check(lv_obj_has_state(Button("Connect"), LV_STATE_DISABLED), "Connect is disabled while it waits");
    Check(Y(home::Look().chip) == L::kChipTop, "the chip has not moved");
    Dump("standby_connecting");
    // 15 s with no snapshot: standby again, the words saying the Mac did not answer, Connect live and ringed again.
    Advance(model::kConnectWaitMs + 5200);
    Check(model::Standby() == model::StandbyPage::kTimedOut && Text("Your Mac did not answer. Try again.") != nullptr && Text("On standby") != nullptr,
          "15 s on: standby again, the words say the Mac did not answer");
    Check(!lv_obj_has_state(Button("Connect"), LV_STATE_DISABLED), "Connect is live again");
    // A standby that comes while Connect waits is the bridge's refusal.
    model::ConnectPressed();
    Settle();
    model::SetStandby(true);
    Settle();
    Check(model::Standby() == model::StandbyPage::kRefused && Text("Another deck is on the cable.") != nullptr, "a standby while connecting: another deck is on the cable");
    // The snapshot's reset takes it off.
    model::Reset();
    Settle();
    Check(model::Standby() == model::StandbyPage::kNone && home::Look().pages >= 1 && Text("Connect") == nullptr, "the snapshot's reset takes the deck off standby");
}

// A request on view when standby comes closes with no page and no cue, and the screens leave to home.
void TestStandbyPage()
{
    TestStandbyLook();
    TestStandbyConnect();
    DeckState();
    router::PushRoute(flow::kRouteRequest, "1001");
    Settle();
    model::SetStandby(true);
    Advance(400);
    Check(router::Depth() == 1 && Text("Timed out") == nullptr && Text("On standby") != nullptr, "a request open leaves with no closed page: standby shows");
    model::Reset();
    Settle();
}

// --- The power button, the frame the glass keeps while off ---------------------------------------------------------------------------------

lv_obj_t *FindBySize(lv_obj_t *root, int w, int h)
{
    lv_area_t a;
    lv_obj_get_coords(root, &a);
    if (lv_area_get_width(&a) == w && lv_area_get_height(&a) == h) {
        return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (lv_obj_t *found = FindBySize(lv_obj_get_child(root, static_cast<int32_t>(i)), w, h)) {
            return found;
        }
    }
    return nullptr;
}

// The frame power_leave was given, built as the router builds the leaving screen.
void BuildLeaving(lv_obj_t *root, lv_group_t *, void *)
{
    power_leave::s_frame(root, power_leave::s_how);
}

void TestPower()
{
    std::printf("the power button and the frame the glass keeps\n");
    DeckState();
    const home::Probe p = home::Look();
    Check(p.power != nullptr && lv_obj_get_group(p.power) != nullptr, "Power is a stop in the foot's focus group");
    Check(std::string(widgets::IconButtonWords(p.power)) == "Turn off", "its name is Turn off");
    Check(lv_color_eq(lv_obj_get_style_bg_color(p.power, LV_PART_MAIN), lv_obj_get_style_bg_color(p.settings, LV_PART_MAIN)) &&
              lv_obj_get_style_radius(p.power, LV_PART_MAIN) == lv_obj_get_style_radius(p.settings, LV_PART_MAIN) &&
              lv_obj_get_style_bg_opa(p.power, LV_PART_MAIN) == lv_obj_get_style_bg_opa(p.settings, LV_PART_MAIN),
          "the same tonal icon button as Wi-Fi and Settings");
    Check(lv_obj_get_child_count(p.power) == 1 && lv_obj_check_type(lv_obj_get_child(p.power, 0), &lv_image_class), "its glyph alone, no word beside it");
    Dump("home_power");

    // Focused and ringed, it shows the ring; OK on it asks the leave — from a timer, not inside the click (the stub is the timer's ask).
    router::Ring(p.power);
    Settle();
    Check(Focused() == p.power && Ringed(p.power), "the walk rings Power");
    Dump("home_power_ringed");
    power_leave::s_asks = 0;
    Press(LV_KEY_ENTER);
    Settle();
    Check(power_leave::s_asks == 1 && power_leave::s_how == power_leave::How::kPowerOff && power_leave::s_frame == &agent_deck::BuildOffFrame,
          "OK on Power asks the power-off, once, with the deck's off frame and no confirmation");
    Check(router::Depth() == 1 && Text("Turn off") == nullptr, "no dialog or confirmation opens: the stack is still home");

    // The frame, built as the router builds the leaving screen over home.
    Check(router::Push(router::MakeScreen(BuildLeaving)) == ESP_OK, "the off frame builds");
    Settle();
    lv_obj_t *off = Text("Off");
    lv_obj_t *words = Text("Hold OK to turn on.");
    Check(off != nullptr && words != nullptr, "the frame says Off and how to turn on");
    lv_obj_t *chip = lv_obj_get_parent(off);
    const lv_area_t c = Area(chip);
    Check(c.y1 == L::kChipTop && lv_area_get_height(&c) == dt::kSpacingChipSmall && std::abs((c.x1 + c.x2) / 2 - kW / 2) <= 1,
          "the state chip stands where every page's chip stands, centred");
    Check(Is(words, ui::Type::kCaption) && Y(words) == L::kFactsTop && std::abs((Area(words).x1 + Area(words).x2) / 2 - kW / 2) <= 1,
          "the line in caption, centred under the mascot's place");
    Check(lv_color_eq(lv_obj_get_style_text_color(words, LV_PART_MAIN), ui::Level(dt::kColorTextPrimary)), "in black");
    lv_obj_t *mascot = FindBySize(lv_screen_active(), L::kMascotW, L::kMascotH);
    Check(mascot != nullptr && X(mascot) == L::kMascotX && Y(mascot) == L::kMascotY, "the mascot stands in the home view's place");
    lv_obj_t *page = lv_obj_get_parent(mascot);
    Check(lv_obj_get_child(page, static_cast<int32_t>(lv_obj_get_child_count(page)) - 1) == mascot, "in front of every element of its page");
    widgets::MascotMove move = widgets::MascotMove::kJump;
    uint32_t since = 0;
    widgets::Mascot rig{};
    rig.root = mascot;
    Check(!widgets::mascot_move_now(rig, &move, &since) || move == widgets::MascotMove::kRest, "at rest");
    Check(Text("Fri 7 Nov  2:32 PM") == nullptr, "no clock");
    Check(lv_group_get_obj_count(s_group) == 0 && FindBySize(lv_screen_active(), dt::kSpacingTarget, dt::kSpacingTarget) == nullptr,
          "no buttons, no foot, nothing to focus");
    Check(FindBySize(lv_screen_active(), kW, dt::kSpacingTarget) == nullptr && Text("Wi-Fi") == nullptr, "no foot row");
    Dump("off_frame");
    Home();
    Settle();
}

// --- The needs-you list's avatars play the deck's idle cycle ------------------------------------------------------------------------------------

// The avatars on the list now, top to bottom: the mascot roots, 10 x 8 cells of 6 px.
constexpr int32_t kAvatarHForTest = 48;  // waiting_screen.cpp's kAvatarH: 8 cells of 6 px

std::vector<lv_obj_t *> Avatars()
{
    std::vector<lv_obj_t *> found;
    std::function<void(lv_obj_t *)> walk = [&](lv_obj_t *o) {
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        if (lv_area_get_width(&a) == 60 && lv_area_get_height(&a) == 48) {
            found.push_back(o);
            return;
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
            walk(lv_obj_get_child(o, static_cast<int32_t>(i)));
        }
    };
    walk(lv_screen_active());
    std::sort(found.begin(), found.end(), [](lv_obj_t *x, lv_obj_t *y) { return Y(x) < Y(y); });
    return found;
}

// The list row an avatar stands over: the row in the walk whose area holds the avatar's slot.
lv_obj_t *RowUnder(lv_obj_t *avatar)
{
    const lv_area_t slot = Area(avatar);
    for (uint32_t i = 0; i < lv_group_get_obj_count(s_group); ++i) {
        lv_obj_t *o = lv_group_get_obj_by_index(s_group, i);
        const lv_area_t a = Area(o);
        if (slot.x1 >= a.x1 && slot.x2 <= a.x2 && slot.y1 >= a.y1 && slot.y2 <= a.y2) {
            return o;
        }
    }
    return nullptr;
}

bool Moving(lv_obj_t *root, uint32_t *since)
{
    widgets::Mascot m{};
    m.root = root;
    widgets::MascotMove move = widgets::MascotMove::kRest;
    if (!widgets::mascot_move_now(m, &move, since)) {
        return false;
    }
    return move != widgets::MascotMove::kRest;
}

struct Cycle {
    uint32_t since = 0;     // when the pick began it
    size_t avatar = 0;      // which row
    uint32_t first = 0, last = 0;  // when it was first and last seen playing
};

// Watches the list for `ms`, looking every 100 ms: each cycle that begins, which avatar it is, and how many play at once.
std::vector<Cycle> Watch(uint32_t ms, size_t *most)
{
    std::vector<Cycle> cycles;
    *most = 0;
    for (uint32_t t = 0; t < ms; t += 100) {
        Advance(100);
        const std::vector<lv_obj_t *> avatars = Avatars();
        size_t now_playing = 0;
        for (size_t i = 0; i < avatars.size(); ++i) {
            uint32_t since = 0;
            if (!Moving(avatars[i], &since)) {
                continue;
            }
            ++now_playing;
            Cycle *known = nullptr;
            for (Cycle &c : cycles) {
                known = c.since == since ? &c : known;
            }
            if (known == nullptr) {
                cycles.push_back({since, i, s_ms, s_ms});
            } else {
                known->last = s_ms;
            }
        }
        *most = std::max(*most, now_playing);
    }
    return cycles;
}

void FourWaiting()
{
    DeckState();
    model::SetAgent(Agent(4, 3, "Plan", model::AgentState::kNeedsYou, 2, "Edit: a", -1, 5000, "Plan"));
    model::SetAgent(Agent(5, 0, "docs-site", model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    model::SetAgent(Agent(6, 5, "Explore", model::AgentState::kNeedsYou, 2, "Bash", -1, 3000));
    Ask(1002, 5, "WebFetch", "https://docs.example.com/api");
    Ask(1003, 4, "Edit", "x");
    Ask(1004, 6, "Bash", "ls");
    Settle();
}

uint32_t TestAvatarsOpen()
{
    std::printf("the needs-you list's avatars\n");
    FourWaiting();
    model::SetSoundOn(true);
    const uint32_t t0 = s_ms;
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    std::vector<lv_obj_t *> avatars = Avatars();
    Check(avatars.size() == 4, "four avatars, 60 x 48, one a row");
    {
        // In front of everything on the page: on one layer, the page's last child, not inside a row that would clip it or a later row
        // that would draw over it.
        bool front = !avatars.empty();
        lv_obj_t *layer = avatars.empty() ? nullptr : lv_obj_get_parent(avatars[0]);
        for (lv_obj_t *a : avatars) {
            front = front && lv_obj_get_parent(a) == layer && RowUnder(a) != nullptr && lv_obj_get_parent(a) != RowUnder(a);
        }
        lv_obj_t *page = layer != nullptr ? lv_obj_get_parent(layer) : nullptr;
        front = front && page != nullptr && lv_obj_get_child(page, -1) == layer && lv_obj_has_flag(layer, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        Check(front, "the avatars stand on a layer over the whole page, never clipped by a row or drawn under one");
        const lv_area_t slot = Area(avatars[0]), row = Area(RowUnder(avatars[0]));
        Check(slot.x1 == row.x1 + dt::kSpacingMd && slot.y1 - row.y1 == (dt::kSpacingListRowHeight - kAvatarHForTest) / 2,
              "each over its row's slot: spacing.md in, centred on the row");
    }
    uint32_t since = 0;
    bool any = false;
    for (lv_obj_t *a : avatars) {
        any = any || Moving(a, &since);
    }
    Check(!any, "the list opens with every avatar at rest");
    Check(agent_deck::rules::kAvatarIdleEveryMs == 3000 && agent_deck::rules::kAvatarLookMs == 100 && widgets::kMascotIdleCycleMs == 5000 && widgets::kMascotLookCycleMs == 5600,
          "the deck's numbers: a pick every 3 s, the screen looking every 100 ms, a lean 5 s, a look-around 5.6 s");
    return t0;
}

void TestAvatarsCycles(uint32_t t0)
{
    const int moments_before = s_moments;
    size_t most = 0;
    const std::vector<Cycle> cycles = Watch(36000, &most);

    Check(s_moments > moments_before, "the cycles are heard: the mascot's moments reach the buzzer");
    Check(cycles.size() >= 8, "about one cycle a 3 s over 36 s: " + std::to_string(cycles.size()));
    // A look-around is begun part way along its plan, so a cycle's own `since` is not when it began: the first look at it is, within the 100 ms.
    Check(!cycles.empty() && cycles[0].first >= t0 + 3000 && cycles[0].first <= t0 + 3000 + 200, "the first begins 3 s after the list opens");
    bool spaced = true, never_same = true, lasts = true;
    for (size_t i = 0; i < cycles.size(); ++i) {
        if (i > 0) {
            const uint32_t gap = cycles[i].first - cycles[i - 1].first;
            spaced = spaced && gap >= 2900 && (gap + 100) % 3000 <= 200;
            never_same = never_same && cycles[i].avatar != cycles[i - 1].avatar;
        }
        const uint32_t played = cycles[i].last - cycles[i].first;
        // A lean 5 s or a look-around 5.6 s, then home within a settle; those begun in the last 6.4 s of the watch are cut off by its end.
        lasts = lasts && (cycles[i].first + 6400 > s_ms || (played >= 4800 && played <= 6300));
    }
    Check(spaced, "each cycle begins a whole number of 3 s after the one before (a pick on a moving avatar waits for the next)");
    Check(never_same, "never the avatar the one before picked, while there is another");
    Check(lasts, "each cycle runs its 5 s or 5.6 s and comes home, through the 5 s rebuilds of the list");
    Check(most == 2, "at most two play at once, and two do (a cycle is longer than the 3 s between picks): " + std::to_string(most));
}

// A covered list stops: its avatars home at once, nothing begun, nothing heard; back, it begins again from 3 s.
void TestAvatarsCovered()
{
    uint32_t since = 0;
    size_t most = 0;
    std::vector<lv_obj_t *> avatars = Avatars();
    Advance(3000);
    lv_obj_send_event(ControlOf(Text("web-shop")), LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 3 && Text("Deny") != nullptr, "a row opens its request over the list");
    bool still = false;
    for (lv_obj_t *a : avatars) {
        still = still || Moving(a, &since);
    }
    Check(!still, "the covered list's avatars are at rest");
    const int covered_moments = s_moments;
    Advance(9000);
    Check(s_moments == covered_moments, "and nothing is begun or heard while it is covered");
    router::Pop();
    Settle();
    const uint32_t back = s_ms;
    avatars = Avatars();
    bool any = false;
    for (lv_obj_t *a : avatars) {
        any = any || Moving(a, &since);
    }
    Check(router::Depth() == 2 && !any, "uncovered, the list is at rest again");
    const std::vector<Cycle> again = Watch(3300, &most);
    Check(!again.empty() && again[0].first >= back + 3000 - 200 && again[0].first <= back + 3000 + 200, "and begins a cycle 3 s after, as when it opened");
}

// Sound off: the same cycles, silent.
void TestAvatarsQuiet()
{
    size_t most = 0;
    Home();
    Settle();
    model::SetSoundOn(false);
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    const int silent = s_moments;
    const std::vector<Cycle> mute = Watch(14000, &most);
    Check(mute.size() >= 3 && s_moments == silent, "with the sound off the avatars still move and are not heard");
    model::SetSoundOn(true);
    Home();
    Settle();

    // One alone on the list is not begun again part way: its cycles are 6 s apart, 3 s picks and one skipped.
    DeckState();
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    Check(Avatars().size() == 1, "one waiting: one avatar");
    const std::vector<Cycle> lone = Watch(26000, &most);
    bool six = lone.size() >= 3;
    for (size_t i = 1; i < lone.size(); ++i) {
        six = six && lone[i].first - lone[i - 1].first >= 5900 && lone[i].first - lone[i - 1].first <= 6100;
    }
    Check(six && most == 1, "a lone avatar plays its cycle through, then rests, and the pick after is 6 s on: " + std::to_string(lone.size()) + " cycles");
    Home();
    Settle();
}

void TestAvatars()
{
    TestAvatarsCycles(TestAvatarsOpen());
    TestAvatarsCovered();
    TestAvatarsQuiet();
}

// The box round the pixels two frames of the glass differ in; x2 is -1 when none does.
lv_area_t ChangedBox(const std::vector<uint8_t> &was, const std::vector<uint8_t> &now)
{
    lv_area_t box = {kW, kH, -1, -1};
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            if (now[y * kW + x] != was[y * kW + x]) {
                box.x1 = std::min(box.x1, x);
                box.y1 = std::min(box.y1, y);
                box.x2 = std::max(box.x2, x);
                box.y2 = std::max(box.y2, y);
            }
        }
    }
    return box;
}

// One cycle of an avatar as the glass shows it: the frames it changed, from its first frame to the one that shows it home, and their box.
struct AvatarRun {
    int frames = 0;
    uint32_t from = 0, to = 0;
    int x1 = kW, y1 = kH, x2 = -1, y2 = -1;
};

// Says each whole cycle's frames and box at `pace`, and the frames that changed pixels outside `allowed`.
void ReportAvatarRuns(uint32_t pace, const std::vector<AvatarRun> &runs, int outside, const lv_area_t &allowed)
{
    for (const AvatarRun &r : runs) {
        if (r.to + 7000 > s_ms && &r == &runs.back()) {
            continue;  // cut by the end of the watch
        }
        std::printf("  at %u ms a frame: a cycle of %u ms changes the glass %d frames, in a box %d x %d (slot 60 x 48)\n", static_cast<unsigned>(pace),
                    static_cast<unsigned>(r.to - r.from), r.frames, r.x2 - r.x1 + 1, r.y2 - r.y1 + 1);
        Check(r.frames > 0 && r.x1 >= allowed.x1 && r.x2 <= allowed.x2, "a cycle's frames change the avatar's box alone");
    }
    std::printf("  at %u ms a frame: %d frames in 30 s changed pixels beyond the avatar's row (the waits counting on, every 5 s)\n",
                static_cast<unsigned>(pace), outside);
    Check(outside <= 7, "nothing but the avatar and the waits' count changes the glass");
}

// Watches one avatar's list for 30 s at `pace`, a frame of the glass each look, and reports what each cycle changed.
void WatchAvatarRefresh(uint32_t pace, const lv_area_t &allowed)
{
    Home();
    Settle();
    DeckState();
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    std::vector<uint8_t> was = Glass();
    int outside = 0;
    bool prev_moving = false;
    std::vector<AvatarRun> runs;
    for (uint32_t t = 0; t < 30000; t += pace) {
        Advance(pace);
        uint32_t since = 0;
        const bool moving = Moving(Avatars()[0], &since);
        const std::vector<uint8_t> now = Glass();
        const lv_area_t changed = ChangedBox(was, now);
        const int cx1 = changed.x1, cy1 = changed.y1, cx2 = changed.x2, cy2 = changed.y2;
        was = now;
        if (moving && !prev_moving) {
            runs.push_back({});
            runs.back().from = s_ms;
        }
        if (!runs.empty() && (moving || prev_moving)) {
            runs.back().to = s_ms;
        }
        if (cx2 >= 0) {
            if (cx1 >= allowed.x1 && cx2 <= allowed.x2 && cy1 >= allowed.y1 && cy2 <= allowed.y2) {
                if (!runs.empty() && (moving || prev_moving)) {
                    AvatarRun &r = runs.back();
                    ++r.frames;
                    r.x1 = std::min(r.x1, cx1);
                    r.y1 = std::min(r.y1, cy1);
                    r.x2 = std::max(r.x2, cx2);
                    r.y2 = std::max(r.y2, cy2);
                }
            } else {
                ++outside;
            }
        }
        prev_moving = moving;
    }
    ReportAvatarRuns(pace, runs, outside, allowed);
}

// What a cycle costs the glass, on the host's render: the frames the avatar's box changes at the glass's pace, and the box. One avatar plays
// (the list holds one agent), watched at the V2.0's 85 Hz partial, about 408 ms, and the V1.0's default, about 543 ms (1.84 fps); each cycle
// from its first frame to the one that shows it home.
void TestAvatarRefresh()
{
    std::printf("the avatars' cycle against the glass\n");
    DeckState();
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    const std::vector<lv_obj_t *> avatars = Avatars();
    Check(avatars.size() == 1, "one avatar");
    const lv_area_t slot = Area(avatars[0]);
    const lv_area_t row_area = Area(RowUnder(avatars[0]));
    // The only pixels a frame may change, besides the waits' counting on every 5 s: the avatar's slot and the row's room round it.
    const lv_area_t allowed = {row_area.x1, row_area.y1, slot.x2 + dt::kSpacingMd, row_area.y2};
    for (const uint32_t pace : {400u, 540u}) {
        WatchAvatarRefresh(pace, allowed);
    }
    Home();
    Settle();
}

// DECK_DUMP only: the list through 12 s of its cycle, a frame every 400 ms — about the glass's pace — for the person's look at it.
void DumpAvatarCycle()
{
    if (std::getenv("DECK_DUMP") == nullptr) {
        return;
    }
    FourWaiting();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    Dump("waiting_cycle_00");
    for (int i = 1; i < 31; ++i) {
        Advance(400);
        Dump(std::string("waiting_cycle_") + (i < 10 ? "0" : "") + std::to_string(i));
    }
    Home();
    Settle();
}

// --- Stepped lines ---------------------------------------------------------------------------------------------------------------------

// Whether a line's label holds `words` at its start: the words themselves, or — a line standing after its passes — the words' first
// characters with LVGL's three full stops in place of the rest.
bool HoldsWords(const std::string &held, const std::string &words)
{
    if (held.rfind(words, 0) == 0) {
        return true;
    }
    const std::string stops = "...";
    return held.size() > stops.size() && held.compare(held.size() - stops.size(), stops.size(), stops) == 0 &&
           words.rfind(held.substr(0, held.size() - stops.size()), 0) == 0;
}

// The stepped line whose words begin `words`, under `root`: the box widgets::MakeMarquee made (its label, in its window, holds the words
// once, twice with a gap when they step, or once ending in dots when they stand after their passes).
lv_obj_t *FindLine(lv_obj_t *root, const std::string &words)
{
    if (widgets::ProbeMarquee(root).valid && lv_obj_get_child_count(root) > 0) {
        lv_obj_t *label = lv_obj_get_child(lv_obj_get_child(root, 0), 0);
        if (HoldsWords(lv_label_get_text(label), words)) {
            return root;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (lv_obj_t *found = FindLine(lv_obj_get_child(root, static_cast<int32_t>(i)), words)) {
            return found;
        }
    }
    return nullptr;
}
lv_obj_t *Line(const std::string &words) { return FindLine(lv_screen_active(), words); }

// A label on the glass that ends in LVGL's three full stops when too long: none stands on a deck screen any more but the kit's network row,
// the value row and the status line, which these screens do not show.
int Dotted(lv_obj_t *root)
{
    int n = lv_obj_check_type(root, &lv_label_class) && lv_label_get_long_mode(root) == LV_LABEL_LONG_MODE_DOTS ? 1 : 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        n += Dotted(lv_obj_get_child(root, static_cast<int32_t>(i)));
    }
    return n;
}

constexpr const char *kLongName = "agent-deck-studio-with-a-very-long-fold";  // 39 characters of title: 700 px
constexpr const char *kLongDoing = "Edit: components/agent_deck/home_screen.cpp and its host tests";  // 59 characters of caption: 700 px
constexpr const char *kLongTool = "mcp__plugin_figma_figma__get_design_context";  // 43 characters of title, and of caption in a row's facts

// How many times a line's offset changed over `ms`, finding the line again each look by its first words (a screen built again makes a new one).
int StepsSeen(const std::string &words, uint32_t ms)
{
    int steps = 0;
    lv_obj_t *line = Line(words);
    int32_t last = line != nullptr ? widgets::ProbeMarquee(line).offset : 0;
    for (uint32_t t = 0; t < ms; t += 20) {
        Advance(20);
        line = Line(words);
        const int32_t now = line != nullptr ? widgets::ProbeMarquee(line).offset : last;
        steps += now != last;
        last = now;
    }
    return steps;
}

// The stepped line whose words begin `words` has made its passes and stands at its start.
bool Standing(const std::string &words)
{
    lv_obj_t *line = Line(words);
    return line != nullptr && widgets::ProbeMarquee(line).stopped && widgets::ProbeMarquee(line).offset == 0;
}

// The line has begun again: not stopped, at rest with its start in view.
bool Begun(const std::string &words)
{
    lv_obj_t *line = Line(words);
    return line != nullptr && !widgets::ProbeMarquee(line).stopped && widgets::ProbeMarquee(line).resting;
}

// The label of the line `words`, in its window.
lv_obj_t *LabelOf(const std::string &words)
{
    lv_obj_t *line = Line(words);
    return line != nullptr ? lv_obj_get_child(lv_obj_get_child(line, 0), 0) : nullptr;
}

// The line stands after its passes in LVGL's dots: the words once, cut where the room ends with three full stops in place of the rest,
// the label the room's width and inside the box, as the dotted label it replaces.
bool EndsInDots(const std::string &words)
{
    lv_obj_t *line = Line(words), *label = LabelOf(words);
    if (line == nullptr || label == nullptr) {
        return false;
    }
    const std::string held = lv_label_get_text(label);
    const lv_area_t box = Area(line), at = Area(label);
    return widgets::ProbeMarquee(line).stopped && lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_DOTS && held.size() < words.size() &&
           held.compare(held.size() - 3, 3, "...") == 0 && lv_area_get_width(&at) == lv_area_get_width(&box) && at.x1 == box.x1 && at.x2 <= box.x2;
}

// The line steps: its label holds the words twice, a gap between, as wide as they are.
bool HoldsTwice(const std::string &words)
{
    lv_obj_t *label = LabelOf(words);
    return label != nullptr && lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_WRAP && std::string(lv_label_get_text(label)).find(words, words.size()) != std::string::npos;
}

// Long enough for the line `words` to rest, make its passes and rest a second more.
uint32_t UntilStanding(const std::string &words)
{
    lv_obj_t *line = Line(words);
    return 4000 + (line != nullptr ? widgets::ProbeMarquee(line).steps : 0) * 1000 + 2000;
}

void TestSteppedSession()
{
    std::printf("stepped lines\n");
    // A session's page: a long name over the chip and a long doing line.
    DeckState();
    model::SetAgent(Agent(1, 0, kLongName, model::AgentState::kWorking, 252, kLongDoing, 63, 126000));
    Home();
    Settle();
    lv_group_focus_next(s_group);
    Settle();
    home::Probe p = home::Look();
    Check(p.page == 1, "DOWN turns to the long-named session's page");
    lv_obj_t *name = Line(kLongName), *doing = Line(kLongDoing);
    Check(name != nullptr && doing != nullptr && widgets::ProbeMarquee(name).stepping && widgets::ProbeMarquee(doing).stepping,
          "a name and a doing line too long for their room step");
    Check(Dotted(lv_screen_active()) == 0, "and the page has no line that ends in dots");
    if (name != nullptr && doing != nullptr) {
        const lv_area_t n = Area(name), c = Area(p.chip);
        Check(n.x1 == dt::kSpacingEdge && lv_area_get_width(&n) == dt::kSpacingContent && n.y2 + 1 + dt::kSpacingSm == c.y1,
              "the name's room is the content, 8 px over the chip, where the short name stood");
        const lv_area_t d = Area(doing);
        Check(d.x1 == dt::kSpacingEdge && lv_area_get_width(&d) == dt::kSpacingContent, "the doing line's room is the content");
        Check(widgets::ProbeMarquee(name).resting && widgets::ProbeMarquee(doing).resting, "both rest with their start in view");
        Advance(4000 + 3000);
        name = Line(kLongName);
        doing = Line(kLongDoing);
        const int32_t n_at = widgets::ProbeMarquee(name).offset, d_at = widgets::ProbeMarquee(doing).offset;
        Check(n_at != 0 && d_at != 0, "after the rest they step: the name at " + std::to_string(n_at) + " px, the doing line at " + std::to_string(d_at));
        // A record on another agent builds the page again: a pass under way goes on.
        model::SetAgent(Agent(3, 0, "web-shop", model::AgentState::kWorking, 12, "Bash: npm run deploy", 33, 66000));
        name = Line(kLongName);
        doing = Line(kLongDoing);
        Check(name != nullptr && doing != nullptr && widgets::ProbeMarquee(name).offset == n_at && widgets::ProbeMarquee(doing).offset == d_at,
              "a record that changed another agent builds the page again and leaves the passes under way");
        // New words on this agent's own line start from their start.
        model::SetAgent(Agent(1, 0, kLongName, model::AgentState::kWorking, 252, "Bash: cd web-shop && npm run build && npm run deploy -- --prod", 63, 126000));
        doing = Line("Bash: cd web-shop");
        Check(doing != nullptr && widgets::ProbeMarquee(doing).offset == 0 && widgets::ProbeMarquee(doing).resting, "a new doing line starts from its start");
        name = Line(kLongName);
        Check(name != nullptr && widgets::ProbeMarquee(name).offset == n_at, "and the name, which did not change, goes on");
    }
    // A short name stands still and centred, as it did.
    model::SetAgent(Agent(1, 0, "agent-deck-studio", model::AgentState::kWorking, 252, "Bash: npm test", 63, 126000));
    Settle();
    lv_obj_t *short_name = Line("agent-deck-studio");
    Check(short_name != nullptr && !widgets::ProbeMarquee(short_name).stepping, "a name that fits stands still");
    lv_obj_t *label = lv_obj_get_child(lv_obj_get_child(short_name, 0), 0);
    Check(lv_obj_get_style_text_align(label, LV_PART_MAIN) == LV_TEXT_ALIGN_CENTER && std::abs((Area(label).x1 + Area(label).x2) / 2 - kW / 2) <= 1,
          "and centred on the glass");
}

// A sub-agent's page: "in <session>" steps too.
void TestSteppedSubagentAndList()
{
    model::SetAgent(Agent(1, 0, kLongName, model::AgentState::kWorking, 252, "Bash: npm test", 63, 126000));
    model::SetAgent(Agent(2, 1, "Explore", model::AgentState::kWorking, 31, "Grep: kSafeSquare", -1, 18400));
    Settle();
    lv_group_focus_next(s_group);
    Settle();
    lv_obj_t *in = Line(std::string("in ") + kLongName);
    Check(home::Look().page == 2 && in != nullptr && widgets::ProbeMarquee(in).stepping, "a sub-agent's \"in <session>\" steps when the session's name is long");
    Check(Dotted(lv_screen_active()) == 0, "and its page has no dots either");

    // The needs-you list: names and facts too long step together, rest at their start after their passes, and go through the list's 5 s rebuilds.
    DeckState();
    model::SetAgent(Agent(5, 0, kLongName, model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    Ask(1002, 5, kLongTool, "u");
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    lv_obj_t *row_name = Line(kLongName), *row_facts = Line(kLongTool);
    Check(row_name != nullptr && row_facts != nullptr && widgets::ProbeMarquee(row_name).stepping && widgets::ProbeMarquee(row_facts).stepping,
          "a row's long name and long facts step");
    Check(ControlOf(row_name) != nullptr && ControlOf(row_name) == ControlOf(row_facts), "both stand in the row, a control");
    Check(Dotted(lv_screen_active()) == 0, "and the list has no line that ends in dots");
    Check(lv_obj_get_height(ControlOf(row_name)) == dt::kSpacingListRowHeight, "the row is 80 px");
    Check(Text("web-shop") != nullptr && !widgets::ProbeMarquee(Line("web-shop") != nullptr ? Line("web-shop") : row_name).stepping,
          "a short name beside them stands still");
    Advance(4000 - 120 + 600);
    const int moved = StepsSeen(kLongName, 30 * 1000);
    Check(moved >= 28, "through six of the list's 5 s rebuilds the name steps on every second: " + std::to_string(moved) + " steps in 30 s");
    const int counted = StepsSeen(kLongTool, 10 * 1000);
    Check(counted >= 8, "and the facts, whose words count the wait and change each rebuild, go on too: " + std::to_string(counted) + " steps in 10 s");
    Advance(40 * 1000);
    row_name = Line(kLongName);
    row_facts = Line(kLongTool);
    Check(row_name != nullptr && row_facts != nullptr && widgets::ProbeMarquee(row_name).stopped && widgets::ProbeMarquee(row_name).offset == 0,
          "after its three passes the name rests at its start: a row is a control");
    const int quiet = StepsSeen(kLongName, 12 * 1000);
    Check(quiet == 0, "and a rebuild does not start it again");
    Check(EndsInDots(kLongName) && EndsInDots(kLongTool), "a row's stopped name and facts end in dots too");
    // Focus leaves the row and comes back: the lines hold their words twice again and step.
    lv_obj_t *row = ControlOf(Line(kLongName));
    lv_group_focus_obj(Button("Dismiss"));
    Advance(1100);
    lv_group_focus_obj(row);
    Advance(1100);
    Check(Begun(kLongName) && HoldsTwice(kLongName), "focus arriving on the row begins its name again, the dots gone for the words twice");
    Check(!EndsInDots(kLongTool), "and its facts, which count the wait, no longer end in dots");
}

// A request's tool, as long.
void TestSteppedRequestAndWifi()
{
    DeckState();
    model::CloseRequest(1001, model::CloseReason::kTimedOut);
    Ask(2001, 3, kLongTool, "body");
    Settle();
    Home();
    Settle();
    Press(LV_KEY_ENTER);
    Check(router::Depth() == 2 && Line(kLongTool) != nullptr, "a request opens");
    lv_obj_t *tool = Line(kLongTool);
    Check(tool != nullptr && widgets::ProbeMarquee(tool).stepping && X(tool) == dt::kSpacingEdge && Y(tool) == L::kHeadingTop,
          "its long tool steps, in title, at the heading's place");
    Check(Dotted(lv_screen_active()) == 0, "and the page has no line that ends in dots");
    Home();
    Settle();

    // The Wi-Fi page joining a network with a long name.
    SettingsState();
    Home();
    Settle();
    lv_obj_send_event(home::Look().wifi, LV_EVENT_CLICKED, nullptr);
    Settle();
    model::SetWifi(WifiRecord(model::WifiStage::kJoining, "Studio-Upstairs-Guest-5G-Annex-2"));
    Settle();
    lv_obj_t *joining = Line("Joining Studio-Upstairs");
    Check(joining != nullptr && widgets::ProbeMarquee(joining).stepping, "a joining network's long name steps");
    Check(Dotted(lv_screen_active()) == 0, "and the joining page has no line that ends in dots");
    model::SetWifi(WifiRecord(model::WifiStage::kJoining));
    Settle();
    Check(Text("Joining Studio") != nullptr && !widgets::ProbeMarquee(Line("Joining Studio")).stepping, "a short one stands still");
    // The joined network's row on the Wi-Fi page, and the settings' network row: a name of 32 characters steps in the row's room.
    model::SetWifi(WifiRecord(model::WifiStage::kJoined, "Studio-Upstairs-Guest-5G-Annex-2"));
    Settle();
    lv_obj_t *net = Line("Studio-Upstairs-Guest-5G-Annex-2");
    Check(net != nullptr && widgets::ProbeMarquee(net).stepping && ControlOf(net) != nullptr, "the joined network's long name steps, in a row that is a control");
    Home();
    Settle();
    lv_obj_send_event(home::Look().settings, LV_EVENT_CLICKED, nullptr);
    Settle();
    Check(router::Depth() == 2 && Text("Settings") != nullptr, "the foot's Settings icon opens the settings");
    lv_obj_t *settings_net = Line("Studio-Upstairs-Guest-5G-Annex-2");
    Check(settings_net != nullptr && widgets::ProbeMarquee(settings_net).stepping, "the settings' network row steps a long name too");
    Home();
    Settle();
}

// A session's name and doing line are readings, in no control. After their three passes they stand at their start, however long, through
// the page's rebuilds; a new doing line begins again, and the name that did not change stands on.
void TestSteppedReadingsStop()
{
    std::printf("stepped readings stop after their passes\n");
    DeckState();
    model::SetAgent(Agent(1, 0, kLongName, model::AgentState::kWorking, 252, kLongDoing, 63, 126000));
    model::SetAgent(Agent(3, 0, "web-shop", model::AgentState::kWorking, 12, "Bash: npm run deploy", 33, 66000));
    Home();
    Settle();
    lv_group_focus_next(s_group);
    Settle();
    Check(home::Look().page == 1 && Line(kLongName) != nullptr && Line(kLongDoing) != nullptr, "DOWN turns to the long-named session's page");
    Check(ControlOf(Line(kLongName)) == nullptr && ControlOf(Line(kLongDoing)) == nullptr, "its name and doing line are readings, in no control");
    Advance(std::max(UntilStanding(kLongName), UntilStanding(kLongDoing)));
    Check(Standing(kLongName) && Standing(kLongDoing), "after three passes the name and the doing line stand at their start");
    Check(EndsInDots(kLongName) && EndsInDots(kLongDoing), "each ending in dots, never cut mid-word with no mark");
    Dump("session_standing");
    const int moved = StepsSeen(kLongName, 12 * 1000) + StepsSeen(kLongDoing, 12 * 1000);
    Check(moved == 0, "and stand there through the page's rebuilds, however long: " + std::to_string(moved) + " steps in 24 s");
    model::SetAgent(Agent(3, 0, "web-shop", model::AgentState::kWorking, 12, "Bash: npm run test", 33, 66000));
    Check(Standing(kLongName) && Standing(kLongDoing), "a record on another agent builds the page again and leaves them standing");
    const char *fresh = "Bash: cd web-shop && npm run build && npm run deploy -- --prod";
    model::SetAgent(Agent(1, 0, kLongName, model::AgentState::kWorking, 252, fresh, 63, 126000));
    Check(Begun("Bash: cd web-shop") && Standing(kLongName), "new words on the doing line begin it again, resting first, and the name stands on");
    Check(HoldsTwice(fresh) && EndsInDots(kLongName), "the new line holds its words twice to step, the name still ends in dots");
    const int again = StepsSeen("Bash: cd web-shop", 4000 + 3000);
    Check(again >= 1 && StepsSeen("Bash: cd web-shop", UntilStanding("Bash: cd web-shop")) >= 1, "it steps after its rest: " + std::to_string(again));
    Check(Standing("Bash: cd web-shop") && EndsInDots(fresh), "and stands again after its passes, in dots");
}

// The page turned to, and the screen entered, begin the readings again.
void TestSteppedReadingsBegin()
{
    std::printf("stepped readings begin again when their page is turned to\n");
    DeckState();
    model::SetAgent(Agent(1, 0, kLongName, model::AgentState::kWorking, 252, kLongDoing, 63, 126000));
    model::SetAgent(Agent(3, 0, "web-shop", model::AgentState::kWorking, 12, "Bash: npm run deploy", 33, 66000));
    Home();
    Settle();
    lv_group_focus_next(s_group);
    Settle();
    Advance(std::max(UntilStanding(kLongName), UntilStanding(kLongDoing)));
    Check(Standing(kLongName) && Standing(kLongDoing), "the long-named session's page stands at rest");
    // A carousel page turned: to the next page and back.
    lv_group_focus_next(s_group);
    Settle();
    Check(home::Look().page == 2, "DOWN turns to the next page");
    lv_group_focus_prev(s_group);
    Settle();
    Check(home::Look().page == 1 && Begun(kLongName) && Begun(kLongDoing) && HoldsTwice(kLongName) && HoldsTwice(kLongDoing),
          "turned back to, the page's name and doing line begin again at rest, holding their words twice");
    Advance(std::max(UntilStanding(kLongName), UntilStanding(kLongDoing)));
    Check(Standing(kLongName) && Standing(kLongDoing), "and stand again after their passes");
    // A screen entered: the settings over home, and back.
    lv_obj_send_event(home::Look().settings, LV_EVENT_CLICKED, nullptr);
    Settle();
    Advance(2000);
    Home();
    Settle();
    Advance(1000);  // the stepped lines' clock sees home shown again at its next step
    Check(Begun(kLongName) && Begun(kLongDoing) && HoldsTwice(kLongName), "home entered again begins the lines again, holding their words twice");
    Home();
}

void TestSteppedLines()
{
    TestSteppedSession();
    TestSteppedSubagentAndList();
    TestSteppedRequestAndWifi();
    TestSteppedReadingsStop();
    TestSteppedReadingsBegin();
}

// DECK_DUMP only: a session's page with a long name and a long doing line through 27 s — a frame at the start and at every step either line
// makes, with how long each held (`mq_session_frames.txt`) — and the needs-you list with long names and facts at rest and mid-step, for the
// person's look at them beside the draft's.
void DumpSteppedLines()
{
    const char *dir = std::getenv("DECK_DUMP");
    if (dir == nullptr) {
        return;
    }
    DeckState();
    model::SetAgent(Agent(1, 0, "agent-deck-studio-design-system", model::AgentState::kWorking, 252,
                          "Bash: cd web-shop && npm run build && npm run deploy -- --prod", 63, 126000));
    Home();
    Settle();
    lv_group_focus_next(s_group);
    Settle();
    const std::string name = "agent-deck-studio-design-system", doing = "Bash: cd web-shop && npm run build && npm run deploy -- --prod";
    FILE *holds = std::fopen((std::string(dir) + "/mq_session_frames.txt").c_str(), "w");
    int frame = 0;
    uint32_t last_at = 0, t = 0;
    int32_t last_n = -1, last_d = -1;
    while (t <= 27000) {
        lv_obj_t *n = Line(name), *d = Line(doing);
        const int32_t on = n != nullptr ? widgets::ProbeMarquee(n).offset : 0, od = d != nullptr ? widgets::ProbeMarquee(d).offset : 0;
        if (on != last_n || od != last_d) {
            if (frame > 0 && holds != nullptr) {
                std::fprintf(holds, "%u\n", t - last_at);
            }
            Dump(std::string("mq_session_") + (frame < 10 ? "00" : frame < 100 ? "0" : "") + std::to_string(frame));
            ++frame;
            last_at = t;
            last_n = on;
            last_d = od;
        }
        Advance(20);
        t += 20;
    }
    if (holds != nullptr) {
        std::fprintf(holds, "%u\n", t - last_at);
        std::fclose(holds);
    }

    DeckState();
    model::SetAgent(Agent(5, 0, "docs-site-with-a-very-long-folder-name", model::AgentState::kNeedsYou, 2, "WebFetch", 14, 27000));
    model::SetAgent(Agent(6, 5, "Explore the whole repository", model::AgentState::kNeedsYou, 2, "Bash", -1, 3000, "Explore"));
    Ask(1002, 5, kLongTool, "u");
    Ask(1004, 6, "Bash", "ls");
    Settle();
    router::PushRoute(flow::kRouteWaiting);
    Settle();
    Dump("mq_list_rest");
    Advance(4000 + 3000);
    Dump("mq_list_mid");
    Home();
    Settle();
}

// --- The screens' own headers: the factories, the closed page and the clock by themselves, the mascot's voice --------------

// What a route makes its screen with, or nullptr for a route nobody registered.
router::RouteFactory MakerOf(const char *id)
{
    const router::Route *route = router::FindRoute(id);
    return route != nullptr ? route->make : nullptr;
}

// Each deck route makes its screen with the factory its screen's header declares, and a factory refuses what names nothing waiting.
void TestFactories()
{
    std::printf("the screens' factories\n");
    DeckState();
    model::Question q{};
    q.id = 2001;
    q.agent = 1;
    std::snprintf(q.tool, sizeof q.tool, "Branch");
    q.question = "Which base branch?";
    q.options[0] = "main: the release branch";
    q.options[1] = "develop: where the work lands";
    q.option_count = 2;
    model::OpenQuestion(q);
    Check(MakerOf(flow::kRouteRequest) == &agent_deck::RequestScreen && MakerOf(flow::kRouteQuestion) == &agent_deck::QuestionScreen,
          "the request's and the question's routes make their screens with answer_screens.h's factories");
    Check(MakerOf(flow::kRouteWaiting) == &router::NoParam<agent_deck::WaitingScreen> &&
              MakerOf(flow::kRouteSettings) == &router::NoParam<agent_deck::SettingsScreen> &&
              MakerOf(flow::kRouteWifi) == &router::NoParam<agent_deck::WifiScreen>,
          "the needs-you list's, the settings' and the Wi-Fi page's routes take no param and make their headers' screens");
    Check(agent_deck::RequestScreen("1001") != nullptr && agent_deck::QuestionScreen("1001") == nullptr,
          "a request waiting makes a request's screen, never a question's");
    Check(agent_deck::QuestionScreen("2001") != nullptr && agent_deck::RequestScreen("2001") == nullptr,
          "a question waiting makes a question's screen, never a request's");
    Check(agent_deck::RequestScreen("777") == nullptr && agent_deck::QuestionScreen("x") == nullptr, "a param that names nothing waiting makes nothing");
    Check(agent_deck::WaitingScreen() != nullptr && agent_deck::SettingsScreen() != nullptr && agent_deck::WifiScreen() != nullptr &&
              agent_deck::ClosedScreen(true) != nullptr && agent_deck::ClosedScreen(false) != nullptr,
          "the list, the settings, the Wi-Fi page and the closed page each make a screen");
    DeckState();
    Settle();
}

void OnClosedLeft(void *user) { ++*static_cast<int *>(user); }

// The closed page built into a container of its own, as a request screen builds it, and the clock every screen carries.
void TestClosedPageAndClock()
{
    std::printf("the closed page and the clock, by themselves\n");
    lv_obj_t *parent = lv_obj_create(nullptr);  // a screen object of its own, never loaded
    int left = 0;
    {
        agent_deck::ClosedPage page;
        page.Build(parent, true, OnClosedLeft, &left);
        Check(page.Answered() && page.Root() != nullptr && lv_obj_get_parent(page.Root()) == parent, "the page is built into its parent, answered");
        Check(page.HoldMs() == dt::kMotionAnsweredHoldDelayMs, "an answer holds the page motion.answered-hold");
        page.Start();
        Advance(page.HoldMs() - 200);
        Check(left == 0, "the page stays through its hold");
        Advance(400);
        Check(left == 1, "and leaves once, as its hold ends");
    }
    {
        agent_deck::ClosedPage page;
        page.Build(parent, false, OnClosedLeft, &left);
        Check(!page.Answered() && page.HoldMs() == dt::kMotionTimedOutHoldDelayMs, "a request that timed out holds it motion.timed-out-hold");
    }
    lv_obj_clean(parent);
    agent_deck::Clock clock;
    clock.Build(parent);
    clock.Update();
    Check(Said(clock.Label()) == "Fri 7 Nov  2:32 PM" && Is(clock.Label(), ui::Type::kCaption), "the clock reads the RTC, in caption: Fri 7 Nov  2:32 PM");
    lv_obj_delete(parent);
}

// A page's voice by itself: on until turned off, its moments to the buzzer's queue, and one blink clock heard once.
void TestVoice()
{
    std::printf("the mascot's voice, by itself\n");
    mascot_voice::Voice voice;
    Check(voice.On(), "a voice is on until it is turned off");
    const int before = s_moments;
    voice.Tell(widgets::MascotSfx::kHop);
    Check(s_moments == before + 1, "a moment told goes to the buzzer's queue");
    voice.SetOn(false);
    voice.Tell(widgets::MascotSfx::kHop);
    Check(!voice.On() && s_moments == before + 1, "turned off, it passes on nothing it is told");
    voice.SetOn(true);
    lv_obj_t *parent = lv_obj_create(nullptr);
    voice.Attach(widgets::make_mascot(parent, dt::kSpacingMascotCell / 4), 500);
    voice.Tell(widgets::MascotSfx::kBlink);
    voice.Tell(widgets::MascotSfx::kBlink);
    Check(s_moments == before + 2, "mascots on one blink clock: two blinks told within its time are heard once");
    lv_obj_delete(parent);
}

// The deck's event sounds, from the model wherever the glass is (STICKY_AGENT_DECK §6): the link up, an agent starting, the plan's limit,
// a request queued — and a lower one never cutting a higher one still sounding.
void TestEventSounds()
{
    using devices::sound::Word;
    Home();
    model::Reset();
    model::SetLink(false);
    Advance(1000);
    s_words.clear();
    model::SetLink(true);
    Check(s_words == std::vector<Word>{Word::kLinkUp}, "the link come up sounds link up");
    Advance(400);
    model::SetAgent(Agent(7, 0, "sound-check", model::AgentState::kWorking, 0, "Bash: ls", 10, 100));
    Check(s_words.size() == 2 && s_words[1] == Word::kAgentStart, "a new agent working sounds agent start");
    model::SetAgent(Agent(7, 0, "sound-check", model::AgentState::kWorking, 0, "Bash: ls -l", 10, 100));
    Check(s_words.size() == 2, "a record that keeps it working is silent");
    Advance(400);
    model::SetLink(false);
    model::SetLink(true);
    model::SetAgent(Agent(8, 0, "sound-check-2", model::AgentState::kWorking, 0, "Bash: ls", 10, 100));
    Check(s_words.size() == 3 && s_words[2] == Word::kLinkUp, "an agent starting under the link coming up is dropped: link up is heard whole");
    Advance(400);
    model::Usage limited;
    limited.limit = model::Limit::kFiveHour;
    model::SetUsage(limited);
    Check(s_words.size() == 4 && s_words[3] == Word::kUsageLimit, "the plan's limit hit sounds usage limit");
    Ask(1201, 7, "Bash", "ls -la");
    Check(s_words.size() == 5 && s_words[4] == Word::kAttention, "a request queued sounds attention, over the limit's");
    Advance(1000);
    model::SetStandby(true);
    model::SetLink(false);
    model::SetLink(true);
    Ask(1202, 7, "Bash", "ls");
    Check(s_words.size() == 5, "on standby, another deck the active one, the deck's news is silent");
    model::Reset();
    Advance(1000);
    model::SetUsage(limited);
    Check(s_words.size() == 6 && s_words[5] == Word::kUsageLimit,
          "standby's end, a Reset, clears the limit it heard: the next limit hit sounds");
    Settle();
    Home();
    model::Reset();
    Settle();
}

}  // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // a crash keeps what was printed
    if (!StartLvgl()) {
        return 1;
    }
    if (ui::Start() != ESP_OK || router::Init() != ESP_OK) {
        std::printf("ui or the router did not start\n");
        return 1;
    }
    agent_deck::Install();
    app_hooks::Seal();
    app_hooks::RunBoot(app_hooks::Phase::kBoardUp);
    if (router::SetRoot(screen_home::Make()) != ESP_OK) {
        std::printf("home did not build\n");
        return 1;
    }
    // The hardware test's home and Display's routes, stood in for: the deck reaches them by id alone.
    constexpr router::Route kHardware[] = {{"hw.home", router::NoParam<HardwareStub>},
                                           {"hw.display", router::NoParam<HardwareStub>},
                                           {"hw.display.grays", router::NoParam<HardwareStub>}};
    router::AddRoutes({kHardware, sizeof(kHardware) / sizeof(kHardware[0])});
    Settle();
    TestSplash();
    TestOverview();
    TestAgentPage();
    TestWalk();
    TestSound();
    TestRequest();
    TestWaiting();
    TestQuestion();
    TestClosed();
    TestCovered();
    TestStageOutlivesItsMascot();
    TestSettings();
    TestWifi();
    TestWifiMock();
    TestSwipeWifi();
    TestSwipeSettings();
    TestSwipeWaiting();
    TestSteppedLines();
    TestStandbyPage();
    TestPower();
    TestAvatars();
    TestAvatarRefresh();
    DumpAvatarCycle();
    DumpSteppedLines();
    TestFactories();
    TestClosedPageAndClock();
    TestVoice();
    TestEventSounds();
    std::printf("%d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
