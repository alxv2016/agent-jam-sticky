// A page's mascot and how it is heard: the widget's rig at the deck's place, the state machine that keeps what it is doing (a
// widgets::MascotPlay, as the deck's stage keeps it), and its voice — the moments it tells to the buzzer and the music kept in time with
// its move (mascot_voice.h). A screen builds one in OnCreate, starts it in OnEnter and stops it in OnExit, so a covered or left page
// plays nothing (DESIGN → Motion → Waits and things that update on their own).

#pragma once

#include <cstdint>

#include "lvgl.h"
#include "mascot_voice.h"
#include "router/router.h"
#include "widgets/mascot.h"

namespace agent_deck {

class Stage {
public:
    Stage() = default;
    Stage(const Stage &) = delete;
    Stage &operator=(const Stage &) = delete;
    ~Stage();

    // The mascot at rest in `parent`, `cell` px a cell, its root at (x, y); drawn in front of what is already in `parent`. The voice
    // is attached to it. Call once.
    void Build(lv_obj_t *parent, int32_t x, int32_t y, int32_t cell);

    // A previous chat's grey, or a current chat's.
    void SetPrevious(bool previous);
    // Brings the mascot to the front of its page, over every element — after a page rebuilds the elements under it.
    void ToFront();

    // `move` from its start, now: a page turned to, a screen opened.
    void Play(widgets::MascotMove move);
    // `move` after the one playing finishes — a record that changes the state of the page on view.
    void Ask(widgets::MascotMove move);
    // The agent's limit hit while it types: a faceplant first, then `move`.
    void Fall(widgets::MascotMove move);
    // `reaction` once, then `move`: a request allowed, a session finished, an agent that joined.
    void React(widgets::MascotMove reaction, widgets::MascotMove move);
    // The look-around from its start, then rest: a request that timed out.
    void Look();
    // Asks it to rest at `after_ms` from now, for a move that is stopped by the screen's own clock (the closed page's look-around).
    void RestAfter(uint32_t after_ms);

    // The voice's timer: while started, the mascot's music is kept in time with its move. Start in OnEnter, Stop in OnExit.
    void Start();
    void Stop();
    // Heard or not: the sound setting.
    void SetHeard(bool on) { voice_.SetOn(on); }

    const widgets::Mascot &Rig() const { return mascot_; }
    widgets::MascotMove Move() const { return move_; }
    bool Built() const { return alive_ != nullptr; }  // the mascot is on a screen LVGL still holds

private:
    static void OnLook(lv_timer_t *t);

    widgets::Mascot mascot_ = {};
    // The mascot's root while LVGL has it, cleared as LVGL deletes it (router::Hold): a screen's objects go before the screen
    // object that owns this stage is freed, and reading a deleted object — even to ask whether it is valid — is a use after free.
    lv_obj_t *alive_ = nullptr;
    widgets::MascotPlay play_ = {};
    widgets::MascotMove move_ = widgets::MascotMove::kRest;
    mascot_voice::Voice voice_;
    lv_timer_t *look_ = nullptr;
    uint32_t rest_at_ = 0;
    bool rest_pending_ = false;
};

}  // namespace agent_deck
