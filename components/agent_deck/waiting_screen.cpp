#include "waiting_screen.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "deck_clock.h"
#include "deck_flow.h"
#include "deck_model.h"
#include "deck_rules.h"
#include "deck_ui.h"
#include "mascot_voice.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/widgets.h"

namespace agent_deck {
namespace {

using namespace layout;
using ui::Type;

// The agents' avatars in the rows: the mascot at 6 px a cell, 60 x 48, in the row's left, the words after it.
constexpr int32_t kAvatarCell = dt::kSpacingMascotCell / 4;  // a quarter of the home mascot's cell: 6 px
static_assert(dt::kSpacingMascotCell % 4 == 0, "the avatar's cell is a whole quarter of the mascot's");
constexpr int32_t kAvatarW = 10 * kAvatarCell;
constexpr int32_t kAvatarH = 8 * kAvatarCell;
// The avatars blink on one clock, so a blink told within this of the last is the same blink: as long as a blink holds shut (the deck's
// kBlinkShutMs, 110 ms), so a list rebuilt mid-blink pips once, and shorter than a double blink's gap, 230 ms, so its second shut is heard.
constexpr uint32_t kSameBlinkMs = 110;

class Waiting final : public router::Screen {
public:
    void OnCreate(lv_obj_t *root, lv_group_t *group) override
    {
        group_ = group;
        ui::StyleScreen(root);
        page_ = Bare(root);
        lv_obj_set_ignore_layout(page_, true);
        lv_obj_set_size(page_, kGlassW, kGlassH);
        lv_obj_set_pos(page_, 0, 0);
        ui::LetRingOut(page_);
        widgets::ClaimPageSwipe(page_, OnSwipe, this);  // an up or down swipe anywhere turns the list, as every paged screen's (DESIGN → Layout → Paging)
        clock_.Build(page_);
        Build();
        router::AttachEscape(OnEscape, this);  // holding OK is Dismiss
    }

    void OnEnter() override
    {
        subscription_ = model::Subscribe(OnModel, this);
        tick_ = lv_timer_create(OnTick, design_tokens::kMotionSelfUpdateDelayMs, this);
        // The avatars' idle cycle begins from here, and is heard as the carousel's mascot is: the sound setting (STICKY_AGENT_DECK §4.3).
        voice_.SetOn(model::SoundOn());
        last_pick_ms_ = lv_tick_get();
        look_ = lv_timer_create(OnLook, rules::kAvatarLookMs, this);
        if (!fresh_) {
            Rebuild();  // back from a request: the queue may have changed under the list
        }
        fresh_ = false;
    }

    void OnExit() override
    {
        if (subscription_ != 0) {
            model::Unsubscribe(subscription_);
            subscription_ = 0;
        }
        if (tick_ != nullptr) {
            lv_timer_delete(tick_);
            tick_ = nullptr;
        }
        // A covered or left list stops: the cycle's timer, and every avatar home at once, its sounds dropped (DESIGN → Motion → Waits and things
        // that update on their own). It is begun again as the list is entered.
        if (look_ != nullptr) {
            lv_timer_delete(look_);
            look_ = nullptr;
        }
        for (Avatar &avatar : avatars_) {
            if (avatar.idling && avatar.rig.root != nullptr && lv_obj_is_valid(avatar.rig.root)) {
                widgets::play_mascot(avatar.rig, widgets::MascotMove::kRest);
            }
            avatar.idling = false;
        }
        voice_.Stop(true, true);
    }

private:
    struct Row {
        lv_obj_t *row = nullptr;
        uint16_t agent = 0;
    };

    // A row's mascot, and its idle cycle if one plays: begun at `play.since`, asked home at `until` (an lv_tick_get() time).
    struct Avatar {
        widgets::Mascot rig = {};
        widgets::MascotPlay play = {};
        bool idling = false;
        uint32_t until = 0;
    };

