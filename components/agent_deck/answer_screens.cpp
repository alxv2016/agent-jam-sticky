#include "answer_screens.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include "closed_page.h"
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

// The body's cut notice, the last line of a body the bridge cut.
constexpr const char *kCutNotice = "\n... Read the full text in the terminal";
// A text view's line is `caption-loose`'s 30 px, and its words stand `spacing.sm` inside it above and below.
constexpr int32_t kViewLine = dt::kTypographyCaptionLoose.line_px;
constexpr int32_t kViewInset = 2 * dt::kSpacingSm;

// What the request and the question share: the heading, the clock, the closing in place and where each leaves to.
class Answer : public router::Screen {
public:
    Answer(uint32_t id, bool from_list) : id_(id), from_list_(from_list) {}

    void OnCreate(lv_obj_t *root, lv_group_t *group) override
    {
        group_ = group;
        root_ = root;
        ui::StyleScreen(root);
        const model::Item *item = model::FindItem(id_);
        agent_ = item != nullptr ? item->agent : 0;
        page_ = Bare(root);
        lv_obj_set_ignore_layout(page_, true);
        lv_obj_set_size(page_, kGlassW, kGlassH);
        lv_obj_set_pos(page_, 0, 0);
        ui::LetRingOut(page_);
        clock_.Build(page_);
        if (item != nullptr) {
            BuildHeading(*item);
            BuildBody(*item);
        }
        router::AttachEscape(OnEscape, this);  // holding OK is Dismiss
    }

    void OnEnter() override
    {
        subscription_ = model::Subscribe(OnModel, this);
        tick_ = lv_timer_create(OnTick, design_tokens::kMotionSelfUpdateDelayMs, this);
        if (closed_) {
            closed_->Start();
        }
        clock_.Update();
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
        if (closed_) {
            closed_->Stop();
        }
    }

protected:
    // Dismiss: a request stays waiting, a question is left to the terminal; then where it leaves to.
    virtual void Dismiss() = 0;
    virtual void BuildBody(const model::Item &item) = 0;

    // The deck's left column on top: the tool or header, then the agent and its place in the queue on one line, `spacing.md` apart, the count in black.
    void BuildHeading(const model::Item &item)
    {
        heading_ = Column(page_, dt::kSpacingContent);
        lv_obj_set_pos(heading_, dt::kSpacingEdge, kHeadingTop);
        Step(heading_, Type::kTitle, item.tool, widgets::MarqueeKey(kScopeRequest, item.id, 1));
        lv_obj_t *line = Row(heading_);
        lv_obj_set_style_pad_column(line, dt::kSpacingMd, 0);
        MarginTop(line, -dt::kSpacingXs);
        const model::Agent *agent = model::FindAgent(item.agent);
        Quiet(line, agent != nullptr ? agent->name : "");
        if (model::QueueCount() > 1) {
            char count[16];
            std::snprintf(count, sizeof count, "%u of %u", static_cast<unsigned>(model::QueuePosition(item.id)),
                          static_cast<unsigned>(model::QueueCount()));
            Words(line, Type::kCaption, count);
        }
        lv_obj_update_layout(page_);
    }

    // The answer taken or dismissed: leaves where AGENT_DECK 4.7 says.
    void Done(bool allowed, size_t others)
    {
        flow::Leave(agent_, from_list_, others, allowed);
    }

    // The request closed elsewhere, on view: the closed page takes this screen's place, as one change of the glass.
    void ShowClosed(bool answered)
    {
        if (closed_) {
            return;
        }
        lv_obj_delete(page_);  // every control goes with it, out of the group: the page has none, and a press does nothing meanwhile
        page_ = nullptr;
        heading_ = nullptr;
        clock_ = Clock();  // the clock went with the page: the closed page carries none
        if (tick_ != nullptr) {
            lv_timer_delete(tick_);
            tick_ = nullptr;
        }
        closed_ = std::make_unique<ClosedPage>();
        closed_->Build(root_, answered, OnClosedLeave, this);
        closed_->Start();
    }

    static void OnClosedLeave(void *user)
    {
        Answer *self = static_cast<Answer *>(user);
        self->Done(false, model::QueueCount());  // a close in the terminal sets no allow
    }

    static void OnStandbyLeave(void *) { router::PopToRoot(); }

