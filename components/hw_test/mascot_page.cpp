// The hardware test's mascot page (STICKY_DESIGN_TOKENS §13): Claude's mascot alone on a stage band across the
// portrait glass, one move at a time — the deck's fourteen and its rest — played once or looped, each moment of a move
// heard through the buzzer, so every move, its sounds and the ghosting a loop leaves can be judged on the glass. A move
// runs as the deck's stage runs it (widgets::mascot_stage_run, AGENT_DECK 9.8), its numbers unchanged, and the glass
// samples it at its own pace, about two frames a second on either board, by the partial refresh (MASCOT §6); a full
// refresh lands wherever the twentieth partial does, and the move goes on through it. The page opens on Rest, still, and
// changes nothing until a press. Its stage, its loop and the mascot's sounds are DESIGN → Exceptions → The hardware-test
// demo's.

#include "mascot_page.h"

#include <array>
#include <cinttypes>
#include <cstdio>

#include "design_tokens.h"
#include "lvgl_display.h"
#include "mascot_rules.h"
#include "mascot_voice.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/mascot.h"
#include "widgets/widgets.h"

namespace hw_test {
namespace {

namespace dt = design_tokens;
using ui::Type;
using widgets::MascotMove;

// One flipbook frame: the partial refresh's hold, which the mascot's moments share (`sound.moments-max`, MASCOT §6) —
// what a pass's length is divided by to say about how many frames it takes before it has played.
constexpr uint32_t kFrameMs = dt::kSoundMomentsMaxMs;
// On a V1.0 the page keeps the person's preset, by default Keep waveform, whose frame is longer: 541 ms, 1.84 frames a
// second; under a scan preset the estimate runs long. The scans' 9 fps left the moves' ghost on the mascot's dithered body (the person's choice, 2026-11-25;
// STICKY_EPAPER_WAVEFORMS §3.5, §4.2).
constexpr uint32_t kV1FrameMs = 541;
// The stage: the glass's width, 15 cells high, under the status bar and `spacing.md`; the mascot's root — 10 x 8 cells
// (widgets/mascot.h) — stands centred on its bottom edge, so the 7 cells above it hold the flag and the confetti, which
// reaches 359 px above the ground at the 24 px cell (mock 4).
constexpr int32_t kCell = dt::kSpacingMascotCell;
constexpr int32_t kMascotWidth = 10 * kCell;
constexpr int32_t kMascotHeight = 8 * kCell;
constexpr int32_t kStageTop = dt::kSpacingStatusBar + dt::kSpacingMd;
constexpr int32_t kStageHeight = 15 * kCell;
static_assert((widgets::kMascotPageCentreX - kMascotWidth / 2) % 4 == 0 && (kStageTop + kStageHeight - kMascotHeight) % 4 == 0,
              "the root on the 4 px grid: every mascot pixel holds one phase of the dithers");
static_assert(widgets::kMascotPageCentreY == kStageTop + kStageHeight - kMascotHeight / 2,
              "the widget reckons its travel from where this page stands it");
// How often the page looks at the glass while a move plays: well inside a frame, so the corner's count is set before
// the next frame is drawn — the refresh benchmark's period.
constexpr uint32_t kWatchMs = 5;
// The footer: Back as on every screen, then Loop and Play, a third each of what is left.
constexpr int32_t kBackWidth = dt::kSpacingButtonBack;
constexpr int32_t kThirdWidth = (dt::kSpacingContent - kBackWidth - 2 * dt::kSpacingItem) / 2;

constexpr int kMoves = static_cast<int>(MascotMove::kIdleInPlace) + 1;
// The moves' names, in the widget's order, in the title with their place: "Idle in place · 15 of 15" is 432 px.
constexpr std::array<const char *, kMoves> kNames = {
    "Rest", "Jump",  "Work", "Finish", "Celebrate", "Idle",      "Wave",         "Needs you",
    "Hello", "Skip", "Spin", "Dust",   "Faceplant", "Skip trip", "Idle in place",
};

// Every frame the glass has taken — drawn, or found unchanged and not sent; a failed refresh is no frame.
uint32_t RefreshCount(const lvgl_display::Stats &st)
{
    return st.full_count + st.partial_count + st.gray_count + st.skipped;
}

class MascotPage final : public router::Screen {
public:
    void OnCreate(lv_obj_t *root, lv_group_t *) override
    {
        ui::StyleScreen(root);
        // The title names what the page shows: the move and its place in the list (mock 4).
        title_ = lv_obj_get_child(ui::NewStatusBar(root, "Mascot"), 0);
        lv_obj_t *body = Column(root, 0, dt::kSpacingMd);
        lv_obj_set_width(body, lv_pct(100));
        BuildStage(body);
        BuildControls(Column(body, dt::kSpacingEdge, dt::kSpacingGroup));
        BuildFooter(root);
        // Play and Loop off: their reason is the readout's first line.
        widgets::LinkDisabledReason(play_, readout_);
        widgets::LinkDisabledReason(loop_, readout_);
        ShowMove();
        lv_group_focus_obj(next_);  // on Rest, Play and Loop are off: the first press rings Next
    }

