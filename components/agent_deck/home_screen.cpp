// The deck's home (STICKY_AGENT_DECK §4): the startup screen for `motion.startup-hold`, then the carousel — the overview, then one page per
// agent in the deck's order — that turns by UP and DOWN, through the focus walk, and by a sideways swipe. One mascot stands in the place
// the home view puts it, in front of every element, playing its state's move; the foot (link, Wi-Fi, sound, settings, power) and the page dots
// persist as pages change; everything else is the page's and is built again from the model whenever a record or a page turn changes it.

#include "home_screen.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <new>

#include "deck_clock.h"
#include "deck_flow.h"
#include "deck_mock.h"
#include "deck_model.h"
#include "deck_rules.h"
#include "deck_stage.h"
#include "deck_ui.h"
#include "design_tokens.h"
#include "esp_log.h"
#include "in_psram.h"
#include "lvgl_display.h"
#include "off_frame.h"
#include "power_leave.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/widgets.h"

extern "C" const lv_image_dsc_t agent_deck_logo;

namespace agent_deck::home {
namespace {

using namespace layout;
using model::Agent;
using model::AgentState;
using ui::Type;
using widgets::MascotMove;

constexpr const char *kTag = "deck_home";

// How often the page counts its state's time on and looks at the clock: DESIGN's self-update, the floor for anything that updates by itself;
// the clock itself changes once a minute (motion.clock-tick).
constexpr uint32_t kTickMs = design_tokens::kMotionSelfUpdateDelayMs;

class Home {
public:
    Home() = default;
    Home(const Home &) = delete;
    Home &operator=(const Home &) = delete;
    ~Home() { Exit(); }
    void Create(lv_obj_t *root, lv_group_t *group);
    void Enter();
    void Exit();
    void Covered(bool covered);
    Probe Look() const;
    void HandOver();

private:
    // --- The startup screen ---------------------------------------------------------------------------------------------------
    void BuildSplash();
    static void OnHold(lv_timer_t *t);

    // --- The carousel -----------------------------------------------------------------------------------------------------------
    void BuildCarousel();
    void Activate();
    void BuildFoot();
    void Refresh(bool turned);
    bool OrderPages(bool turned);
    void BuildOverview();
    void BuildAgent(const Agent &agent);
    void BuildStandby();
    lv_obj_t *BuildChip(const char *text, bool control);
    void BuildWindow(lv_obj_t *column, const char *word, int pct, int32_t resets_min, bool lead, Type figure, bool known);
    void PlaceStop(lv_obj_t *over);
    void UpdateDots();
    void UpdateMascot(bool turned);
    void UpdateLink();
    void TurnTo(int page);
    void WalkTo(int dir);
    int Pages() const { return static_cast<int>(pages_count_) + 1; }
    const Agent *CurrentAgent() const;

    // --- Events --------------------------------------------------------------------------------------------------------------------
    static void OnModel(const model::Change &change, void *user);
    void Heard(const model::Change &change);
    static void OnTick(lv_timer_t *t);
    static void OnSwipe(int32_t dir, void *user);
    static void OnGuard(lv_event_t *e);
    static void OnStopClicked(lv_event_t *e);
    static void OnOpenWaiting(lv_event_t *);
    static void OnReview(lv_event_t *e);
    static void OnConnect(lv_event_t *e);
    static void ConnectNow(void *);
    static void OnWifi(lv_event_t *);
    static void OnSettings(lv_event_t *);
    static void OnPower(lv_event_t *);
    static void OnSound(lv_event_t *e);
    static void OnDefocused(lv_event_t *e);

    lv_obj_t *root_ = nullptr;
    lv_group_t *group_ = nullptr;
    bool carousel_ = false;
    bool active_ = false;
    bool covered_ = false;
    Stage stage_;

    // The startup screen.
    lv_obj_t *splash_ = nullptr;
    lv_timer_t *hold_ = nullptr;
    // The startup screen's hold: the mascot's hello — one hand's wave — and never less than `motion.startup-hold`, counted from the
    // screen's first frame on the glass (the person, 2026-11-26: it waves before it goes home).
    uint32_t hold_left_ms_ = std::max<uint32_t>(design_tokens::kMotionStartupHoldDelayMs, widgets::kMascotHelloMs);
    bool splash_drawn_ = false;  // the startup screen's first frame has reached the glass: the wave and its hold may begin
    void BeginWave();
    static void OnSplashDrawn(lv_event_t *e);
    uint32_t hold_began_ = 0;

    // The carousel's persistent parts.
    lv_obj_t *page_ = nullptr;
    lv_obj_t *guard_prev_ = nullptr;
    lv_obj_t *stop_ = nullptr;
    lv_obj_t *guard_next_ = nullptr;
    lv_obj_t *content_ = nullptr;
    Clock clock_;
    lv_obj_t *dots_ = nullptr;
    int dots_count_ = 0;
    lv_obj_t *foot_ = nullptr;
    lv_obj_t *link_ = nullptr;
    lv_obj_t *wifi_ = nullptr;
    lv_obj_t *sound_ = nullptr;
    lv_obj_t *settings_ = nullptr;
    lv_obj_t *power_ = nullptr;
    lv_obj_t *last_defocused_ = nullptr;
    lv_timer_t *tick_ = nullptr;
    int subscription_ = 0;

