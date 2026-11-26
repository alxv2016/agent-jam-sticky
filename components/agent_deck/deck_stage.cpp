#include "deck_stage.h"

#include "deck_ui.h"
#include "design_tokens.h"

namespace agent_deck {
namespace {

// How often the voice looks at the mascot: the deck's own look, kLookMs, 50 ms — well inside the shortest sound it keeps time with.
constexpr uint32_t kLookMs = 50;

}  // namespace

void Stage::Build(lv_obj_t *parent, int32_t x, int32_t y, int32_t cell)
{
    mascot_ = widgets::make_mascot(parent, cell);
    router::Hold(&alive_, mascot_.root);
    lv_obj_set_pos(mascot_.root, x, y);
    voice_.Attach(mascot_);
    play_ = widgets::mascot_play(widgets::MascotMove::kRest, lv_tick_get());
}

void Stage::SetPrevious(bool previous)
{
    layout::ColourMascot(mascot_, previous);
}

void Stage::ToFront()
{
    if (alive_ != nullptr) {
        lv_obj_move_foreground(mascot_.root);
    }
}

void Stage::Play(widgets::MascotMove move)
{
    move_ = move;
    rest_pending_ = false;
    play_ = widgets::mascot_play(move, lv_tick_get());
    widgets::play_mascot(mascot_, play_);
}

void Stage::Ask(widgets::MascotMove move)
{
    if (move == move_) {
        return;
    }
    move_ = move;
    widgets::ask_mascot(play_, move, lv_tick_get());
    widgets::play_mascot(mascot_, play_);
}

void Stage::Fall(widgets::MascotMove move)
{
    move_ = move;
    widgets::fall_mascot(play_, move, lv_tick_get());
    widgets::play_mascot(mascot_, play_);
}

void Stage::React(widgets::MascotMove reaction, widgets::MascotMove move)
{
    move_ = move;
    widgets::react_mascot(play_, reaction, move, lv_tick_get());
    widgets::play_mascot(mascot_, play_);
}

void Stage::Look()
{
    move_ = widgets::MascotMove::kRest;
    play_ = widgets::mascot_play_look(lv_tick_get());
    widgets::play_mascot(mascot_, play_);
}

void Stage::RestAfter(uint32_t after_ms)
{
    rest_at_ = lv_tick_get() + after_ms;
    rest_pending_ = true;
}

void Stage::OnLook(lv_timer_t *t)
{
    Stage *stage = static_cast<Stage *>(lv_timer_get_user_data(t));
    const uint32_t now = lv_tick_get();
    if (stage->rest_pending_ && lv_tick_diff(now, stage->rest_at_) < (1u << 31)) {
        stage->rest_pending_ = false;
        widgets::ask_mascot(stage->play_, widgets::MascotMove::kRest, now);
        widgets::play_mascot(stage->mascot_, stage->play_);
    }
    stage->voice_.Listen(stage->mascot_, now);
}

void Stage::Start()
{
    if (look_ == nullptr) {
        look_ = lv_timer_create(OnLook, kLookMs, this);
    }
    if (alive_ != nullptr) {
        // A mascot built and not yet entered, or one covered and uncovered: its move goes on where the clock says it is.
        widgets::play_mascot(mascot_, play_);
    }
}

void Stage::Stop()
{
    if (look_ != nullptr) {
        lv_timer_delete(look_);
        look_ = nullptr;
    }
    voice_.Stop(true, true);
    if (alive_ != nullptr) {
        widgets::play_mascot(mascot_, widgets::MascotMove::kRest);
    }
}

Stage::~Stage()
{
    Stop();
    router::Hold(&alive_, nullptr);  // the slot dies with the stage: let go of the mascot's delete event first
}

}  // namespace agent_deck
