#include "wifi_screen.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "deck_clock.h"
#include "deck_flow.h"
#include "deck_model.h"
#include "deck_rules.h"
#include "deck_ui.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/widgets.h"

namespace agent_deck {
namespace {

using namespace layout;
using model::WifiAsk;
using model::WifiStage;
using ui::Type;
using widgets::ButtonVariant;

// The step bar's running step is drawn at the approved frame's fill: nothing on this page animates.
constexpr int32_t kRunningPermille = 450;
constexpr int kSteps = 3;
constexpr int kRowsChosen = 4;  // the Mac's networks a page
constexpr int kRowsFacts = 5;   // the joined network's facts a page

class Wifi final : public router::Screen {
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
        widgets::ClaimPageSwipe(page_, OnSwipe, this);  // an up or down swipe anywhere turns the page's list (DESIGN → Layout → Paging)
        clock_.Build(page_);
        Build();
        lv_group_focus_obj(primary_);  // opens with no ring: the first press shows it
        router::AttachEscape(OnEscape, this);
    }

    void OnEnter() override
    {
        subscription_ = model::Subscribe(OnModel, this);
        tick_ = lv_timer_create(OnTick, design_tokens::kMotionSelfUpdateDelayMs, this);
        clock_.Update();
        if (!fresh_ && Signature() != built_) {
            Rebuild(false);
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
    }

    void OnCovered(bool covered) override
    {
        if (!covered && dirty_) {
            Rebuild(true);
        }
    }

private:
    // What the page shows: the stage, the switch and the networks, so a change to any rebuilds it.
    std::string Signature() const
    {
        const model::Wifi &w = model::GetWifi();
        std::string s = std::to_string(static_cast<int>(w.stage)) + (model::WifiOn() ? "1" : "0") + w.network.name;
        for (size_t i = 0; i < w.network_count; ++i) {
            s += w.networks[i].name;
        }
        return s;
    }

    lv_obj_t *Heading(const char *title, const char *line)
    {
        lv_obj_t *col = Column(body_, dt::kSpacingContent);
        lv_obj_set_pos(col, dt::kSpacingEdge, kHeadingTop);
        Words(col, Type::kTitle, title);
        if (line != nullptr) {
            MarginTop(Quiet(col, line), -dt::kSpacingXs);  // tight under the heading
        }
        return col;
    }

    // The place a list stands in: as wide as the glass, so its scroll indicator stands in the right margin (a column the content's width would clip
    // it), the rows `spacing.edge` in, `gap` under `above`.
    lv_obj_t *ListHost(lv_obj_t *above, int32_t gap)
    {
        lv_obj_update_layout(body_);
        lv_obj_t *host = Bare(body_);
        lv_obj_set_size(host, kGlassW, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(host, dt::kSpacingEdge, 0);
        lv_obj_set_flex_flow(host, LV_FLEX_FLOW_COLUMN);
        ui::LetRingOut(host);
        lv_obj_set_pos(host, 0, lv_obj_get_y(above) + lv_obj_get_height(above) + gap);
        return host;
    }

    // A column for a state's content, centred on the glass's height once filled (the person, 2026-11-25).
    lv_obj_t *Middle()
    {
        lv_obj_t *col = Column(body_, dt::kSpacingContent);
        lv_obj_set_pos(col, dt::kSpacingEdge, 0);
        return col;
    }
    void CentreMiddle(lv_obj_t *col)
    {
        lv_obj_update_layout(page_);
        lv_obj_set_y(col, (kGlassH - lv_obj_get_height(col)) / 2);
    }

    // The step bar of the join, `done` steps done and the next as `next` says.
    void StepBar(lv_obj_t *parent, int done, widgets::StepState next, int32_t gap)
    {
        const widgets::StepBar bar = widgets::MakeStepBar(parent, dt::kSpacingContent, kSteps);
        for (int i = 0; i < kSteps; ++i) {
            if (i < done) {
                widgets::SetStep(bar, i, widgets::StepState::kDone);
            } else if (i == done && next != widgets::StepState::kWaiting) {
                widgets::SetStep(bar, i, next, next == widgets::StepState::kRunning ? kRunningPermille : 0);
            } else {
                widgets::SetStep(bar, i, widgets::StepState::kWaiting);
            }
        }
        MarginTop(bar.root, gap);
    }

    // Two buttons in two halves at the answers' place, one button centred.
    void Halves(const char *left, ButtonVariant lv, lv_event_cb_t lcb, const char *right, ButtonVariant rv, lv_event_cb_t rcb, lv_obj_t **l,
                lv_obj_t **r)
    {
        *l = widgets::MakeButton(body_, left, lv, kHalfW);
        lv_obj_set_pos(*l, dt::kSpacingEdge, kAnswersTop);
        lv_obj_add_event_cb(*l, lcb, LV_EVENT_CLICKED, this);
        *r = widgets::MakeButton(body_, right, rv, kHalfW);
        lv_obj_set_pos(*r, kHalfRightX, kAnswersTop);
        lv_obj_add_event_cb(*r, rcb, LV_EVENT_CLICKED, this);
    }
    lv_obj_t *Centred(const char *label, ButtonVariant v, lv_event_cb_t cb)
    {
        lv_obj_t *b = widgets::MakeButton(body_, label, v, dt::kSpacingButtonWidth);
        lv_obj_set_pos(b, (kGlassW - dt::kSpacingButtonWidth) / 2, kAnswersTop);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
        return b;
    }

    // Not connected, or Wi-Fi off: the status, then Back | Connect — Connect out of reach while Wi-Fi is off.
    void BuildNotJoined()
    {
        lv_obj_t *l = nullptr, *r = nullptr;
        Heading("Wi-Fi", nullptr);
        lv_obj_t *col = Middle();
        if (model::WifiOn()) {
            widgets::MakeStackedStatusBlock(col, "Not connected", "Connect asks your Mac for its network.", widgets::StatusMark::kWifi);
        } else {
            widgets::MakeStackedStatusBlock(col, "Wi-Fi off", "Turn Wi-Fi on in Settings to connect.", widgets::StatusMark::kWifi);
        }
        CentreMiddle(col);
        Halves("Back", ButtonVariant::kOutline, OnBack, "Connect", ButtonVariant::kAction, OnConnect, &l, &r);
        if (!model::WifiOn()) {
            lv_obj_add_state(r, LV_STATE_DISABLED);  // never reached: Back is the walk's one stop
            primary_ = l;
        } else {
            primary_ = r;
        }
    }

    // Joining: step 2 of 3, the network's name, then Cancel.
    void BuildJoining(const model::Wifi &w)
    {
        Heading("Wi-Fi", nullptr);
        lv_obj_t *col = Middle();
        Words(col, Type::kCaption, "Step 2 of 3");
        char joining[model::kSsidChars + 12];
        std::snprintf(joining, sizeof joining, "Joining %s", w.network.name);
        MarginTop(Step(col, Type::kTitle, joining, widgets::MarqueeKey(kScopeWifi, 0, 1)), dt::kSpacingXs);
        MarginTop(Quiet(col, "This takes a few seconds."), dt::kSpacingXs);
        StepBar(col, 1, widgets::StepState::kRunning, dt::kSpacingLg);
        CentreMiddle(col);
        primary_ = Centred("Cancel", ButtonVariant::kOutline, OnCancel);
    }

    void Build()
    {
        built_ = Signature();
        list_ = {};
        body_ = Bare(page_);
        lv_obj_set_ignore_layout(body_, true);
        lv_obj_set_size(body_, kGlassW, kGlassH);
        lv_obj_set_pos(body_, 0, 0);
        ui::LetRingOut(body_);
        const model::Wifi &w = model::GetWifi();
        stage_ = model::WifiOn() ? w.stage : WifiStage::kNotJoined;
        lv_obj_t *l = nullptr, *r = nullptr;
        switch (stage_) {
        case WifiStage::kJoined: BuildJoined(); break;
        case WifiStage::kNotJoined: BuildNotJoined(); break;
        case WifiStage::kChoosing: BuildChoosing(); break;
        case WifiStage::kJoining: BuildJoining(w); break;
        case WifiStage::kFailed: {
            Heading("Wi-Fi", nullptr);
            lv_obj_t *col = Middle();
            widgets::MakeStackedStatusBlock(col, "Wrong password", "The network refused your Mac's password.", widgets::StatusMark::kWifi);
            StepBar(col, 1, widgets::StepState::kFailed, dt::kSpacingXl);
            CentreMiddle(col);
            Halves("Cancel", ButtonVariant::kOutline, OnCancel, "Retry", ButtonVariant::kAction, OnRetry, &l, &r);
            primary_ = r;
            break;
        }
        case WifiStage::kPaired: {
            Heading("Wi-Fi", nullptr);
            lv_obj_t *col = Middle();
            widgets::MakeStackedStatusBlock(col, "Bridge paired", "Your Mac reaches the deck over Wi-Fi now.", widgets::StatusMark::kCheck);
            StepBar(col, kSteps, widgets::StepState::kDone, dt::kSpacingXl);
            CentreMiddle(col);
            primary_ = Centred("Done", ButtonVariant::kAction, OnDone);
            break;
        }
        }
    }

    // Joined: the network a control — OK or a tap opens the Forget dialog — then its facts, a page at a time, then Back and Disconnect.
    void BuildJoined()
    {
        const model::Wifi &w = model::GetWifi();
        lv_obj_t *col = Heading("Wi-Fi", nullptr);
        lv_obj_t *net = widgets::MakeNetworkRow(col, w.network.name, w.network.locked, w.network.signal, widgets::NetworkState::kConnected,
                                                widgets::NetworkLook::kOutlined, widgets::NetworkName::kStepped,
                                                widgets::MarqueeKey(kScopeWifi, 0, 2));
        MarginTop(net, dt::kSpacingLg);
        lv_obj_add_event_cb(net, OnNetwork, LV_EVENT_CLICKED, this);
        const widgets::ListView list = widgets::MakeListView(ListHost(col, dt::kSpacingMd), kRowsFacts, dt::kSpacingListRowHeight);
        list_ = list;
        for (size_t i = 0; i < w.fact_count; ++i) {
            const widgets::DetailRow row = widgets::MakeDetailRow(list.root, w.facts[i].name, w.facts[i].value);
            widgets::SetDetailRowOneSizeDown(row);  // the labels in `caption-strong`, the values in `caption`
        }
        widgets::UpdateListView(list);  // before the buttons: a page of facts is a stop in the walk, added at the end
        lv_obj_t *l = nullptr, *r = nullptr;
        Halves("Back", ButtonVariant::kOutline, OnBack, "Disconnect", ButtonVariant::kOutline, OnDisconnect, &l, &r);
        primary_ = net;
    }

    // The Mac's networks, strongest first, four a page; Cancel and Scan in halves.
    void BuildChoosing()
    {
        const model::Wifi &w = model::GetWifi();
        lv_obj_t *col = Heading("Networks", "Pick the Mac's network");
        const widgets::ListView list = widgets::MakeListView(ListHost(col, dt::kSpacingLg), kRowsChosen, dt::kSpacingListRowHeight);
        list_ = list;
        lv_obj_t *first = nullptr;
        for (size_t i = 0; i < w.network_count; ++i) {
            const model::Network &n = w.networks[i];
            // A long name steps; the key is the place in the Mac's list, which a page built again keeps.
            lv_obj_t *row = widgets::MakeNetworkRow(list.root, n.name, n.locked, n.signal, widgets::NetworkState::kRest,
                                                    widgets::NetworkLook::kInverted, widgets::NetworkName::kStepped,
                                                    widgets::MarqueeKey(kScopeWifi, static_cast<uint32_t>(i) + 1, 2));
            lv_obj_add_event_cb(row, OnPick, LV_EVENT_CLICKED, this);
            first = first != nullptr ? first : row;
        }
        widgets::UpdateListView(list);
        lv_obj_t *l = nullptr, *r = nullptr;
        Halves("Cancel", ButtonVariant::kOutline, OnCancel, "Scan", ButtonVariant::kAction, OnScan, &l, &r);
        primary_ = first != nullptr ? first : r;
    }

    // Built again for the record's new stage: the control the stage offers rings as it appears.
    void Rebuild(bool ring)
    {
        if (widgets::DialogOpen()) {
            dirty_ = true;
            return;
        }
        dirty_ = false;
        lv_obj_t *old = body_;
        lv_group_remove_all_objs(group_);  // the old page's controls go with it: the new page's join in their own order
        lv_obj_delete(old);
        body_ = nullptr;
        list_ = {};
        Build();
        if (ring) {
            router::Ring(primary_);
        } else {
            lv_group_focus_obj(primary_);
        }
    }

    // The escape: holding OK cancels what a Cancel here would, finishes what Done would, else goes back.
    static void OnEscape(void *user)
    {
        Wifi *self = static_cast<Wifi *>(user);
        switch (self->stage_) {
        case WifiStage::kChoosing:
        case WifiStage::kJoining:
        case WifiStage::kFailed:
            model::AskWifi(WifiAsk::kCancel);
            break;
        case WifiStage::kPaired:
            model::AskWifi(WifiAsk::kDone);
            break;
        default:
            router::Pop();
            break;
        }
    }

    static void OnModel(const model::Change &change, void *user)
    {
        Wifi *self = static_cast<Wifi *>(user);
        if ((change.event == model::Event::kWifi || change.event == model::Event::kSettings) && self->Signature() != self->built_) {
            self->Rebuild(true);
        }
    }
    static void OnTick(lv_timer_t *t) { static_cast<Wifi *>(lv_timer_get_user_data(t))->clock_.Update(); }

    static void OnBack(lv_event_t *) { router::Pop(); }
    static void OnConnect(lv_event_t *) { model::AskWifi(WifiAsk::kConnect); }
    static void OnScan(lv_event_t *) { model::AskWifi(WifiAsk::kScan); }
    static void OnCancel(lv_event_t *) { model::AskWifi(WifiAsk::kCancel); }
    static void OnRetry(lv_event_t *) { model::AskWifi(WifiAsk::kRetry); }
    static void OnDone(lv_event_t *) { model::AskWifi(WifiAsk::kDone); }
    static void OnDisconnect(lv_event_t *) { model::AskWifi(WifiAsk::kDisconnect); }
    static void OnPick(lv_event_t *e)
    {
        lv_obj_t *row = lv_event_get_current_target_obj(e);
        model::AskWifi(WifiAsk::kPick, static_cast<int>(lv_obj_get_index(row)));
    }

    // The joined network: forgetting it erases its password, so the dialog is destructive — its focus is on Cancel.
    static void OnNetwork(lv_event_t *)
    {
        static std::string title;
        title = std::string("Forget ") + model::GetWifi().network.name + "?";
        widgets::DialogConfig cfg{title.c_str(), "The deck erases its password. Joining it again needs your Mac.", "Forget", "Cancel", true,
                                  OnForgetAnswer, nullptr};
        widgets::OpenDialog(cfg);
    }
    // A swipe begun anywhere on the page turns its list, as one on the list's rows does; a stage with no list has nothing to turn.
    static void OnSwipe(int32_t dir, void *user)
    {
        Wifi *self = static_cast<Wifi *>(user);
        if (self->list_.root != nullptr) {
            widgets::TurnListView(self->list_, dir);
        }
    }

    static void OnForgetAnswer(bool accepted, void *)
    {
        if (accepted) {
            model::AskWifi(WifiAsk::kForget);
        }
    }

    lv_group_t *group_ = nullptr;
    lv_obj_t *page_ = nullptr;
    lv_obj_t *body_ = nullptr;
    lv_obj_t *primary_ = nullptr;
    widgets::ListView list_ = {};  // the stage's list, which a swipe anywhere turns; none on a stage without one
    WifiStage stage_ = WifiStage::kNotJoined;
    std::string built_;
    Clock clock_;
    int subscription_ = 0;
    lv_timer_t *tick_ = nullptr;
    bool fresh_ = true;
    bool dirty_ = false;
};

}  // namespace

std::unique_ptr<router::Screen> WifiScreen()
{
    return std::make_unique<Wifi>();
}

}  // namespace agent_deck