    // The title, its count and the list of rows, then Dismiss: built whole, so a rebuild keeps the walk's order — the rows, then Dismiss.
    void Build()
    {
        uint16_t queued[model::kMaxRequests] = {};
        size_t n = 0;
        for (size_t i = 0; i < model::QueueCount() && n < model::kMaxRequests; ++i) {
            queued[n++] = model::QueueAt(i)->agent;
        }
        uint16_t waiting[model::kMaxRequests] = {};
        count_ = rules::WaitingAgents(queued, n, waiting);

        body_ = Column(page_, dt::kSpacingContent);
        lv_obj_set_pos(body_, dt::kSpacingEdge, kHeadingTop);
        Words(body_, Type::kTitle, "Needs you");
        char sessions[24];
        std::snprintf(sessions, sizeof sessions, count_ == 1 ? "%u session" : "%u sessions", static_cast<unsigned>(count_));
        // Tight under the heading, in `caption-regular`: the heading 30 px semibold over 20 px regular differs in size and weight.
        MarginTop(Quiet(body_, sessions, Fit::kWords, Type::kCaptionRegular), -dt::kSpacingXs);

        const widgets::ListView list = widgets::MakeListView(body_, kRowsPerPage, dt::kSpacingListRowHeight);
        list_ = list;
        MarginTop(list.frame, dt::kSpacingLg);
        rows_ = {};
        avatars_ = {};
        bool backgrounds[model::kMaxRequests] = {};
        for (size_t i = 0; i < count_; ++i) {
            const model::Agent *agent = model::FindAgent(waiting[i]);
            const model::Item *item = model::OldestFor(waiting[i]);
            if (item == nullptr) {
                continue;
            }
            char facts[96];
            const uint32_t waited = lv_tick_diff(lv_tick_get(), item->opened_ms) / 1000;
            const char *kind = agent != nullptr && agent->parent != 0 ? (agent->kind[0] != '\0' ? agent->kind : "Sub-agent") : nullptr;
            rules::FormatWaiting(facts, sizeof facts, kind, agent != nullptr && agent->background, item->is_question ? "Question" : item->tool,
                                 waited);
            // The name in `body`, the page's own words: the list row's title role, and the facts under it in `caption-regular`, 20 px regular.
            // Names and facts too long for the row step instead of ending in dots, resting at their start after their passes: a row is a control. The
            // facts count the wait, so their words change every rebuild and the pass goes on through it (widgets/marquee.h).
            lv_obj_t *row = widgets::MakeListRow(list.root, agent != nullptr ? agent->name : "", facts, OnRow, this, Type::kBody,
                                                 widgets::ListRowFit::kSteppedCounting, widgets::MarqueeKey(kScopeList, waiting[i], 1),
                                                 Type::kCaptionRegular);
            lv_obj_set_style_pad_left(row, dt::kSpacingMd + kAvatarW + dt::kSpacingMd, 0);  // the avatar's room, the words after it
            rows_[i] = {row, waiting[i]};
            backgrounds[i] = agent != nullptr && agent->background;
        }
        widgets::UpdateListView(list);
        dismiss_ = widgets::MakeButton(page_, "Dismiss", widgets::ButtonVariant::kOutline, dt::kSpacingButtonWidth);
        lv_obj_set_pos(dismiss_, (kGlassW - dt::kSpacingButtonWidth) / 2, kAnswersTop);
        lv_obj_add_event_cb(dismiss_, OnDismiss, LV_EVENT_CLICKED, this);
        BuildAvatarLayer(backgrounds);
    }

