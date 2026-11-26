#include "closed_page.h"

#include "deck_ui.h"
#include "design_tokens.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/widgets.h"

namespace agent_deck {
namespace {

using namespace layout;

}  // namespace

uint32_t ClosedPage::HoldMs() const
{
    return answered_ ? design_tokens::kMotionAnsweredHoldDelayMs : design_tokens::kMotionTimedOutHoldDelayMs;
}

void ClosedPage::Build(lv_obj_t *parent, bool answered, LeaveFn leave, void *user)
{
    answered_ = answered;
    leave_ = leave;
    user_ = user;
    root_ = Bare(parent);
    lv_obj_set_ignore_layout(root_, true);
    lv_obj_set_size(root_, kGlassW, kGlassH);
    lv_obj_set_pos(root_, 0, 0);
    stage_.Build(root_, kMascotX, kMascotY, kMascotCell);
    stage_.SetHeard(true);
    lv_obj_t *column = Column(root_, dt::kSpacingContent);
    lv_obj_set_pos(column, dt::kSpacingEdge, kMascotY + kMascotH + dt::kSpacingLg);
    if (answered) {
        widgets::MakeStackedStatusBlock(column, "Answered", "in the terminal", widgets::StatusMark::kCheck);
    } else {
        widgets::MakeStackedStatusBlock(column, "Timed out", "before an answer came", widgets::StatusMark::kClock);
    }
    stage_.ToFront();
}

// The spin begins so that it ends on the rest pose as the page leaves: the hold less the move's own length, the deck's half-second of cue.
void ClosedPage::OnBegin(lv_timer_t *t)
{
    ClosedPage *page = static_cast<ClosedPage *>(lv_timer_get_user_data(t));
    page->begin_ = nullptr;
    lv_timer_delete(t);
    page->stage_.Play(widgets::MascotMove::kSpin);
}

void ClosedPage::OnHold(lv_timer_t *t)
{
    ClosedPage *page = static_cast<ClosedPage *>(lv_timer_get_user_data(t));
    page->hold_ = nullptr;
    lv_timer_delete(t);
    if (page->leave_ != nullptr) {
        page->leave_(page->user_);
    }
}

void ClosedPage::Start()
{
    if (root_ == nullptr) {
        return;
    }
    started_ = true;
    stage_.Start();
    const uint32_t hold = HoldMs();
    if (answered_) {
        const uint32_t spin = widgets::mascot_stage_run(widgets::MascotMove::kSpin).pass_ms;
        const uint32_t wait = hold > spin ? hold - spin : 1;
        begin_ = lv_timer_create(OnBegin, wait, this);
        lv_timer_set_repeat_count(begin_, 1);
    } else {
        // The look-around from its start, asked to rest so the eyes are home as the page leaves.
        stage_.Look();
        stage_.RestAfter(hold > widgets::kMascotLookSettleMs ? hold - widgets::kMascotLookSettleMs : 1);
    }
    hold_ = lv_timer_create(OnHold, hold, this);
    lv_timer_set_repeat_count(hold_, 1);
}

void ClosedPage::Stop()
{
    if (hold_ != nullptr) {
        lv_timer_delete(hold_);
        hold_ = nullptr;
    }
    if (begin_ != nullptr) {
        lv_timer_delete(begin_);
        begin_ = nullptr;
    }
    stage_.Stop();
    started_ = false;
}

namespace {

class Closed final : public router::Screen {
public:
    explicit Closed(bool answered) : answered_(answered) {}

    void OnCreate(lv_obj_t *root, lv_group_t *) override
    {
        ui::StyleScreen(root);
        page_.Build(root, answered_, OnLeave, nullptr);
    }
    void OnEnter() override { page_.Start(); }
    void OnExit() override { page_.Stop(); }

private:
    // The page's own leave: back to the screen it covers.
    static void OnLeave(void *) { router::Pop(); }

    bool answered_;
    ClosedPage page_;
};

}  // namespace

std::unique_ptr<router::Screen> ClosedScreen(bool answered)
{
    return std::make_unique<Closed>(answered);
}

}  // namespace agent_deck