    void OnEnter() override
    {
        const lvgl_display::Stats st = lvgl_display::GetStats();
        full_base_ = st.full_count;
        frame_ms_ = st.ssd2677 ? kFrameMs : kV1FrameMs;
        ShowRest();
    }

    void OnExit() override
    {
        Stop();
    }

private:
    using Mode = mascot_rules::Mode;

    // --- Building --------------------------------------------------------------------------------------------------

    // A column that fills what is left of `parent`, `pad_hor` in from each side and `pad_top` below its top, its rings let out.
    static lv_obj_t *Column(lv_obj_t *parent, int32_t pad_hor, int32_t pad_top)
    {
        lv_obj_t *column = lv_obj_create(parent);
        lv_obj_remove_style_all(column);
        lv_obj_set_width(column, lv_pct(100));
        lv_obj_set_flex_grow(column, 1);
        lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_hor(column, pad_hor, 0);
        lv_obj_set_style_pad_top(column, pad_top, 0);
        lv_obj_set_scrollable(column, false);
        ui::LetRingOut(column);
        return column;
    }

    // The stage: full bleed, white, no hit area; it clips what the moves draw to its band. The frame's count at its top
    // left, the mascot standing on its bottom edge.
    void BuildStage(lv_obj_t *body)
    {
        lv_obj_t *stage = lv_obj_create(body);
        lv_obj_remove_style_all(stage);
        lv_obj_set_size(stage, lv_pct(100), kStageHeight);
        lv_obj_set_scrollable(stage, false);
        lv_obj_set_clickable(stage, false);
        count_ = ui::NewLabel(stage, Type::kCaption, "", true);
        lv_obj_set_pos(count_, dt::kSpacingEdge, 0);
        lv_obj_set_hidden(count_, true);
        mascot_ = widgets::make_mascot(stage, kCell);
        lv_obj_set_pos(mascot_.root, widgets::kMascotPageCentreX - kMascotWidth / 2,
                       widgets::kMascotPageCentreY - kStageTop - kMascotHeight / 2);
        voice_.Attach(mascot_);  // its moments and its music, heard as the deck's are (mascot_voice.h)
    }

