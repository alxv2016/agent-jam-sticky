#include "settings_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "deck_clock.h"
#include "deck_flow.h"
#include "deck_model.h"
#include "deck_rules.h"
#include "deck_ui.h"
#include "lvgl_display.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/widgets.h"

namespace agent_deck {
namespace {

using namespace layout;
using ui::Type;

// Words that never take a ring stand on the content's edges, x 24 and 456, with the heading: Sound's and Wi-Fi's names (their ring goes
// round the toggle), Auto sleep's, Battery's. A row whose ring goes round it whole is a card (CardFace), its words `spacing.md` inside
// (the person, 2026-11-26: DESIGN → Layout → Spacing). A value row reserves an edge it never draws: Battery's gives it up.
void FlushWords(lv_obj_t *row)
{
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_hor(row, 0, 0);
}

class Settings final : public router::Screen {
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
        widgets::ClaimPageSwipe(page_, OnSwipe, this);  // an up or down swipe anywhere turns the rows' page (DESIGN → Layout → Paging)
        clock_.Build(page_);
        Build();
    }

    void OnEnter() override
    {
        subscription_ = model::Subscribe(OnModel, this);
        tick_ = lv_timer_create(OnTick, design_tokens::kMotionSelfUpdateDelayMs, this);
        clock_.Update();
        if (!fresh_ && Signature() != built_) {
            Rebuild();  // back from the Wi-Fi page: the network row may say something else
        }
        UpdateBattery();
        fresh_ = false;
    }

    ~Settings() override { lv_async_call_cancel(RebuildNow, this); }

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
            Rebuild();
        }
    }