    void BuildAvatarLayer(const bool (&backgrounds)[model::kMaxRequests])
    {
        // The avatars stand on a layer of their own over the whole page, made last: a move's lean and look carry them past the row,
        // and no row, ring or button clips them or draws over them — the mascot is in front of everything on the page (DESIGN →
        // Exceptions → Claude's mascot on the deck's screens). The list never pages (four rows, four requests at most), so each
        // stands where its row's slot is.
        static_assert(kRowsPerPage >= static_cast<int32_t>(model::kMaxRequests), "the avatars' layer assumes one page of rows");
        lv_obj_update_layout(page_);
        avatar_layer_ = Bare(page_);
        lv_obj_set_ignore_layout(avatar_layer_, true);
        lv_obj_set_size(avatar_layer_, kGlassW, kGlassH);
        lv_obj_set_pos(avatar_layer_, 0, 0);
        lv_obj_set_overflow_visible(avatar_layer_, true);
        lv_obj_remove_flag(avatar_layer_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(avatar_layer_, LV_OBJ_FLAG_SCROLLABLE);
        for (size_t i = 0; i < count_; ++i) {
            if (rows_[i].row != nullptr) {
                avatars_[i].rig = AddAvatar(rows_[i].row, backgrounds[i]);
            }
        }
    }

    // The agent's mascot over the row's left slot, at rest in its own grey, the words after it; on the avatars' layer, heard through the
    // list's one voice.
    widgets::Mascot AddAvatar(lv_obj_t *row, bool previous)
    {
        lv_area_t content;
        lv_obj_get_content_coords(row, &content);
        const widgets::Mascot avatar = widgets::make_mascot(avatar_layer_, kAvatarCell);
        ColourMascot(avatar, previous);
        lv_obj_set_pos(avatar.root, content.x1 - kAvatarW - dt::kSpacingMd,
                       content.y1 + (dt::kSpacingListRowHeight - 2 * dt::kSpacingSm - kAvatarH) / 2);
        voice_.Attach(avatar, kSameBlinkMs);
        return avatar;
    }

    // Each agent's cycle that was under way before a rebuild goes on, on the avatar its row has now.
    void CarryCycles(const std::array<Row, model::kMaxRequests> &was_rows, const std::array<Avatar, model::kMaxRequests> &was_avatars)
    {
        for (size_t i = 0; i < count_; ++i) {
            for (size_t j = 0; j < was_rows.size(); ++j) {
                if (was_rows[j].agent == rows_[i].agent && was_rows[j].agent != 0 && was_avatars[j].idling) {
                    avatars_[i].play = was_avatars[j].play;
                    avatars_[i].until = was_avatars[j].until;
                    avatars_[i].idling = true;
                    widgets::play_mascot(avatars_[i].rig, avatars_[i].play);
                }
            }
        }
    }

    // The list follows the queue: rebuilt from the model, the ring where it was — on the same agent's row while it waits, else the row that
    // took its place or the one before it, ringed only if it showed.
    void Rebuild()
    {
        uint16_t was_agent = 0;
        size_t was_index = 0;
        bool ringed = false;
        bool on_dismiss = false;
        if (lv_obj_t *focused = group_ != nullptr ? lv_group_get_focused(group_) : nullptr) {
            ringed = lv_obj_has_state(focused, LV_STATE_FOCUS_KEY);
            on_dismiss = focused == dismiss_;
            for (size_t i = 0; i < rows_.size(); ++i) {
                if (rows_[i].row == focused) {
                    was_agent = rows_[i].agent;
                    was_index = i;
                }
            }
        }
        // A cycle under way carries on through the rebuild, its agent still on the list: the waits count on, and the list is made again.
        const std::array<Row, model::kMaxRequests> was_rows = rows_;
        const std::array<Avatar, model::kMaxRequests> was_avatars = avatars_;
        if (body_ != nullptr) {
            lv_obj_delete(body_);
            lv_obj_delete(dismiss_);
            if (avatar_layer_ != nullptr) {
                lv_obj_delete(avatar_layer_);
            }
            body_ = dismiss_ = avatar_layer_ = nullptr;
            list_ = {};
        }
        Build();
        CarryCycles(was_rows, was_avatars);
        if (count_ == 0) {
            // Nothing waits: on to the asking agent's page, or the overview — after this call returns, since a router call may be what ran it.
            lv_async_call(OnNoneLeft, nullptr);
            return;
        }
        lv_obj_t *target = nullptr;
        for (size_t i = 0; i < count_; ++i) {
            if (rows_[i].agent == was_agent && was_agent != 0) {
                target = rows_[i].row;
            }
        }
        if (target == nullptr && was_agent != 0) {
            target = rows_[std::min(was_index, count_ - 1)].row;
        }
        if (on_dismiss) {
            target = dismiss_;
        }
        if (target != nullptr) {
            if (ringed) {
                router::Ring(target);
            } else {
                lv_group_focus_obj(target);
            }
        }
    }

    static void OnNoneLeft(void *)
    {
        router::PopToRoot();
    }

    static void OnModel(const model::Change &change, void *user)
    {
        switch (change.event) {
        case model::Event::kRequestOpened:
        case model::Event::kRequestClosed:
        case model::Event::kRequestAnswered:
        case model::Event::kAgent:
        case model::Event::kAgentRemoved:
            static_cast<Waiting *>(user)->Rebuild();
            break;
        default:
            break;
        }
    }

    static void OnTick(lv_timer_t *t)
    {
        Waiting *self = static_cast<Waiting *>(lv_timer_get_user_data(t));
        self->clock_.Update();
        self->Rebuild();  // the waits count on
    }

    static void OnLook(lv_timer_t *t) { static_cast<Waiting *>(lv_timer_get_user_data(t))->Look(); }

    // Each avatar's cycle home at its end; and every `kAvatarIdleEveryMs`, the next to begin one (rules::PickAvatar). A pick whose avatar is still
    // in its cycle — one alone on the list — is not begun again part way, a cut (AGENT_DECK 9.4): it waits for the next.
    void Look()
    {
        const uint32_t now = lv_tick_get();
        for (size_t i = 0; i < count_; ++i) {
            Avatar &avatar = avatars_[i];
            if (avatar.idling && static_cast<int32_t>(now - avatar.until) >= 0) {
                avatar.idling = false;
                widgets::ask_mascot(avatar.play, widgets::MascotMove::kRest, now);
                widgets::play_mascot(avatar.rig, avatar.play);
            }
        }
        if (count_ == 0 || lv_tick_diff(now, last_pick_ms_) < rules::kAvatarIdleEveryMs) {
            return;
        }
        last_pick_ms_ = now;
        const rules::AvatarPick pick = rules::PickAvatar(now, count_, last_picked_);
        Avatar &avatar = avatars_[pick.row];
        if (avatar.idling) {
            return;
        }
        last_picked_ = pick.row;
        avatar.play = pick.look ? widgets::mascot_play_look(now) : widgets::mascot_play(widgets::MascotMove::kIdle, now);
        avatar.until = now + rules::AvatarCycleMs(pick.look);
        avatar.idling = true;
        widgets::play_mascot(avatar.rig, avatar.play);
    }

    static void OnRow(lv_event_t *e)
    {
        const Waiting *self = static_cast<Waiting *>(lv_event_get_user_data(e));
        const lv_obj_t *row = lv_event_get_current_target_obj(e);
        for (const Row &r : self->rows_) {
            if (r.row == row) {
                flow::OpenOldestOf(r.agent, true);
                return;
            }
        }
    }

    // Dismiss: home on the overview.
    static void Leave()
    {
        flow::ClearReturn();
        router::PopToRoot();
    }
    static void OnDismiss(lv_event_t *) { Leave(); }
    static void OnEscape(void *) { Leave(); }
    static void OnSwipe(int32_t dir, void *user)
    {
        Waiting *self = static_cast<Waiting *>(user);
        if (self->list_.root != nullptr) {
            widgets::TurnListView(self->list_, dir);
        }
    }

    static constexpr int32_t kRowsPerPage = 4;

    lv_group_t *group_ = nullptr;
    lv_obj_t *page_ = nullptr;
    lv_obj_t *body_ = nullptr;
    widgets::ListView list_ = {};  // the rows, which a swipe anywhere turns: one page of four at most, so `sound.at-end` answers it
    lv_obj_t *dismiss_ = nullptr;
    lv_obj_t *avatar_layer_ = nullptr;  // the avatars, over everything on the page
    std::array<Row, model::kMaxRequests> rows_{};
    std::array<Avatar, model::kMaxRequests> avatars_{};  // a row each, in rows_' order
    mascot_voice::Voice voice_;                          // the avatars' moments, heard as the carousel's mascot is
    size_t last_picked_ = model::kMaxRequests;           // the row that began the last cycle
    uint32_t last_pick_ms_ = 0;
    lv_timer_t *look_ = nullptr;
    size_t count_ = 0;
    Clock clock_;
    int subscription_ = 0;
    lv_timer_t *tick_ = nullptr;
    bool fresh_ = true;  // built a moment ago: the first enter has nothing to rebuild
};

}  // namespace

std::unique_ptr<router::Screen> WaitingScreen()
{
    return std::make_unique<Waiting>();
}

}  // namespace agent_deck