    // Under the stage: the readout — three caption lines in one label, as the Display screen's — Previous and Next, and
    // the Move sounds toggle.
    void BuildControls(lv_obj_t *content)
    {
        readout_ = ui::NewLabel(content, Type::kCaption, "", true);
        lv_obj_t *steps = lv_obj_create(content);
        lv_obj_remove_style_all(steps);
        lv_obj_set_size(steps, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(steps, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(steps, dt::kSpacingItem, 0);
        lv_obj_set_style_margin_top(steps, dt::kSpacingGroup, 0);
        lv_obj_set_scrollable(steps, false);
        ui::LetRingOut(steps);
        const int32_t half = (dt::kSpacingContent - dt::kSpacingItem) / 2;
        previous_ = widgets::MakeButton(steps, "Previous", widgets::ButtonVariant::kOutline, half);
        lv_obj_add_event_cb(previous_, OnPrevious, LV_EVENT_CLICKED, this);
        next_ = widgets::MakeButton(steps, "Next", widgets::ButtonVariant::kOutline, half);
        lv_obj_add_event_cb(next_, OnNext, LV_EVENT_CLICKED, this);
        sounds_ = widgets::MakeToggleRow(content, "Move sounds", true);
        lv_obj_set_style_margin_top(sounds_.root, dt::kSpacingItem, 0);
        lv_obj_add_event_cb(sounds_.toggle, OnSounds, LV_EVENT_VALUE_CHANGED, this);
    }

    // The footer: Back as on every screen, then Loop and Play.
    void BuildFooter(lv_obj_t *root)
    {
        lv_obj_t *footer = lv_obj_create(root);
        lv_obj_remove_style_all(footer);
        lv_obj_set_size(footer, lv_pct(100), dt::kSpacingFooter);
        lv_obj_set_style_pad_hor(footer, dt::kSpacingEdge, 0);
        lv_obj_set_style_pad_column(footer, dt::kSpacingItem, 0);
        lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollable(footer, false);
        ui::LetRingOut(footer);
        lv_obj_t *back = widgets::MakeButton(footer, "Back", widgets::ButtonVariant::kOutline, kBackWidth);
        lv_obj_add_event_cb(back, OnBack, LV_EVENT_CLICKED, nullptr);
        loop_ = widgets::MakeButton(footer, "Loop", widgets::ButtonVariant::kOutline, kThirdWidth);
        lv_obj_add_event_cb(loop_, OnLoop, LV_EVENT_CLICKED, this);
        play_ = widgets::MakeButton(footer, "Play", widgets::ButtonVariant::kAction, kThirdWidth);
        lv_obj_add_event_cb(play_, OnPlay, LV_EVENT_CLICKED, this);
    }

    // --- Events ------------------------------------------------------------------------------------------------------

    static MascotPage *Of(lv_event_t *e) { return static_cast<MascotPage *>(lv_event_get_user_data(e)); }
    static void OnBack(lv_event_t *) { router::Pop(); }
    static void OnPrevious(lv_event_t *e) { Of(e)->Step(-1); }
    static void OnNext(lv_event_t *e) { Of(e)->Step(1); }
    static void OnPlay(lv_event_t *e)
    {
        MascotPage *page = Of(e);
        if (page->pass_.mode == Mode::kRest) {
            page->Start(Mode::kOnce);
        } else {
            page->Stop();
            page->ShowRest();
        }
    }
    static void OnLoop(lv_event_t *e) { Of(e)->Start(Mode::kLoop); }
    static void OnSounds(lv_event_t *e)
    {
        MascotPage *page = Of(e);
        page->voice_.SetOn(lv_obj_has_state(page->sounds_.toggle, LV_STATE_CHECKED));
    }
    static void OnWatch(lv_timer_t *t) { static_cast<MascotPage *>(lv_timer_get_user_data(t))->Watch(); }

    // --- Playing -----------------------------------------------------------------------------------------------------

    MascotMove Move() const { return static_cast<MascotMove>(move_); }
    uint32_t RunMs() const { return widgets::mascot_stage_run(Move()).run_ms; }

    // About how many frames a pass of the move takes: once it has played, the count it took; before, its length over a frame.
    uint32_t Frames() const
    {
        return measured_[move_] > 0 ? measured_[move_]
                                    : mascot_rules::EstimateFrames(widgets::mascot_stage_run(Move()).pass_ms, frame_ms_);
    }

    void Start(Mode mode)
    {
        if (move_ == 0) {
            return;
        }
        pass_.Start(mode);
        lv_label_set_text(lv_obj_get_child(play_, 0), "Stop");
        if (lv_group_get_focused(lv_obj_get_group(play_)) == loop_) {
            router::Focus(play_);  // before Loop goes off: a disabled button is never focused
        }
        lv_obj_add_state(loop_, LV_STATE_DISABLED);
        if (mode == Mode::kLoop) {
            mascot_rules::FormatLooping(said_[0].data(), said_[0].size(), 0);
        } else {
            Line(0, "Playing…");
        }
        Line(1, "");
        Line(2, "");
        Say();
        BeginPass();
        if (watch_ == nullptr) {
            watch_ = lv_timer_create(OnWatch, kWatchMs, this);
        }
    }

    // The move from its start, as the deck's stage begins it: a move that plays once asked to rest as it begins, so it
    // plays to its end and rests by itself.
    void BeginPass()
    {
        const uint32_t now = lv_tick_get();
        play_state_ = widgets::mascot_play(Move(), now);
        pass_.Begin(now, RefreshCount(lvgl_display::GetStats()), RunMs());
        if (pass_.asked_rest) {
            play_state_.stopped = now;
            play_state_.next = MascotMove::kRest;
        }
        widgets::play_mascot(mascot_, play_state_);
        ShowCount();
        lv_obj_set_hidden(count_, false);
    }

    // The move stops where it is and the next refresh draws rest; the readout is the at-rest one only once the caller
    // shows it. A press that stops it (`drop`) drops its moments still waiting to sound; a pass that ended by itself keeps
    // them, for the rest frame's refresh plays its last pose's sound.
    void Stop(bool drop = true)
    {
        if (watch_ != nullptr) {
            lv_timer_delete(watch_);
            watch_ = nullptr;
        }
        widgets::play_mascot(mascot_, MascotMove::kRest);
        voice_.Stop(drop, drop);
        pass_.mode = Mode::kRest;
        lv_obj_set_hidden(count_, true);
    }

    void Step(int by)
    {
        if (pass_.mode != Mode::kRest) {
            Stop();
        }
        move_ = ((move_ + by) % kMoves + kMoves) % kMoves;
        ShowMove();
        ShowRest();
    }

    // Every few ms while a move plays: a frame drawn moves the corner's count on; the deck's stage's run asks the move to
    // rest; once it rests, the pass is done — the next pass after the loop's rest, or the page at rest (mascot_rules.h).
    void Watch()
    {
        const uint32_t now = lv_tick_get();
        const lvgl_display::Stats st = lvgl_display::GetStats();
        const bool was_resting = pass_.resting;
        if (pass_.Drawn(RefreshCount(st))) {
            if (!was_resting) {
                ShowCount();
            }
            if (st.last == lvgl_display::Refresh::kFull) {
                ShowGlass(st);  // the glass just flashed: its counts, which the next frame draws
            }
        }
        MascotMove shown = MascotMove::kRest;
        uint32_t since = 0;
        const bool at_rest = widgets::mascot_move_now(mascot_, &shown, &since) && shown == MascotMove::kRest;
        voice_.Listen(mascot_, now);
        switch (pass_.Look(now, RunMs(), at_rest)) {
        case mascot_rules::Next::kNothing: return;
        case mascot_rules::Next::kAskRest:
            widgets::ask_mascot(play_state_, MascotMove::kRest, now);
            widgets::play_mascot(mascot_, play_state_);
            return;
        case mascot_rules::Next::kPassEnded:
            // The next frame is the pass's last: rest.
            measured_[move_] = pass_.frame;
            ++played_[move_];
            lv_obj_set_hidden(count_, true);
            if (pass_.mode == Mode::kOnce) {
                Stop(false);
                ShowRest();
                return;
            }
            mascot_rules::FormatLooping(said_[0].data(), said_[0].size(), pass_.passes);
            ShowGlass(st);
            return;
        case mascot_rules::Next::kBeginPass: BeginPass(); return;
        }
    }

    // --- What the page says ------------------------------------------------------------------------------------------

    void ShowMove()
    {
        lv_label_set_text_fmt(title_, "%s · %d of %d", kNames[move_], move_ + 1, kMoves);
    }

    void ShowCount()
    {
        char count[24];
        mascot_rules::FormatCount(count, sizeof count, pass_.frame, Frames());
        lv_label_set_text(count_, count);
    }

    // The readout's last two lines: the partials since the last full refresh and the fulls since the page opened.
    void ShowGlass(const lvgl_display::Stats &st)
    {
        Line(1, "Partials since full: %" PRIu32 "/%d", st.partials, dt::kRefreshPartialLimit);
        Line(2, "Fulls since opening: %" PRIu32, st.full_count - full_base_);
        Say();
    }

    // One of the readout's lines, said with the next Say().
    template <typename... Args>
    void Line(size_t i, const char *format, Args... args)
    {
        if constexpr (sizeof...(Args) == 0) {
            std::snprintf(said_[i].data(), said_[i].size(), "%s", format);
        } else {
            std::snprintf(said_[i].data(), said_[i].size(), format, args...);
        }
    }

    // The readout as its lines stand; a blank line keeps its place, so the label never changes height.
    void Say()
    {
        const auto line = [this](size_t i) { return said_[i][0] != '\0' ? said_[i].data() : " "; };
        lv_label_set_text_fmt(readout_, "%s\n%s\n%s", line(0), line(1), line(2));
    }

    // The page at rest: Play and Loop on for a move, off on Rest with the reason in words; the readout's three lines.
    void ShowRest()
    {
        lv_label_set_text(lv_obj_get_child(play_, 0), "Play");
        if (move_ == 0) {
            lv_obj_t *focused = lv_group_get_focused(lv_obj_get_group(play_));
            if (focused == play_ || focused == loop_) {
                router::Focus(next_);  // before they go off: a disabled button is never focused
            }
            lv_obj_add_state(play_, LV_STATE_DISABLED);
            lv_obj_add_state(loop_, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(play_, LV_STATE_DISABLED);
            lv_obj_remove_state(loop_, LV_STATE_DISABLED);
        }
        mascot_rules::FormatAtRest(said_[0].data(), said_[0].size(), move_ == 0, Frames(), played_[move_]);
        ShowGlass(lvgl_display::GetStats());
    }

    widgets::Mascot mascot_ = {};
    widgets::MascotPlay play_state_ = {};
    widgets::ToggleRow sounds_ = {};
    lv_obj_t *count_ = nullptr;
    lv_obj_t *title_ = nullptr;  // the status bar's words
    lv_obj_t *readout_ = nullptr;
    std::array<std::array<char, 48>, 3> said_ = {};  // the readout's lines
    lv_obj_t *previous_ = nullptr;
    lv_obj_t *next_ = nullptr;
    lv_obj_t *loop_ = nullptr;
    lv_obj_t *play_ = nullptr;
    lv_timer_t *watch_ = nullptr;
    int move_ = 0;  // MascotMove's order: 0 is Rest
    mascot_voice::Voice voice_;  // the Move sounds toggle's: the moments and the music
    mascot_rules::Pass pass_;
    uint32_t full_base_ = 0;
    uint32_t frame_ms_ = kFrameMs;  // this glass's frame on this page: the estimate's divisor
    std::array<uint32_t, kMoves> played_ = {};
    std::array<uint32_t, kMoves> measured_ = {};  // the frames a pass took, last time it played
};

}  // namespace

std::unique_ptr<router::Screen> MascotScreen()
{
    return std::make_unique<MascotPage>();
}

}  // namespace hw_test
