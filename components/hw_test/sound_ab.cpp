// The buzzer's A/B (STICKY_UI_KIT_PORT §4 item 8): one of the agent deck's own sounds (chiptune.h) two ways on the note
// player, for the person's ear — 1, shifted up by whole octaves into the buzzer's band; 2, at the deck's own pitches, as
// the mascot's sounds are (a moment's own token, sounds.h kMoments, for a moment). The console's `sound ab`; nothing on
// the glass. The deck's synth streamed through the buzzer was a third way, heard and taken out: it hissed.

#include "sound_ab.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "chiptune.h"
#include "design_tokens.h"
#include "devices.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sounds.h"

namespace hw_test {
namespace {

namespace dt = design_tokens;
using chiptune::SoundId;

static_assert(static_cast<int>(chiptune::Wave::kPulse50) == static_cast<int>(dt::SoundWave::kPulse50) &&
                  static_cast<int>(chiptune::Wave::kPulse25) == static_cast<int>(dt::SoundWave::kPulse25) &&
                  static_cast<int>(chiptune::Wave::kPulse12) == static_cast<int>(dt::SoundWave::kPulse12) &&
                  static_cast<int>(chiptune::Wave::kTriangle) == static_cast<int>(dt::SoundWave::kTriangle) &&
                  static_cast<int>(chiptune::Wave::kNoise) == static_cast<int>(dt::SoundWave::kNoise),
              "the deck's voices and the buzzer's, in one order");
static_assert(static_cast<int>(SoundId::kFxFlop) - static_cast<int>(SoundId::kFxBlink) + 1 ==
                  static_cast<int>(devices::sound::kMomentCount),
              "a deck effect for every buzzer moment, in one order");

struct Named {
    const char *name;
    SoundId id;
};
// The deck's music and loops by a short name; its effects, the mascot's moments, as "fx-" and the moment.
constexpr Named kNamed[] = {
    {"typing", SoundId::kWorking}, {"hop", SoundId::kHop},       {"hops", SoundId::kHops},
    {"success", SoundId::kSuccess}, {"spin", SoundId::kSpin},    {"think", SoundId::kThink},
    {"wave", SoundId::kWave},       {"skip", SoundId::kSkip},    {"walk", SoundId::kWalk},
    {"dust", SoundId::kDust},       {"attention", SoundId::kAttention}, {"startup", SoundId::kStartup},
    {"limit", SoundId::kUsageLimit},
    {"fx-blink", SoundId::kFxBlink},   {"fx-happy", SoundId::kFxHappy},     {"fx-chevron", SoundId::kFxChevron},
    {"fx-wink", SoundId::kFxWink},     {"fx-look", SoundId::kFxLook},       {"fx-hop", SoundId::kFxHop},
    {"fx-land", SoundId::kFxLand},     {"fx-sway", SoundId::kFxSway},       {"fx-stretch", SoundId::kFxStretch},
    {"fx-hand", SoundId::kFxHand},     {"fx-wipe", SoundId::kFxWipe},       {"fx-laptop", SoundId::kFxLaptopDown},
    {"fx-lid-open", SoundId::kFxLidOpen}, {"fx-lid-shut", SoundId::kFxLidShut}, {"fx-flag", SoundId::kFxFlag},
    {"fx-burst", SoundId::kFxBurst},   {"fx-puff", SoundId::kFxPuff},       {"fx-bead", SoundId::kFxBead},
    {"fx-pop", SoundId::kFxPop},       {"fx-step", SoundId::kFxStep},       {"fx-turn", SoundId::kFxTurn},
    {"fx-hand-down", SoundId::kFxHandDown}, {"fx-crouch", SoundId::kFxCrouch}, {"fx-topple", SoundId::kFxTopple},
    {"fx-flop", SoundId::kFxFlop},
};

constexpr size_t kMaxNotes = 64;
std::array<dt::SoundNote, kMaxNotes> s_notes;  // way 1 or 2's notes, kept while the buzzer plays them
uint32_t LengthMs(const chiptune::Sound &sound)
{
    uint32_t ms = 0;
    for (size_t i = 0; i < sound.count; ++i) {
        ms += sound.notes[i].ms;
    }
    return ms;
}

bool IsMoment(SoundId id)
{
    return static_cast<int>(id) >= static_cast<int>(SoundId::kFxBlink);
}

// The deck's notes on the note player: at their own pitches, or shifted up whole octaves until the lowest pitch is in
// the buzzer's band (`sound.band-low`), none past the player's 10 kHz.
devices::sound::Sound AsNotes(const chiptune::Sound &sound, bool shifted, int *octaves)
{
    const size_t n = std::min(sound.count, kMaxNotes);
    uint32_t lowest = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        if (sound.notes[i].hz != 0) {
            lowest = std::min<uint32_t>(lowest, sound.notes[i].hz);
        }
    }
    int k = 0;
    while (shifted && lowest != UINT32_MAX && (lowest << k) < dt::kSoundBandLowHz) {
        ++k;
    }
    const auto up = [k](uint16_t hz) { return static_cast<uint16_t>(std::min<uint32_t>(10000, uint32_t{hz} << k)); };
    for (size_t i = 0; i < n; ++i) {
        const chiptune::Note &d = sound.notes[i];
        s_notes[i] = {static_cast<dt::SoundWave>(d.wave), up(d.hz), d.hz_end != 0 ? up(d.hz_end) : uint16_t{0}, d.ms,
                      d.volume};
    }
    *octaves = k;
    return {s_notes.data(), n};
}

// Plays `named` way `way` (1 or 2); returns how long it lasts, ms.
uint32_t PlayWay(const Named &named, int way)
{
    const chiptune::Sound &sound = chiptune::sound(named.id);
    const uint32_t ms = LengthMs(sound);
    if (way == 2 && IsMoment(named.id)) {
        const auto moment = static_cast<devices::sound::Moment>(static_cast<int>(named.id) -
                                                                 static_cast<int>(SoundId::kFxBlink));
        devices::Play(devices::sound::SoundOf(moment));
        std::printf("sound ab %s 2: the mascot's moment, as it plays now, %u ms\n", named.name, static_cast<unsigned>(ms));
        return ms;
    }
    int octaves = 0;
    devices::Play(AsNotes(sound, way == 1, &octaves));
    if (way == 1) {
        std::printf("sound ab %s 1: the note player, up %d octave%s, %u ms\n", named.name, octaves,
                    octaves == 1 ? "" : "s", static_cast<unsigned>(ms));
    } else {
        std::printf("sound ab %s 2: the note player at the deck's pitches, %u ms\n", named.name,
                    static_cast<unsigned>(ms));
    }
    return ms;
}

}  // namespace