    static void OnModel(const model::Change &change, void *user)
    {
        Answer *self = static_cast<Answer *>(user);
        if (change.event == model::Event::kRequestClosed && change.id == self->id_) {
            if (change.reason == model::CloseReason::kStandby) {
                // The deck went on standby: no closed page and no cue; home shows the standby page once the screens above it are gone.
                flow::ClearReturn();
                lv_async_call(OnStandbyLeave, nullptr);
                return;
            }
            self->ShowClosed(change.reason == model::CloseReason::kAnsweredElsewhere);
        }
    }

    static void OnTick(lv_timer_t *t)
    {
        static_cast<Answer *>(lv_timer_get_user_data(t))->clock_.Update();
    }

    static void OnEscape(void *user)
    {
        Answer *self = static_cast<Answer *>(user);
        if (!self->closed_ && model::FindItem(self->id_) != nullptr) {
            self->Dismiss();
        }
    }

    uint32_t id_;
    bool from_list_;
    uint16_t agent_ = 0;
    lv_group_t *group_ = nullptr;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *page_ = nullptr;
    lv_obj_t *heading_ = nullptr;
    Clock clock_;
    int subscription_ = 0;
    lv_timer_t *tick_ = nullptr;
    std::unique_ptr<ClosedPage> closed_;
};

// --- A permission request ---------------------------------------------------------------------------------------------------------

class Request final : public Answer {
public:
    using Answer::Answer;

protected:
    // The body in the kit's text view between the heading and the answers, a page of whole lines at a time, its scroll indicator in the glass's
    // right margin; Deny and Allow in two halves, `spacing.md` apart. It opens with no ring, and the first press rings Deny.
    void BuildBody(const model::Item &item) override
    {
        const int32_t top = lv_obj_get_y(heading_) + lv_obj_get_height(heading_) + dt::kSpacingMd;
        const int32_t room = kAnswersTop - dt::kSpacingLg - top;
        const int32_t lines = (room - kViewInset) / kViewLine;
        lv_obj_t *holder = Bare(page_);
        lv_obj_set_size(holder, kGlassW, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(holder, dt::kSpacingEdge, 0);
        lv_obj_set_flex_flow(holder, LV_FLEX_FLOW_COLUMN);
        ui::LetRingOut(holder);
        lv_obj_set_pos(holder, 0, top);
        std::string body = item.body;
        if (item.cut) {
            body += kCutNotice;
        }
        view_ = widgets::MakeTextView(holder, lines, body.c_str());
        deny_ = widgets::MakeButton(page_, "Deny", widgets::ButtonVariant::kTonal, kHalfW);
        lv_obj_set_pos(deny_, dt::kSpacingEdge, kAnswersTop);
        lv_obj_add_event_cb(deny_, OnDeny, LV_EVENT_CLICKED, this);
        allow_ = widgets::MakeButton(page_, "Allow", widgets::ButtonVariant::kAction, kHalfW);
        lv_obj_set_pos(allow_, dt::kSpacingEdge + kHalfW + dt::kSpacingMd, kAnswersTop);
        lv_obj_add_event_cb(allow_, OnAllow, LV_EVENT_CLICKED, this);
        lv_group_focus_obj(deny_);  // the first press rings the first answer
    }

    void Dismiss() override
    {
        Done(false, model::QueueCount() - 1);  // it waits on, for Review, the count or the list
    }

private:
    void Give(bool allow)
    {
        model::Answer(id_, allow);
        Done(allow, model::QueueCount());
    }
    static void OnDeny(lv_event_t *e) { static_cast<Request *>(lv_event_get_user_data(e))->Give(false); }
    static void OnAllow(lv_event_t *e) { static_cast<Request *>(lv_event_get_user_data(e))->Give(true); }

    widgets::TextView view_ = {};
    lv_obj_t *deny_ = nullptr;
    lv_obj_t *allow_ = nullptr;
};

// --- A question ----------------------------------------------------------------------------------------------------------------------

class Question final : public Answer {
public:
    using Answer::Answer;

protected:
    // The question in `caption`, then each answer a choice card — a ring for one, a box for several — whose name comes down to `caption-strong`
    // since a question can be long. The question and its cards turn by page together, whole items a page, when they run past the room; the
    // scroll indicator stands in the right margin. Dismiss and Send in two halves; Send waits for a pick.
    void BuildBody(const model::Item &item) override
    {
        multi_ = item.multi;
        const int32_t band_top = lv_obj_get_y(heading_) + lv_obj_get_height(heading_) + dt::kSpacingLg;
        band_h_ = kAnswersTop - dt::kSpacingLg - band_top;
        // The scroll indicator's lane stands in the glass's right margin on a host of its own, the band's full height whatever page shows.
        lane_ = Bare(page_);
        lv_obj_set_size(lane_, kGlassW, band_h_);
        lv_obj_set_pos(lane_, 0, band_top);
        lv_obj_set_style_pad_hor(lane_, dt::kSpacingEdge, 0);  // the lane stands in the right margin, as the content column's right edge
        ui::LetRingOut(lane_);
        // The band the question and its cards stand in: a vertical swipe turns a page, and a page is scrolled to, never by a pixel.
        host_ = widgets::MakeSwipePage(page_, kGlassW, band_h_, OnSwipe, this, widgets::SwipeAxis::kVertical);
        lv_obj_set_pos(host_, 0, band_top);
        lv_obj_set_style_pad_hor(host_, dt::kSpacingEdge, 0);
        items_ = Column(host_, dt::kSpacingContent);
        lv_obj_set_style_pad_row(items_, dt::kSpacingMd, 0);
        lv_obj_set_style_pad_bottom(items_, band_h_, 0);  // the last page has room to stand at its own top
        list_[count_++] = Words(items_, Type::kCaption, item.question.c_str(), Fit::kWrap);
        for (size_t i = 0; i < item.option_count; ++i) {
            // `label: description`, split at the first `: `; an option with none is one plain line.
            const std::string &option = item.options[i];
            const size_t colon = option.find(": ");
            const std::string label = colon == std::string::npos ? option : option.substr(0, colon);
            const std::string description = colon == std::string::npos ? "" : option.substr(colon + 2);
            lv_obj_t *card = widgets::MakeChoiceCard(items_, label.c_str(), item.multi, description.empty() ? nullptr : description.c_str());
            widgets::SetWordsRole(card, Type::kCaptionStrong);
            lv_obj_add_event_cb(card, OnPick, LV_EVENT_VALUE_CHANGED, this);
            cards_[i] = card;
            list_[count_++] = card;
        }
        card_count_ = item.option_count;
        dismiss_ = widgets::MakeButton(page_, "Dismiss", widgets::ButtonVariant::kOutline, kHalfW);
        lv_obj_set_pos(dismiss_, dt::kSpacingEdge, kAnswersTop);
        lv_obj_add_event_cb(dismiss_, OnDismiss, LV_EVENT_CLICKED, this);
        send_ = widgets::MakeButton(page_, "Send", widgets::ButtonVariant::kAction, kHalfW);
        lv_obj_set_pos(send_, dt::kSpacingEdge + kHalfW + dt::kSpacingMd, kAnswersTop);
        lv_obj_add_state(send_, LV_STATE_DISABLED);
        lv_obj_add_event_cb(send_, OnSend, LV_EVENT_CLICKED, this);
        Page();
    }