    // The page on view and its parts, rebuilt by Refresh.
    int current_ = 0;                 // 0 the overview, else the agent page's place
    uint16_t current_id_ = 0;         // the agent on view, 0 for the overview
    std::array<uint16_t, model::kMaxAgents> pages_{};
    size_t pages_count_ = 0;
    lv_obj_t *chip_ = nullptr;
    lv_obj_t *chip_label_ = nullptr;
    lv_obj_t *review_ = nullptr;
    lv_obj_t *connect_ = nullptr;     // Connect, on the standby page
    bool standby_ = false;            // the page on view is the standby page
    lv_obj_t *control_ = nullptr;     // what OK presses on this page: the chip or Review, or nullptr
    uint32_t control_key_ = 0;        // which control showed last, so a new one is ringed as it appears
    // What the mascot has been told.
    MascotMove desired_ = MascotMove::kRest;
    bool desired_set_ = false;
    MascotMove reaction_ = MascotMove::kRest;
    bool has_reaction_ = false;
    int last_five_ = -1;
};

Home &s_home = lvgl_display::InPsram<Home>();  // in PSRAM (lvgl_display::InPsram): the LVGL task's alone

// --- Building ---------------------------------------------------------------------------------------------------------------------

// A one-pixel stop in the focus group that takes no touch and draws nothing: the walk's guards (below).
lv_obj_t *MakeGuard(lv_obj_t *page, lv_group_t *group)
{
    lv_obj_t *guard = Bare(page);
    lv_obj_set_ignore_layout(guard, true);
    lv_obj_set_size(guard, 1, 1);
    lv_obj_set_pos(guard, 0, 0);
    lv_group_add_obj(group, guard);
    return guard;
}

void Home::Create(lv_obj_t *root, lv_group_t *group)
{
    root_ = root;
    group_ = group;
    carousel_ = false;
    BuildSplash();
}

void Home::BuildSplash()
{
    splash_ = Bare(root_);
    lv_obj_set_ignore_layout(splash_, true);
    lv_obj_set_size(splash_, kGlassW, kGlassH);
    lv_obj_set_pos(splash_, 0, 0);
    // The mascot waving in the home view's place, so the screen hands over to home with the mascot where it stands.
    stage_.Build(splash_, kMascotX, kMascotY, kMascotCell);
    // Under it, centred: the logo, its line and the maker's name.
    lv_obj_t *column = Column(splash_, dt::kSpacingContent);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_pos(column, dt::kSpacingEdge, kMascotY + kMascotH + dt::kSpacingLg);
    lv_obj_t *logo = lv_image_create(column);
    lv_image_set_src(logo, &agent_deck_logo);
    lv_obj_set_style_image_recolor(logo, ui::Level(dt::kColorTextPrimary), 0);
    lv_obj_set_style_image_recolor_opa(logo, LV_OPA_COVER, 0);
    lv_obj_set_clickable(logo, false);
    lv_obj_t *line = Words(column, Type::kCaption, "Designed for Creators");
    MarginTop(line, dt::kSpacingMd);
    lv_obj_t *by = Words(column, Type::kTitle, "ALXV Labs");  // ux-exception: the maker's name, a proper noun
    MarginTop(by, dt::kSpacingXl);
    stage_.ToFront();
}

void Home::OnSplashDrawn(lv_event_t *e)
{
    lv_display_remove_event_cb_with_user_data(static_cast<lv_display_t *>(lv_event_get_target(e)), OnSplashDrawn, nullptr);
    s_home.splash_drawn_ = true;
    if (s_home.active_ && !s_home.carousel_) {
        s_home.BeginWave();
    }
}

void Home::BeginWave()
{
    if (hold_ != nullptr) {
        return;
    }
    if (stage_.Move() == MascotMove::kRest) {
        stage_.Play(MascotMove::kHello);  // the deck's entrance: one hand's wave, then rest
    }
    hold_began_ = lv_tick_get();
    hold_ = lv_timer_create(OnHold, hold_left_ms_ > 0 ? hold_left_ms_ : 1, nullptr);
    lv_timer_set_repeat_count(hold_, 1);
}

void Home::OnHold(lv_timer_t *t)
{
    lv_timer_delete(t);
    s_home.hold_ = nullptr;
    s_home.hold_left_ms_ = 0;
    s_home.HandOver();
}

void Home::HandOver()
{
    if (carousel_) {
        return;
    }
    if (hold_ != nullptr) {
        lv_timer_delete(hold_);
        hold_ = nullptr;
    }
    hold_left_ms_ = 0;
    stage_.Stop();
    BuildCarousel();
    lvgl_display::RequestFull();  // the startup screen leaves no ghost of its logo: the carousel is the next full frame
    if (active_) {
        if (!mock::Start()) {  // the feed starts as home is ready (AGENT_DECK 4.8)
            ESP_LOGE(kTag, "the mock day did not start: the deck shows what the model holds");
        }
        Activate();
    }
}

// The carousel on top: what it hears and counts runs, and the page is built from the model — on the agent a request sent it back to, when one did.
void Home::Activate()
{
    subscription_ = model::Subscribe(OnModel, this);
    stage_.Start();
    stage_.SetHeard(model::SoundOn());
    if (sound_ != nullptr) {
        // Home heard nothing while a screen stood over it: the foot's toggle follows the setting Settings may have changed.
        lv_obj_set_state(sound_, LV_STATE_CHECKED, model::SoundOn());
    }
    tick_ = lv_timer_create(OnTick, kTickMs, this);
    // Back from a request: on the asking agent's page, the spin played if it was allowed there.
    const flow::Return back = flow::TakeReturn();
    bool turned = false;
    if (back.pending) {
        current_id_ = 0;
        for (size_t i = 0; i < model::AgentCount(); ++i) {
            if (model::AgentAt(i)->id == back.agent) {
                current_id_ = back.agent;
            }
        }
        turned = true;
        if (back.allowed) {
            reaction_ = MascotMove::kSpin;
            has_reaction_ = true;
            desired_set_ = true;  // a spin, then the page's own move: not a page turned to
        }
    }
    control_key_ = 0;  // a control rings as home is entered
    model::StandbyTick();  // a Connect that ran out of time while another screen was up
    Refresh(turned && !back.allowed);
}

// The carousel: everything that stays while pages change, then the first page.
void Home::BuildCarousel()
{
    lv_obj_delete(splash_);
    splash_ = nullptr;
    carousel_ = true;
    page_ = widgets::MakeSwipePage(root_, kGlassW, kGlassH, OnSwipe, this);
    // The walk's stops, in the order UP and DOWN reach them: a guard, the page's own stop, a guard, then the foot's icons. A guard turns the
    // page the walk steps onto it from the stop, and gives the focus straight back, so the focus stays on the page; at the last or first page
    // it passes on to the foot instead, and from the foot it comes back to the last or first page (WalkTo).
    guard_prev_ = MakeGuard(page_, group_);
    stop_ = Bare(page_);
    lv_obj_set_ignore_layout(stop_, true);
    lv_obj_set_style_radius(stop_, dt::kRoundedLg, 0);
    ui::RingOutside(stop_);
    lv_group_add_obj(group_, stop_);
    lv_obj_add_event_cb(stop_, OnDefocused, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_event_cb(stop_, OnStopClicked, LV_EVENT_CLICKED, this);
    guard_next_ = MakeGuard(page_, group_);

    content_ = Bare(page_);
    lv_obj_set_ignore_layout(content_, true);
    lv_obj_set_size(content_, kGlassW, kGlassH);
    lv_obj_set_pos(content_, 0, 0);
    ui::LetRingOut(content_);

    clock_.Build(page_);
    BuildFoot();
    // The group focused the first stop added, a guard, before any listened; the page's own stop has it, with no ring, and the guards listen
    // from here on.
    lv_group_focus_obj(stop_);
    lv_obj_remove_state(stop_, LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(guard_prev_, OnGuard, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(guard_next_, OnGuard, LV_EVENT_FOCUSED, this);
    stage_.Build(page_, kMascotX, kMascotY, kMascotCell);
    stage_.SetHeard(model::SoundOn());
    stage_.ToFront();
}

// The foot: the bridge link, an indicator, on the left edge; the icon buttons — Wi-Fi, sound, Settings, then Power last — flush to the right
// edge, `spacing.sm` apart, the space between them.
void Home::BuildFoot()
{
    foot_ = Row(page_, dt::kSpacingContent);
    lv_obj_set_flex_align(foot_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui::LetRingOut(foot_);
    link_ = widgets::MakeLinkIndicator(foot_, model::LinkUp());
    lv_obj_t *buttons = Row(foot_, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(buttons, dt::kSpacingSm, 0);
    ui::LetRingOut(buttons);
    wifi_ = widgets::MakeIconButton(buttons, widgets::IconAction::kWifi, widgets::ButtonVariant::kTonal, "Open Wi-Fi");
    lv_obj_add_event_cb(wifi_, OnWifi, LV_EVENT_CLICKED, nullptr);
    sound_ = widgets::MakeToggleIconButton(buttons);
    lv_obj_set_state(sound_, LV_STATE_CHECKED, model::SoundOn());
    lv_obj_add_event_cb(sound_, OnSound, LV_EVENT_VALUE_CHANGED, this);
    settings_ = widgets::MakeIconButton(buttons, widgets::IconAction::kSettings, widgets::ButtonVariant::kTonal, "Open settings");
    lv_obj_add_event_cb(settings_, OnSettings, LV_EVENT_CLICKED, nullptr);
    power_ = widgets::MakeIconButton(buttons, widgets::IconAction::kPower, widgets::ButtonVariant::kTonal, "Turn off");
    lv_obj_add_event_cb(power_, OnPower, LV_EVENT_CLICKED, nullptr);
    for (lv_obj_t *button : {wifi_, sound_, settings_, power_}) {
        lv_obj_add_event_cb(button, OnDefocused, LV_EVENT_DEFOCUSED, this);
    }
    lv_obj_align(foot_, LV_ALIGN_BOTTOM_LEFT, dt::kSpacingEdge, -kFootBottom);
    lv_obj_update_layout(page_);
}

// --- A page ------------------------------------------------------------------------------------------------------------------------

const Agent *Home::CurrentAgent() const
{
    return current_id_ != 0 ? model::FindAgent(current_id_) : nullptr;
}

// The state chip, centred across the glass: a row the glass's width holds it, so its words changing keeps it centred. A control joins no
// focus group of its own: the page's one stop stands over it (PlaceStop), so the walk's order stays the page's.
lv_obj_t *Home::BuildChip(const char *text, bool control)
{
    lv_obj_t *chip = StateChip(content_, text, control);
    if (control) {
        lv_group_remove_obj(chip);
        lv_obj_add_event_cb(chip, OnOpenWaiting, LV_EVENT_CLICKED, nullptr);
    }
    chip_label_ = lv_obj_get_child(chip, 0);
    return chip;
}

// A usage window: its word and share in `title` or `caption`, the bar `spacing.xs` under them, the reset line right-aligned in quiet words.
void Home::BuildWindow(lv_obj_t *column, const char *word, int pct, int32_t resets_min, bool lead, Type figure, bool known)
{
    lv_obj_t *row = Row(column);
    lv_obj_set_style_pad_column(row, dt::kSpacingSm, 0);
    Words(row, figure, word);
    char share[8];
    if (known && pct >= 0) {
        std::snprintf(share, sizeof share, "%d%%", pct > 100 ? 100 : pct);
    } else {
        std::snprintf(share, sizeof share, "--%%");
    }
    Words(row, figure, share);
    widgets::ProgressBar bar = widgets::MakeProgressBar(column, dt::kSpacingContent, lead);
    widgets::SetProgress(bar, known && pct >= 0 ? pct : 0);
    MarginTop(bar.root, dt::kSpacingXs);
    char resets[24];
    rules::FormatReset(resets, sizeof resets, known ? resets_min : -1);
    if (resets[0] != '\0') {
        lv_obj_t *line = Quiet(column, resets, Fit::kWrap);
        lv_obj_set_style_text_align(line, LV_TEXT_ALIGN_RIGHT, 0);
        MarginTop(line, dt::kSpacingXs);
    }
}

void Home::BuildOverview()
{
    int needs = 0, working = 0, done = 0;
    for (size_t i = 0; i < model::AgentCount(); ++i) {
        const Agent *a = model::AgentAt(i);
        needs += a->state == AgentState::kNeedsYou;
        working += a->state == AgentState::kWorking;
        done += a->state == AgentState::kDone;
    }
    const rules::Chip say = rules::ChipFor(needs, working, done, model::LinkUp(), model::QueueCount() > 0);
    chip_ = nullptr;
    control_ = nullptr;
    if (say.shown) {
        chip_ = BuildChip(say.text, say.control);
        control_ = say.control ? chip_ : nullptr;
    }
    lv_obj_t *column = Column(content_, dt::kSpacingContent);
    lv_obj_set_pos(column, dt::kSpacingEdge, kFactsTop);
    const model::Usage &usage = model::GetUsage();
    const bool known = model::UsageKnown();
    BuildWindow(column, "Usage", usage.five_hour_pct, usage.five_hour_resets_min, true, Type::kTitle, known);
    const uint32_t before = lv_obj_get_child_count(column);
    BuildWindow(column, "Weekly", usage.seven_day_pct, usage.seven_day_resets_min, false, Type::kCaption, known);
    MarginTop(lv_obj_get_child(column, static_cast<int32_t>(before)), dt::kSpacingSm);
    if (!known) {
        lv_obj_t *none = Quiet(column, "No plan data", Fit::kWrap);
        lv_obj_set_style_text_align(none, LV_TEXT_ALIGN_RIGHT, 0);
        MarginTop(none, dt::kSpacingXs);
    }
}

// The doing line: the last tool and its subject, or for a turn the plan's limit stopped the limit instead.
void FormatDoing(char *doing, size_t size, const Agent &agent)
{
    if (agent.limit != model::Limit::kNone && agent.state == AgentState::kDone) {
        std::snprintf(doing, size, "%s", agent.limit == model::Limit::kFiveHour ? "Usage limit"
                                         : agent.limit == model::Limit::kSevenDay ? "Weekly limit" : "Plan limit");
    } else {
        std::snprintf(doing, size, "%s", agent.detail);
    }
}

// A session: `Context 63%` in `title` over the lead bar, at the facts' top as the overview's Usage, its tokens
// right-aligned under it in quiet words, then the doing line `spacing.sm` on, where the overview's Weekly starts.
void SessionFacts(lv_obj_t *column, const Agent &agent, const char *tokens, const char *doing)
{
    lv_obj_t *row = Row(column);
    lv_obj_set_style_pad_column(row, dt::kSpacingSm, 0);
    Words(row, Type::kTitle, "Context");
    char share[8];
    if (agent.context_pct >= 0) {
        std::snprintf(share, sizeof share, "%d%%", agent.context_pct);
    } else {
        std::snprintf(share, sizeof share, "--%%");
    }
    Words(row, Type::kTitle, share);
    widgets::ProgressBar bar = widgets::MakeProgressBar(column, dt::kSpacingContent, true);
    widgets::SetProgress(bar, agent.context_pct >= 0 ? agent.context_pct : 0);
    MarginTop(bar.root, dt::kSpacingXs);
    if (tokens[0] != '\0') {
        lv_obj_t *line = Quiet(column, tokens, Fit::kWrap);
        lv_obj_set_style_text_align(line, LV_TEXT_ALIGN_RIGHT, 0);
        MarginTop(line, dt::kSpacingXs);
    }
    if (doing[0] != '\0') {
        MarginTop(Step(column, Type::kCaption, doing, widgets::MarqueeKey(kScopeHome, agent.id, 2)), dt::kSpacingSm);
    }
}

// A sub-agent: its type and its tokens side by side on one line, `spacing.sm` apart.
void SubAgentFacts(lv_obj_t *column, const Agent &agent, const char *tokens)
{
    lv_obj_t *line = Row(column);
    lv_obj_set_style_pad_column(line, dt::kSpacingSm, 0);
    MarginTop(line, dt::kSpacingMd);
    Words(line, Type::kCaption, agent.kind[0] != '\0' ? agent.kind : "Sub-agent");
    if (tokens[0] != '\0') {
        Quiet(line, tokens);
    }
}

void Home::BuildAgent(const Agent &agent)
{
    const uint32_t now = lv_tick_get();
    const uint32_t seconds = agent.since_s + lv_tick_diff(now, model::AgentReceivedMs(agent.id)) / 1000;
    char state[40];
    rules::FormatState(state, sizeof state, agent.state, agent.limit != model::Limit::kNone && agent.state == AgentState::kDone, seconds);
    chip_ = BuildChip(state, false);
    // The agent's name above the state chip, centred, `spacing.sm` over it: a session's folder or a sub-agent's task.
    lv_obj_t *name = Step(content_, Type::kTitle, agent.name, widgets::MarqueeKey(kScopeHome, agent.id, 1), widgets::MarqueeAlign::kCentre);
    lv_obj_set_width(name, dt::kSpacingContent);
    lv_obj_update_layout(content_);
    lv_obj_set_pos(name, dt::kSpacingEdge, kChipTop - dt::kSpacingSm - lv_obj_get_height(name));

    lv_obj_t *column = Column(content_, dt::kSpacingContent);
    lv_obj_set_pos(column, dt::kSpacingEdge, kFactsTop);
    char doing[model::kDetailChars + 16] = {};
    FormatDoing(doing, sizeof doing, agent);
    // A sub-agent's doing line leads its facts; a session's follows its Context window, so that window stands where the
    // overview's Usage window does and the bar never shifts from one page to the next (the person, 2026-11-26).
    if (doing[0] != '\0' && agent.parent != 0) {
        Step(column, Type::kCaption, doing, widgets::MarqueeKey(kScopeHome, agent.id, 2));
    }
    const Agent *session = agent.parent != 0 ? model::FindAgent(agent.parent) : nullptr;
    if (session != nullptr) {
        char in[model::kNameChars + 8];
        std::snprintf(in, sizeof in, "in %s", session->name);
        MarginTop(Step(column, Type::kCaption, in, widgets::MarqueeKey(kScopeHome, agent.id, 3)), dt::kSpacingXs);
    }
    char tokens[24] = {};
    if (agent.tokens > 0) {
        char short_form[12];
        rules::FormatTokens(short_form, sizeof short_form, agent.tokens);
        std::snprintf(tokens, sizeof tokens, "%s tokens", short_form);
    }
    if (agent.parent == 0) {
        SessionFacts(column, agent, tokens, doing);
    } else {
        SubAgentFacts(column, agent, tokens);
    }
    // Review: only on a Needs you page with a request of its own queued, always in the same place, centred.
    review_ = nullptr;
    control_ = nullptr;
    if (agent.state == AgentState::kNeedsYou && model::OldestFor(agent.id) != nullptr) {
        review_ = widgets::MakeButton(content_, "Review", widgets::ButtonVariant::kAction, dt::kSpacingButtonWidth);
        lv_group_remove_obj(review_);  // the page's one stop stands over it
        lv_obj_set_pos(review_, (kGlassW - dt::kSpacingButtonWidth) / 2, kReviewTop);
        lv_obj_add_event_cb(review_, OnReview, LV_EVENT_CLICKED, this);
        control_ = review_;
    }
}

// The standby page (AGENT_DECK 4.13): this deck is not the active one. The state in the chip where every page's chip stands, the mascot idling in
// the home view's place, the line under it centred in black, Connect where Review stands — outline, ringed as it appears — and the foot
// with no page dots: it is the one page.
void Home::BuildStandby()
{
    const model::StandbyPage page = model::Standby();
    chip_ = BuildChip(rules::StandbyChip(page), false);
    lv_obj_t *column = Column(content_, dt::kSpacingContent);
    lv_obj_set_pos(column, dt::kSpacingEdge, kFactsTop);
    lv_obj_t *words = Words(column, Type::kCaption, rules::StandbyWords(page), Fit::kWrap);
    lv_obj_set_style_text_align(words, LV_TEXT_ALIGN_CENTER, 0);
    connect_ = widgets::MakeButton(content_, "Connect", widgets::ButtonVariant::kOutline, dt::kSpacingButtonWidth);
    lv_group_remove_obj(connect_);  // the page's one stop stands over it
    lv_obj_set_pos(connect_, (kGlassW - dt::kSpacingButtonWidth) / 2, kReviewTop);
    lv_obj_add_event_cb(connect_, OnConnect, LV_EVENT_CLICKED, nullptr);
    control_ = nullptr;
    if (page == model::StandbyPage::kConnecting) {
        lv_obj_add_state(connect_, LV_STATE_DISABLED);  // waiting: the chip and the line say what is happening
    } else {
        control_ = connect_;
    }
}

// The page's one stop: over its control when it has one, so the ring is the control's and the first press only shows it; else an
// unseen stop over the page's facts, which draws no ring — OK there presses nothing and the page dots say where the person is — and
// whose first UP or DOWN turns the page at once (DESIGN → Layout → Paging; the person, 2026-11-26).
void Home::PlaceStop(lv_obj_t *over)
{
    lv_obj_set_style_outline_opa(stop_, over != nullptr ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    router::SetFirstPressActs(stop_, over == nullptr);
    lv_obj_update_layout(page_);
    if (over != nullptr) {
        lv_area_t a;
        lv_obj_get_coords(over, &a);
        lv_obj_set_pos(stop_, a.x1, a.y1);
        lv_obj_set_size(stop_, lv_area_get_width(&a), lv_area_get_height(&a));
    } else {
        // The facts' block: from a line over the first, `spacing.edge` in from each side, to `spacing.md` over the page dots.
        const int32_t top = kChipTop - dt::kSpacingXl - dt::kSpacingSm;
        lv_obj_set_pos(stop_, dt::kSpacingEdge, top);
        lv_obj_set_size(stop_, dt::kSpacingContent, kDotsTop - dt::kSpacingMd - top);
    }
}

// The page dots, made again with every page and every record, never flipped in place: a pill's width follows its state, and a state changed
// before the dots' first frame is not laid out (the kit's page dots, widgets/page_dots.h).
void Home::UpdateDots()
{
    const int pages = Pages();
    if (dots_ != nullptr) {
        lv_obj_delete(dots_);
    }
    dots_ = widgets::MakePageDots(page_, pages, current_);
    dots_count_ = pages;
    lv_obj_set_ignore_layout(dots_, true);
    lv_obj_update_layout(page_);
    lv_obj_set_pos(dots_, (kGlassW - lv_obj_get_width(dots_)) / 2, kDotsTop);
    lv_obj_set_hidden(dots_, pages < 2);  // a lone dot says nothing
}

void Home::UpdateLink()
{
    widgets::SetLinkIndicator(link_, model::LinkUp());
}

// Which move the page's mascot plays, and how it gets there: a page turned to starts it from the beginning; a record on the page on view
// asks for it, the one playing finishing first; a reaction plays once between.
void Home::UpdateMascot(bool turned)
{
    MascotMove move = MascotMove::kIdle;
    bool previous = false;
    if (standby_) {
        move = MascotMove::kIdleInPlace;  // the idle kept in its column, silent: no walk and no skip across Connect
    } else if (const Agent *agent = CurrentAgent()) {
        move = rules::AgentMove(agent->state, agent->background, agent->limit != model::Limit::kNone, (lv_tick_get() / 1000) % 2 != 0);
        previous = agent->background;
    } else {
        int needs = 0, working = 0;
        for (size_t i = 0; i < model::AgentCount(); ++i) {
            needs += model::AgentAt(i)->state == AgentState::kNeedsYou;
            working += model::AgentAt(i)->state == AgentState::kWorking;
        }
        move = rules::OverviewMove(needs, working);
    }
    stage_.SetPrevious(previous);
    stage_.SetHeard(model::SoundOn() && !standby_);
    if (turned || !desired_set_) {
        stage_.Play(move);
    } else if (has_reaction_) {
        stage_.React(reaction_, move);
    } else if (move != desired_) {
        stage_.Ask(move);
    }
    desired_ = move;
    desired_set_ = true;
    has_reaction_ = false;
}

// The pages: the overview, then the agents in the deck's order; the page on view stays on its agent when others come and go. On standby
// there is the one. Returns `turned`, or true when the agent on view left and the page that took its place is on view.
bool Home::OrderPages(bool turned)
{
    std::array<uint16_t, model::kMaxAgents> ids{}, parents{};
    const size_t n = model::AgentCount();
    for (size_t i = 0; i < n; ++i) {
        ids[i] = model::AgentAt(i)->id;
        parents[i] = model::AgentAt(i)->parent;
    }
    pages_count_ = standby_ ? 0 : rules::PageOrder(ids.data(), parents.data(), n, pages_.data());
    int found = -1;
    for (size_t i = 0; i < pages_count_; ++i) {
        if (pages_[i] == current_id_) {
            found = static_cast<int>(i) + 1;
        }
    }
    if (standby_) {
        current_ = 0;
        current_id_ = 0;
    } else if (current_id_ != 0 && found < 0) {
        current_ = std::min(current_, Pages() - 1);  // the agent left: the page that took its place, or the last
        current_id_ = current_ > 0 ? pages_[static_cast<size_t>(current_) - 1] : 0;
        turned = true;
    } else if (found >= 0) {
        current_ = found;
    } else {
        current_ = 0;
    }
    return turned;
}

// Everything the page says, from the model: the page's own objects built again, the dots, the stop, the mascot's move.
void Home::Refresh(bool turned)
{
    if (!carousel_) {
        return;
    }
    standby_ = model::Standby() != model::StandbyPage::kNone;
    turned = OrderPages(turned);
    lv_obj_clean(content_);
    chip_ = chip_label_ = review_ = connect_ = control_ = nullptr;
    if (standby_) {
        BuildStandby();
    } else if (const Agent *agent = CurrentAgent()) {
        BuildAgent(*agent);
    } else {
        BuildOverview();
    }
    UpdateDots();
    UpdateLink();
    PlaceStop(control_);
    stage_.ToFront();
    UpdateMascot(turned);
    clock_.Update();
    // A control rings as it appears: a page turned to, a request arriving. The key is which control, so the same one on the next record does not ring again.
    const uint32_t key = control_ == nullptr ? 0 : standby_ ? 0x30000u : (current_id_ == 0 ? 0x10000u : 0x20000u + current_id_);
    if (key != 0 && key != control_key_ && active_ && !covered_) {
        // A finger that turned the page shows no ring (DESIGN → Focus and input: the ring is the buttons'); a page the keys turned, or a
        // request arriving by itself, rings its control as it appears.
        lv_indev_t *indev = lv_indev_active();
        if (indev != nullptr && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            router::Focus(stop_);
        } else {
            router::Ring(stop_);
        }
    }
    if (standby_ && key == 0 && control_key_ != 0) {
        router::ClearRing();  // Connect is waiting: nothing to ring, and the page's own ring stays off
    }
    control_key_ = key;
}

// --- Turning pages -----------------------------------------------------------------------------------------------------------

void Home::TurnTo(int page)
{
    page = std::clamp(page, 0, Pages() - 1);
    current_ = page;
    current_id_ = page == 0 ? 0 : pages_[static_cast<size_t>(page) - 1];
    Refresh(true);
}

// The walk's step onto a guard: a page turn from the stop, or on to the foot, or back from it. `dir` is -1 for the guard before the stop
// (UP), +1 for the one after it (DOWN).
void Home::WalkTo(int dir)
{
    const bool from_foot = last_defocused_ == wifi_ || last_defocused_ == sound_ || last_defocused_ == settings_ || last_defocused_ == power_;
    if (from_foot) {
        // Up from the first icon reaches the guard after the stop: the last page; down from the last icon, the guard before it: the first.
        router::Focus(stop_);
        TurnTo(dir > 0 ? Pages() - 1 : 0);
        return;
    }
    const int next = current_ + dir;
    if (next < 0) {
        router::Focus(power_);
    } else if (next >= Pages()) {
        router::Focus(wifi_);
    } else {
        router::Focus(stop_);  // first: a control that appears on the page turned to is then ringed (Refresh), whatever the input
        TurnTo(next);
    }
}

void Home::OnGuard(lv_event_t *e)
{
    Home *home = static_cast<Home *>(lv_event_get_user_data(e));
    home->WalkTo(lv_event_get_target_obj(e) == home->guard_prev_ ? -1 : 1);
}

// Which stop the walk left last, so a guard knows whether the step onto it came from the page's stop or round from the foot.
void Home::OnDefocused(lv_event_t *e)
{
    static_cast<Home *>(lv_event_get_user_data(e))->last_defocused_ = lv_event_get_target_obj(e);
}

// OK on the page's stop presses its control, as a tap on it does; a page with none ticks and nothing more.
void Home::OnStopClicked(lv_event_t *e)
{
    Home *home = static_cast<Home *>(lv_event_get_user_data(e));
    if (home->control_ != nullptr) {
        lv_obj_send_event(home->control_, LV_EVENT_CLICKED, nullptr);
    }
}

// A swipe left turns to the next page, right to the one before; at either end nothing wraps, and `sound.at-end` says there is no page.
void Home::OnSwipe(int32_t dir, void *user)
{
    Home *home = static_cast<Home *>(user);
    const int next = home->current_ + (dir > 0 ? 1 : -1);
    if (next < 0 || next >= home->Pages()) {
        lvgl_display::PlayCue(lvgl_display::Cue::kAtEnd);
        return;
    }
    home->TurnTo(next);
}

void Home::OnOpenWaiting(lv_event_t *)
{
    flow::OpenWaiting();
}

void Home::OnReview(lv_event_t *e)
{
    const Home *home = static_cast<Home *>(lv_event_get_user_data(e));
    flow::OpenOldestOf(home->current_id_, false);
}

// Connect: asked on LVGL's next pass, since the page is rebuilt as the state changes and Connect's own click must not delete Connect.
void Home::OnConnect(lv_event_t *)
{
    lv_async_call(ConnectNow, nullptr);
}

void Home::ConnectNow(void *)
{
    model::ConnectPressed();
}

// The foot's icons open the Wi-Fi page and the settings (a press does nothing where a route is not registered); Power turns the deck off.
void Home::OnWifi(lv_event_t *)
{
    if (router::FindRoute(flow::kRouteWifi) != nullptr) {
        router::PushRoute(flow::kRouteWifi);
    }
}

void Home::OnSettings(lv_event_t *)
{
    if (router::FindRoute(flow::kRouteSettings) != nullptr) {
        router::PushRoute(flow::kRouteSettings);
    }
}

// Power turns the deck off at once, with no confirmation (the person, 2026-11-26): the frame the glass keeps, then the board's power-off, from a
// timer and not this click (power_leave).
void Home::OnPower(lv_event_t *)
{
    power_leave::LeaveSoon(power_leave::How::kPowerOff, BuildOffFrame);
}

void Home::OnSound(lv_event_t *e)
{
    const Home *home = static_cast<Home *>(lv_event_get_user_data(e));
    model::SetSoundOn(lv_obj_has_state(home->sound_, LV_STATE_CHECKED));
}

// --- What the model says -----------------------------------------------------------------------------------------------------------

void Home::OnModel(const model::Change &change, void *user)
{
    static_cast<Home *>(user)->Heard(change);
}

// A record, as the page on view takes it. The moves that answer an event — a skip for an agent that joined, a celebrate for a session done, a
// dust for a sub-agent done, a spin at half the allowance — play where the person is looking: the overview or the agent's own page.
void Home::Heard(const model::Change &change)
{
    if (!carousel_ || !active_) {
        return;
    }
    switch (change.event) {
    case model::Event::kAgent: {
        const Agent *agent = model::FindAgent(static_cast<uint16_t>(change.id));
        if (agent != nullptr && current_id_ == 0 && change.added && model::AgentCount() > 1) {
            reaction_ = MascotMove::kSkip;
            has_reaction_ = true;
        } else if (agent != nullptr && current_id_ == 0 && agent->parent == 0 && change.was != AgentState::kDone &&
                   agent->state == AgentState::kDone && agent->limit == model::Limit::kNone) {
            reaction_ = MascotMove::kCelebrate;
            has_reaction_ = true;
        } else if (agent != nullptr && agent->parent != 0 && agent->parent == current_id_ && change.was != AgentState::kDone &&
                   agent->state == AgentState::kDone) {
            reaction_ = MascotMove::kDust;
            has_reaction_ = true;
        }
        break;
    }
    case model::Event::kUsage: {
        const int five = model::UsageKnown() ? model::GetUsage().five_hour_pct : -1;
        if (current_id_ == 0 && last_five_ >= 0 && last_five_ < 50 && five >= 50 && model::GetUsage().limit == model::Limit::kNone) {
            reaction_ = MascotMove::kSpin;
            has_reaction_ = true;
        }
        last_five_ = five;
        break;
    }
    case model::Event::kSound:
        stage_.SetHeard(model::SoundOn() && !standby_);
        lv_obj_set_state(sound_, LV_STATE_CHECKED, model::SoundOn());
        return;
    case model::Event::kStandby:
        // The page changes in place: a deck put on standby or made active starts its mascot's move over; a Connect or its timeout only
        // changes the chip, the line and the button, and rings Connect again as it comes back.
        {
            const bool now = model::Standby() != model::StandbyPage::kNone;
            if (now != standby_) {
                control_key_ = 0;  // put on standby or made active: Connect, or the overview's control, rings as it appears
            }
            Refresh(now != standby_);
        }
        return;
    case model::Event::kSettings:
    case model::Event::kBattery:
    case model::Event::kWifi:
        return;  // the settings' and the Wi-Fi page's, not the carousel's
    default:
        break;
    }
    Refresh(false);
}

// Every DESIGN `motion.self-update`: the state's time counts on, and the clock follows the minute.
void Home::OnTick(lv_timer_t *t)
{
    Home *home = static_cast<Home *>(lv_timer_get_user_data(t));
    const Agent *agent = home->CurrentAgent();
    if (agent != nullptr && home->chip_label_ != nullptr) {
        const uint32_t seconds = agent->since_s + lv_tick_diff(lv_tick_get(), model::AgentReceivedMs(agent->id)) / 1000;
        char state[40];
        rules::FormatState(state, sizeof state, agent->state, agent->limit != model::Limit::kNone && agent->state == AgentState::kDone,
                           seconds);
        if (std::strcmp(lv_label_get_text(home->chip_label_), state) != 0) {
            lv_label_set_text(home->chip_label_, state);
        }
    }
    model::StandbyTick();  // Connect that ran out of time: the page says the Mac did not answer
    home->clock_.Update();
}

// --- The lifecycle -----------------------------------------------------------------------------------------------------------------

void Home::Enter()
{
    active_ = true;
    covered_ = false;
    if (!carousel_) {
        // The startup screen, for what is left of its hold; its mascot waves. Its first frame is a full refresh, which holds the glass
        // about 1.6 s on a V1.0: the hello and the hold begin once that frame is drawn, so the whole hello reaches the glass.
        stage_.Start();
        stage_.SetHeard(model::SoundOn());
        if (splash_drawn_) {
            BeginWave();
        } else {
            lv_display_add_event_cb(lv_obj_get_display(root_), OnSplashDrawn, LV_EVENT_REFR_READY, nullptr);
        }
        return;
    }
    Activate();
}

void Home::Exit()
{
    active_ = false;
    if (hold_ != nullptr) {
        // What is left of the hold, kept for the next time the startup screen is on top.
        const uint32_t spent = lv_tick_diff(lv_tick_get(), hold_began_);
        hold_left_ms_ = spent >= hold_left_ms_ ? 1 : hold_left_ms_ - spent;
        lv_timer_delete(hold_);
        hold_ = nullptr;
    }
    if (tick_ != nullptr) {
        lv_timer_delete(tick_);
        tick_ = nullptr;
    }
    if (subscription_ != 0) {
        model::Unsubscribe(subscription_);
        subscription_ = 0;
    }
    stage_.Stop();
}

void Home::Covered(bool covered)
{
    covered_ = covered;
    if (covered) {
        stage_.Stop();
    } else {
        stage_.Start();
        control_key_ = 0;  // uncovered: a control rings as it appears
        Refresh(false);
    }
}

Probe Home::Look() const
{
    Probe p;
    p.carousel = carousel_;
    p.page = current_;
    p.pages = carousel_ ? Pages() : 0;
    p.page_object = page_;
    p.stop = stop_;
    p.chip = chip_;
    p.review = review_;
    p.wifi = wifi_;
    p.sound = sound_;
    p.settings = settings_;
    p.power = power_;
    p.link = link_;
    p.dots = dots_;
    p.clock = clock_.Label();
    p.mascot = stage_.Built() ? stage_.Rig().root : nullptr;
    return p;
}

}  // namespace

void Create(lv_obj_t *root, lv_group_t *group)
{
    s_home.~Home();
    new (&s_home) Home();
    s_home.Create(root, group);
}
void Enter() { s_home.Enter(); }
void Exit() { s_home.Exit(); }
void Covered(bool covered) { s_home.Covered(covered); }
Probe Look() { return s_home.Look(); }
void HandOver() { s_home.HandOver(); }

}  // namespace agent_deck::home