void SoundAb(const char *arg)
{
    char name[24] = {};
    char way[12] = {};
    if (std::sscanf(arg, "%23s %11s", name, way) < 1) {
        std::printf("sound ab <name> [1|2|all]: 1 shifted up into the band on the note player, 2 the deck's pitches on "
                    "it, as the mascot's are; all plays 1, then 2. Names:\n");
        for (const Named &n : kNamed) {
            std::printf("  %-14s %u ms\n", n.name, static_cast<unsigned>(LengthMs(chiptune::sound(n.id))));
        }
        return;
    }
    const Named *named = nullptr;
    for (const Named &n : kNamed) {
        if (std::strcmp(name, n.name) == 0) {
            named = &n;
        }
    }
    if (named == nullptr) {
        std::printf("sound ab: no sound \"%s\" (try: sound ab)\n", name);
        return;
    }
    if (way[0] == '\0' || std::strcmp(way, "all") == 0) {
        for (int w = 1; w <= 2; ++w) {
            const uint32_t ms = PlayWay(*named, w);
            vTaskDelay(pdMS_TO_TICKS(ms + 900));  // the sound, then a beat of quiet before the next way
        }
        return;
    }
    const int w = way[0] - '0';
    if (w < 1 || w > 2 || way[1] != '\0') {
        std::printf("sound ab: way \"%s\" is 1, 2 or all\n", way);
        return;
    }
    PlayWay(*named, w);
}

}  // namespace hw_test