    // A question is left to the terminal, which asks it too; it does not come back.
    void Dismiss() override
    {
        model::AnswerQuestion(id_, part(), 0);
        Done(false, model::QueueCount());
    }

private:
    uint8_t part() const
    {
        const model::Item *item = model::FindItem(id_);
        return item != nullptr ? item->part : 0;
    }

    // Whole items a page: an item that would cross the band's foot starts the next page. The pages and what stands on each are worked out
    // once from the items' heights; a card turns its page when it takes the focus, and a swipe up or down turns one.
    void Page()
    {
        lv_obj_update_layout(page_);
        const int32_t reach = dt::kFocusRingReach;
        int32_t y = 0;
        int page = 0;
        int32_t page_top = 0;
        for (size_t i = 0; i < count_; ++i) {
            const int32_t h = lv_obj_get_height(list_[i]);
            const int32_t item_top = lv_obj_get_y(list_[i]);
            if (i > 0 && item_top + h - page_top > band_h_ - reach) {
                ++page;
                page_top = item_top;
                y = 0;
            }
            page_of_[i] = page;
            if (y == 0) {
                page_y_[static_cast<size_t>(page)] = item_top;
            }
            page_end_[static_cast<size_t>(page)] = item_top + h;
            y += h;
        }
        pages_ = page + 1;
        content_h_ = 0;
        for (size_t i = 0; i < count_; ++i) {
            content_h_ += lv_obj_get_height(list_[i]) + dt::kSpacingMd;
        }
        indicator_ = widgets::MakeScrollIndicator(lane_, band_h_);
        for (size_t i = 0; i < card_count_; ++i) {
            ui::ShowWhenFocused(cards_[i], host_);  // the page brings the band into view, never the card by a pixel
            lv_obj_add_event_cb(cards_[i], OnCardFocused, LV_EVENT_FOCUSED, this);
        }
        Show(0);
    }

