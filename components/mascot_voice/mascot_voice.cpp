#include "mascot_voice.h"

#include "devices.h"
#include "esp_timer.h"

namespace mascot_voice {
namespace {

// A moment the mascot told as its frame was drawn: held for the next refresh, when the shell's frame hook plays it. A mascot's
// moment is the buzzer's moment of the same name, the lists in one order (widgets::MascotSfx, devices::sound::Moment).
static_assert(static_cast<int>(widgets::MascotSfx::kFlop) == static_cast<int>(devices::sound::Moment::kFlop) &&
                  static_cast<int>(widgets::MascotSfx::kBlink) == static_cast<int>(devices::sound::Moment::kBlink),
              "a mascot's moment is the buzzer's moment of the same name");

void OnSfx(widgets::MascotSfx sfx, void *user)
{
    static_cast<Voice *>(user)->Tell(sfx);
}

}  // namespace

void Voice::Attach(const widgets::Mascot &mascot, uint32_t same_blink_ms)
{
    same_blink_ms_ = same_blink_ms;
    widgets::set_mascot_sfx(mascot, OnSfx, this);
}

void Voice::Tell(widgets::MascotSfx sfx)
{
    if (!on_) {
        return;
    }
    if (sfx == widgets::MascotSfx::kBlink && same_blink_ms_ != 0) {
        const uint32_t now = lv_tick_get();
        if (blinked_ && lv_tick_diff(now, last_blink_ms_) < same_blink_ms_) {
            return;
        }
        blinked_ = true;
        last_blink_ms_ = now;
    }
    devices::QueueMoment(static_cast<devices::sound::Moment>(sfx));
}

void Voice::SetOn(bool on)
{
    if (on_ == on) {
        return;
    }
    on_ = on;
    if (!on_) {
        Stop(true, true);
    }
}

void Voice::Stop(bool drop, bool force)
{
    if (drop) {
        devices::DropMoments();
    }
    Hear({}, force);
}

void Voice::Listen(const widgets::Mascot &mascot, uint32_t now)
{
    if (!on_) {
        return;
    }
    widgets::MascotMove shown = widgets::MascotMove::kRest;
    uint32_t since = 0;
    widgets::mascot_move_now(mascot, &shown, &since);
    Seen seen;
    seen.typing = widgets::mascot_typing(mascot);
    if (seen.typing && !typing_) {
        typing_since_ = now;
    }
    typing_ = seen.typing;
    seen.spin = shown == widgets::MascotMove::kSpin;
    seen.done = shown == widgets::MascotMove::kFinish || shown == widgets::MascotMove::kCelebrate;
    seen.done_after = seen.done ? widgets::mascot_stage_run(shown).pass_ms : 0;
    seen.move_since = since;
    widgets::MascotJaunt jaunt = widgets::MascotJaunt::kSkip;
    uint32_t jaunt_since = 0;
    if (widgets::mascot_jaunt(mascot, &jaunt, &jaunt_since)) {
        seen.wave = jaunt == widgets::MascotJaunt::kWave;
        seen.think = jaunt == widgets::MascotJaunt::kThink;
        seen.jaunt_since = jaunt_since;
    }
    Hear(MusicOnView(seen, typing_since_), false);
}

// `heard` from now: played from where its move is, kept, or stopped — `force` stops even the success.
void Voice::Hear(const Heard &heard, bool force)
{
    const Change change = ChangeOf(heard_, heard);
    if (change == Change::kKeep && !(force && heard_.music != Music::kNone)) {
        return;
    }
    heard_ = heard;
    if (change != Change::kPlay || force) {
        devices::StopMusic();
        heard_ = {};
        return;
    }
    using DeviceMusic = devices::sound::Music;
    const DeviceMusic music = heard.music == Music::kTyping    ? DeviceMusic::kTyping
                              : heard.music == Music::kSpin    ? DeviceMusic::kSpin
                              : heard.music == Music::kSuccess ? DeviceMusic::kSuccess
                              : heard.music == Music::kWave    ? DeviceMusic::kWave
                                                               : DeviceMusic::kThink;
    // Its start on the move's clock, which may lie ahead (the celebrate's success): the buzzer waits for it.
    const auto into_ms = static_cast<int32_t>(lv_tick_get() - heard.since);
    devices::PlayMusic(music, esp_timer_get_time() - int64_t{into_ms} * 1000);
}

}  // namespace mascot_voice
