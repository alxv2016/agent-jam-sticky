// A request closed elsewhere (AGENT_DECK 4.3): the mascot in the home view's place — the spin for an answer given in the terminal, the look-around
// for a request that timed out — and under it the stacked status block, "Answered" in the terminal or "Timed out" before an answer came.
// No control: a tap and a press do nothing, and the page leaves by itself after `motion.answered-hold` or `motion.timed-out-hold`
// (DESIGN → Exceptions → Timed leaves). It is built into a page container, so a request screen shows it in its own place and the route
// `deck.closed` shows it alone.

#pragma once

#include <memory>

#include "deck_stage.h"
#include "lvgl.h"
#include "router/screen.h"

namespace agent_deck {

class ClosedPage {
public:
    using LeaveFn = void (*)(void *user);

    ClosedPage() = default;
    ClosedPage(const ClosedPage &) = delete;
    ClosedPage &operator=(const ClosedPage &) = delete;
    ~ClosedPage() { Stop(); }

    // Builds the page into `parent`, which it fills. `leave` runs under the lock when the hold ends.
    void Build(lv_obj_t *parent, bool answered, LeaveFn leave, void *user);
    // The page is on view: its mascot moves and its hold begins. Stop ends both; a page that was stopped starts again from its hold's start.
    void Start();
    void Stop();
    bool Answered() const { return answered_; }
    const Stage &Mascot() const { return stage_; }
    lv_obj_t *Root() const { return root_; }
    // How long the page stays, from its start.
    uint32_t HoldMs() const;

private:
    static void OnHold(lv_timer_t *t);
    static void OnBegin(lv_timer_t *t);

    lv_obj_t *root_ = nullptr;
    Stage stage_;
    bool answered_ = false;
    LeaveFn leave_ = nullptr;
    void *user_ = nullptr;
    lv_timer_t *hold_ = nullptr;
    lv_timer_t *begin_ = nullptr;
    bool started_ = false;
};

// The route `deck.closed` ("answered" or "timed-out"): the page alone, leaving back to the screen under it.
std::unique_ptr<router::Screen> ClosedScreen(bool answered);

}  // namespace agent_deck