private:
    // What the network row shows, so a change to it rebuilds the page: the stage's class, the wifi switch and the network's name.
    std::string Signature() const
    {
        const model::Wifi &w = model::GetWifi();
        const char kind = !model::WifiOn() ? 'o' : w.stage == model::WifiStage::kJoined ? 'j' : w.stage == model::WifiStage::kJoining ? 'c' : 'n';
        return std::string(1, kind) + w.network.name + (w.network.locked ? "L" : "O") + static_cast<char>('0' + w.network.signal);
    }

    // The heading and the rows' band under it; returns the rows' column.
    lv_obj_t *BuildBand()
    {
        body_ = Column(page_, dt::kSpacingContent);
        lv_obj_set_pos(body_, dt::kSpacingEdge, kHeadingTop);
        Words(body_, Type::kTitle, "Settings");
        // The rows' band, `spacing.lg` under the heading: it clips the rows to the page on view (BuildPaging), a ring's reach
        // round them so a ring is never cut; it never scrolls itself.
        lv_obj_update_layout(page_);
        band_ = Bare(page_);
        lv_obj_remove_flag(band_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(band_, dt::kSpacingEdge - dt::kFocusRingReach,
                       lv_obj_get_y(body_) + lv_obj_get_height(body_) + dt::kSpacingLg - dt::kFocusRingReach);
        lv_obj_set_width(band_, dt::kSpacingContent + 2 * dt::kFocusRingReach);
        lv_obj_t *rows = Column(band_, dt::kSpacingContent);
        lv_obj_set_style_pad_row(rows, dt::kSpacingMd, 0);
        lv_obj_set_pos(rows, dt::kFocusRingReach, dt::kFocusRingReach);
        rows_ = rows;
        return rows;
    }

    void BuildSoundAndSleep(lv_obj_t *rows)
    {
        // Sound: the buzzer on or off, the setting the home foot's toggle shares. Every control's words are the Sound label's size: caption.
        const widgets::ToggleRow sound = widgets::MakeToggleRow(rows, "Sound", model::SoundOn());
        widgets::SetWordsRole(sound.root, Type::kCaption);
        lv_obj_add_event_cb(sound.toggle, OnSound, LV_EVENT_VALUE_CHANGED, this);
        stops_.push_back(sound.toggle);
        first_stops_[0] = sound.toggle;

        // Auto sleep: its name over the slider (the person, 2026-11-26: no `On battery only` beside it).
        lv_obj_t *name = Row(rows);
        Words(name, Type::kCaption, "Auto sleep");
        const widgets::RangeSlider sleep = widgets::MakeRangeSlider(rows, model::kAutoSleepMin, model::kAutoSleepMax, model::kAutoSleepStep,
                                                                    model::AutoSleepMinutes(), " min");
        MarginTop(sleep.root, dt::kSpacingSm - dt::kSpacingMd);
        lv_obj_add_event_cb(sleep.root, OnSleep, LV_EVENT_VALUE_CHANGED, this);
        stops_.push_back(sleep.root);
        first_stops_[1] = sleep.root;
    }

    void Build()
    {
        built_ = Signature();
        stops_.clear();
        lv_obj_t *rows = BuildBand();
        BuildSoundAndSleep(rows);

        const widgets::ToggleRow wifi = widgets::MakeToggleRow(rows, "Wi-Fi", model::WifiOn());
        widgets::SetWordsRole(wifi.root, Type::kCaption);
        MarginTop(wifi.root, dt::kSpacingSm);
        wifi_row_ = wifi.root;
        lv_obj_add_event_cb(wifi.toggle, OnWifiSwitch, LV_EVENT_VALUE_CHANGED, this);
        stops_.push_back(wifi.toggle);

        BuildNetwork(rows);

        // Time zone: the kit's dropdown, whose select sheet lists the fifteen zones; the page has no Time zone page of its own.
        zone_ = widgets::MakeDropdown(rows, "Time zone", rules::kTimeZones, model::kTimeZones, model::TimeZone());
        CardFace(zone_->field);
        widgets::SetWordsRole(zone_->frame, Type::kCaption);
        lv_obj_add_event_cb(zone_->field, OnZone, LV_EVENT_VALUE_CHANGED, this);
        stops_.push_back(zone_->field);

        // Battery: the percent and the word, a reading on its row — no page, no details; out of the walk.
        const widgets::ValueRow battery = widgets::MakeValueRow(rows, "Battery", "");
        FlushWords(battery.root);
        widgets::SetWordsRole(battery.root, Type::kCaption);
        lv_group_remove_obj(battery.root);
        battery_ = battery;
        UpdateBattery();

        // Hardware test: the rows' last, the kit's list row, its words caption; it opens the hardware test's own home in the settings' place.
        hardware_ = nullptr;
        if (router::FindRoute(flow::kRouteHardware) != nullptr) {
            hardware_ = widgets::MakeListRow(rows, "Hardware test", nullptr, OnHardware, nullptr, Type::kCaption);
            CardFace(hardware_);
            stops_.push_back(hardware_);
        }

        back_ = widgets::MakeButton(page_, "Back", widgets::ButtonVariant::kOutline, dt::kSpacingButtonBack);
        lv_obj_set_pos(back_, dt::kSpacingEdge, kAnswersTop);
        lv_obj_add_event_cb(back_, OnBack, LV_EVENT_CLICKED, nullptr);
        stops_.push_back(back_);
        BuildPaging();
    }

    // The rows paged, whole rows a page, as the kit's lists: they run past Back's ring, so page 1 is the approved six and
    // page 2 starts at Wi-Fi and ends at Hardware test. The band shows one page and clips the rest, so every row stays in the
    // walk; the walk turns the page — a stop on the other page focused shows it — and the kit's scroll indicator stands in
    // the right margin of the rows' band (DESIGN → Components → The kit).
    void BuildPaging()
    {
        indicator_ = {};
        lv_obj_update_layout(page_);
        if (hardware_ == nullptr) {
            page_index_ = 0;
            lv_obj_set_height(band_, lv_obj_get_height(rows_) + 2 * dt::kFocusRingReach);
            return;
        }
        const int32_t band_top = lv_obj_get_y(band_) + dt::kFocusRingReach;
        band_h_ = kAnswersTop - dt::kSpacingMd - band_top;
        content_h_ = lv_obj_get_height(rows_);
        indicator_host_ = Bare(page_);
        lv_obj_set_size(indicator_host_, dt::kSpacingContent, band_h_);
        lv_obj_set_pos(indicator_host_, dt::kSpacingEdge, band_top);
        ui::LetRingOut(indicator_host_);
        lv_obj_remove_flag(indicator_host_, LV_OBJ_FLAG_CLICKABLE);
        indicator_ = widgets::MakeScrollIndicator(indicator_host_, band_h_);
        for (lv_obj_t *stop : stops_) {
            lv_obj_add_event_cb(stop, OnStopFocused, LV_EVENT_FOCUSED, this);
        }
        ShowPage(page_index_);
    }

    // A page: the rows moved up to its first row and the band cut at its last, a ring's reach round both.
    void ShowPage(int page)
    {
        if (hardware_ == nullptr) {
            return;
        }
        page_index_ = page;
        const int32_t top = page == 0 ? 0 : lv_obj_get_y(wifi_row_);
        lv_obj_t *last = page == 0 ? battery_.root : hardware_;
        const int32_t bottom = lv_obj_get_y(last) + lv_obj_get_height(last);
        lv_obj_set_y(rows_, dt::kFocusRingReach - top);
        lv_obj_set_height(band_, bottom - top + 2 * dt::kFocusRingReach);
        widgets::UpdateScrollIndicator(indicator_, page, 2, band_h_, content_h_);
    }

    // A swipe up shows the second page, down the first; past either end nothing turns and `sound.at-end` says so, as a list's swipe
    // does. A ring on a stop the new page hides moves to the page's first stop: Wi-Fi's toggle on the second, Sound's on the first.
    static void OnSwipe(int32_t dir, void *user)
    {
        Settings *self = static_cast<Settings *>(user);
        const int next = self->page_index_ + (dir > 0 ? 1 : -1);
        if (self->hardware_ == nullptr || next < 0 || next > 1) {
            lvgl_display::PlayCue(lvgl_display::Cue::kAtEnd);
            return;
        }
        self->ShowPage(next);
        lv_obj_t *focused = lv_group_get_focused(self->group_);
        const bool hidden = next == 1 ? focused == self->first_stops_[0] || focused == self->first_stops_[1] : focused == self->hardware_;
        if (hidden) {
            lv_group_focus_obj(next == 1 ? self->stops_[2] : self->first_stops_[0]);
        }
    }

    // Which page a stop stands on: Sound and Auto sleep only on the first, Hardware test only on the second; the rest on both.
    static void OnStopFocused(lv_event_t *e)
    {
        Settings *self = static_cast<Settings *>(lv_event_get_user_data(e));
        if (self->rebuilding_) {
            return;  // LVGL refocusing as the old rows go: the page shown is the one kept
        }
        const lv_obj_t *stop = lv_event_get_target_obj(e);
        if (stop == self->hardware_ && self->page_index_ != 1) {
            self->ShowPage(1);
        } else if ((stop == self->first_stops_[0] || stop == self->first_stops_[1]) && self->page_index_ != 0) {
            self->ShowPage(0);
        }
    }

    // The joined network as the kit's network row on the light dither; with none joined, a value row that says so. Either opens the Wi-Fi page.
    void BuildNetwork(lv_obj_t *rows)
    {
        const model::Wifi &w = model::GetWifi();
        const bool on = model::WifiOn();
        lv_obj_t *row = nullptr;
        if (on && (w.stage == model::WifiStage::kJoined || w.stage == model::WifiStage::kJoining)) {
            row = widgets::MakeNetworkRow(rows, w.network.name, w.network.locked, w.network.signal,
                                          w.stage == model::WifiStage::kJoined ? widgets::NetworkState::kConnected
                                                                               : widgets::NetworkState::kConnecting,
                                          widgets::NetworkLook::kCard, widgets::NetworkName::kStepped,
                                          widgets::MarqueeKey(kScopeSettings, 0, 1));
        } else {
            const widgets::ValueRow none = widgets::MakeValueRow(rows, "Network", on ? "Not joined" : "Wi-Fi off");
            widgets::SetWordsRole(none.root, Type::kCaption);
            CardFace(none.root);  // it opens the Wi-Fi page: its ring goes round it whole
            row = none.root;
            if (!on) {
                lv_obj_add_state(row, LV_STATE_DISABLED);  // out of reach while the switch is off, never hidden
            }
        }
        lv_obj_add_event_cb(row, OnNetwork, LV_EVENT_CLICKED, nullptr);
        if (on) {
            stops_.push_back(row);
        }
    }

    void UpdateBattery()
    {
        if (battery_.value == nullptr) {
            return;
        }
        char words[40];
        rules::FormatBattery(words, sizeof words, model::GetBattery().percent, model::GetBattery().charging);
        widgets::SetValueRow(battery_, words);
    }

    // Built again, the walk's place kept: the same stop by its number, ringed only if it showed.
    void Rebuild()
    {
        if (widgets::SelectSheetOpen() || widgets::DialogOpen()) {
            dirty_ = true;
            return;
        }
        dirty_ = false;
        size_t was = 0;
        bool ringed = false;
        if (lv_obj_t *focused = lv_group_get_focused(group_)) {
            ringed = lv_obj_has_state(focused, LV_STATE_FOCUS_KEY);
            for (size_t i = 0; i < stops_.size(); ++i) {
                if (stops_[i] == focused) {
                    was = i;
                }
            }
        }
        rebuilding_ = true;  // deleting a focused stop refocuses another, whose handler must not turn the page
        lv_obj_delete(body_);
        lv_obj_delete(back_);
        lv_obj_delete(band_);
        if (indicator_host_ != nullptr) {
            lv_obj_delete(indicator_host_);
        }
        body_ = back_ = hardware_ = indicator_host_ = rows_ = wifi_row_ = band_ = nullptr;
        battery_ = {};
        Build();
        rebuilding_ = false;
        lv_obj_t *target = stops_[std::min(was, stops_.size() - 1)];
        if (ringed) {
            router::Ring(target);
        } else {
            lv_group_focus_obj(target);
        }
    }

    static void OnModel(const model::Change &change, void *user)
    {
        Settings *self = static_cast<Settings *>(user);
        switch (change.event) {
        case model::Event::kBattery:
            self->UpdateBattery();
            break;
        case model::Event::kWifi:
        case model::Event::kSettings:
            // On the next pass: the switch that changed the row is the object a rebuild deletes, and its own handler is still running.
            if (self->Signature() != self->built_) {
                lv_async_call_cancel(RebuildNow, self);
                lv_async_call(RebuildNow, self);
            }
            break;
        default:
            break;
        }
    }

    static void RebuildNow(void *user)
    {
        Settings *self = static_cast<Settings *>(user);
        if (self->subscription_ == 0) {
            return;  // left in the same pass (Back): the screen is going, and a ring on it would land on nothing
        }
        if (self->Signature() != self->built_) {
            self->Rebuild();
        }
    }

    static void OnTick(lv_timer_t *t)
    {
        static_cast<Settings *>(lv_timer_get_user_data(t))->clock_.Update();
    }

    static void OnSound(lv_event_t *e)
    {
        const lv_obj_t *toggle = lv_event_get_target_obj(e);
        model::SetSoundOn(lv_obj_has_state(toggle, LV_STATE_CHECKED));
    }
    static void OnSleep(lv_event_t *e)
    {
        const widgets::RangeSlider slider{lv_event_get_target_obj(e), nullptr, nullptr, nullptr};  // the value is read from the track alone
        model::SetAutoSleepMinutes(widgets::RangeSliderValue(slider));
    }
    static void OnWifiSwitch(lv_event_t *e)
    {
        const lv_obj_t *toggle = lv_event_get_target_obj(e);
        model::SetWifiOn(lv_obj_has_state(toggle, LV_STATE_CHECKED));
    }
    static void OnZone(lv_event_t *e)
    {
        const Settings *self = static_cast<Settings *>(lv_event_get_user_data(e));
        model::SetTimeZone(widgets::DropdownSelected(self->zone_));
    }
    static void OnNetwork(lv_event_t *)
    {
        if (router::FindRoute(flow::kRouteWifi) != nullptr) {
            router::PushRoute(flow::kRouteWifi);
        }
    }
    static void OnBack(lv_event_t *) { router::Pop(); }
    // The hardware test replaces the settings, not stacked on them: its Display screen's own pages are the fourth screen up, at
    // router::kMaxDepth, so Back from the test's home goes to the deck's home (the person, 2026-11-26).
    static void OnHardware(lv_event_t *) { router::GoToRoute(flow::kRouteHardware); }

    lv_group_t *group_ = nullptr;
    lv_obj_t *page_ = nullptr;
    lv_obj_t *body_ = nullptr;
    lv_obj_t *back_ = nullptr;
    lv_obj_t *hardware_ = nullptr;
    lv_obj_t *rows_ = nullptr;
    lv_obj_t *wifi_row_ = nullptr;
    lv_obj_t *band_ = nullptr;
    lv_obj_t *first_stops_[2] = {};  // their stops: Sound's toggle and the slider
    lv_obj_t *indicator_host_ = nullptr;
    widgets::ScrollIndicator indicator_ = {};
    int32_t band_h_ = 0;
    int32_t content_h_ = 0;
    int page_index_ = 0;
    bool rebuilding_ = false;
    widgets::Dropdown *zone_ = nullptr;
    widgets::ValueRow battery_ = {};
    std::vector<lv_obj_t *> stops_;
    std::string built_;
    Clock clock_;
    int subscription_ = 0;
    lv_timer_t *tick_ = nullptr;
    bool fresh_ = true;
    bool dirty_ = false;
};

}  // namespace

std::unique_ptr<router::Screen> SettingsScreen()
{
    return std::make_unique<Settings>();
}

}  // namespace agent_deck