    void Show(int page)
    {
        page = std::clamp(page, 0, pages_ - 1);
        shown_ = page;
        const size_t p = static_cast<size_t>(page);
        // The band is cut to this page: its top at the first item's, its foot at the last item's, so the next page's first item never shows in
        // what is left; a ring reaches `LetRingOut`'s room past the band, and the cards stand `spacing.md` apart, more than that.
        const int32_t top = page_y_[p];
        const int32_t height = page_end_[p] - top;
        lv_obj_set_height(host_, pages_ > 1 ? std::min(height, band_h_) : band_h_);
        lv_obj_scroll_to_y(host_, top, LV_ANIM_OFF);
        widgets::UpdateScrollIndicator(indicator_, page, pages_, band_h_, content_h_);
    }

    // A swipe up turns to the next page, down to the one before; at either end nothing wraps, and `sound.at-end` says there is no page.
    static void OnSwipe(int32_t dir, void *user)
    {
        Question *self = static_cast<Question *>(user);
        const int next = self->shown_ + (dir > 0 ? 1 : -1);
        if (next < 0 || next >= self->pages_) {
            lvgl_display::PlayCue(lvgl_display::Cue::kAtEnd);
            return;
        }
        self->Show(next);
    }

    static void OnCardFocused(lv_event_t *e)
    {
        Question *self = static_cast<Question *>(lv_event_get_user_data(e));
        const lv_obj_t *card = lv_event_get_target_obj(e);
        for (size_t i = 0; i < self->count_; ++i) {
            if (self->list_[i] == card && self->page_of_[i] != self->shown_) {
                self->Show(self->page_of_[i]);
            }
        }
    }

    // A pick is not the answer; Send is. Send is on offer once any card is chosen.
    static void OnPick(lv_event_t *e)
    {
        Question *self = static_cast<Question *>(lv_event_get_user_data(e));
        lv_obj_set_state(self->send_, LV_STATE_DISABLED, self->Chosen() == 0);
    }

    uint8_t Chosen() const
    {
        uint8_t mask = 0;
        for (size_t i = 0; i < card_count_; ++i) {
            mask = static_cast<uint8_t>(mask | (widgets::ChoiceChosen(cards_[i]) ? 1u << i : 0u));
        }
        return mask;
    }

    static void OnDismiss(lv_event_t *e) { static_cast<Question *>(lv_event_get_user_data(e))->Dismiss(); }
    static void OnSend(lv_event_t *e)
    {
        Question *self = static_cast<Question *>(lv_event_get_user_data(e));
        const uint8_t chosen = self->Chosen();
        if (chosen == 0) {
            return;
        }
        model::AnswerQuestion(self->id_, self->part(), chosen);
        self->Done(false, model::QueueCount());
    }

    bool multi_ = false;
    int32_t band_h_ = 0;
    lv_obj_t *host_ = nullptr;
    lv_obj_t *items_ = nullptr;
    lv_obj_t *lane_ = nullptr;
    int32_t content_h_ = 0;
    widgets::ScrollIndicator indicator_ = {};
    std::array<lv_obj_t *, model::kMaxOptions + 1> list_{};  // the question, then each card
    std::array<lv_obj_t *, model::kMaxOptions> cards_{};
    std::array<int, model::kMaxOptions + 1> page_of_{};
    std::array<int32_t, model::kMaxOptions + 2> page_y_{};
    std::array<int32_t, model::kMaxOptions + 2> page_end_{};
    size_t count_ = 0;
    size_t card_count_ = 0;
    int pages_ = 1;
    int shown_ = 0;
    lv_obj_t *dismiss_ = nullptr;
    lv_obj_t *send_ = nullptr;
};

template <class T>
std::unique_ptr<router::Screen> Make(const char *param, bool want_question)
{
    uint32_t id = 0;
    bool from_list = false;
    if (!flow::ParseParam(param, &id, &from_list)) {
        return nullptr;
    }
    const model::Item *item = model::FindItem(id);
    if (item == nullptr || item->is_question != want_question) {
        return nullptr;
    }
    return std::make_unique<T>(id, from_list);
}

}  // namespace

std::unique_ptr<router::Screen> RequestScreen(const char *param)
{
    return Make<Request>(param, false);
}

std::unique_ptr<router::Screen> QuestionScreen(const char *param)
{
    return Make<Question>(param, true);
}

}  // namespace agent_deck
