// Agent Jam Sticky's sound words — tick, at end, error, success, recording start and recording stop — each one of DESIGN.md's
// sound tokens (design_tokens.h `kSound*`), chosen by the person by ear on the board from the candidates in the agent
// deck's 8-bit style. devices::Play(Word) plays one; the hardware test's audition and its `sound <name>` list them
// (DESIGN → Motion → Sound; STICKY_DESIGN_TOKENS §5).

#pragma once

#include <cstddef>
#include <cstdint>

#include "design_tokens.h"
#include "sound_rules.h"

namespace devices::sound {

enum class Word : uint8_t { kTick, kAtEnd, kError, kSuccess, kRecordingStart, kRecordingStop };

struct NamedSound {
    Word word;
    const char *id;    // the console's name, `sound <id>`, and the token's: sound.<id>
    const char *name;  // the word, as the audition row says it
    const char *what;  // the chosen sound, as the row's value says it
    Sound sound;
};

// In Word's order, so kSounds[Word] is that word's.
inline constexpr NamedSound kSounds[] = {
    {Word::kTick, "tick", "Tick", "Click", Of(design_tokens::kSoundTick)},
    {Word::kAtEnd, "at-end", "At end", "Double tick", Of(design_tokens::kSoundAtEnd)},
    {Word::kError, "error", "Error", "Womp womp", Of(design_tokens::kSoundError)},
    {Word::kSuccess, "success", "Success", "Rise", Of(design_tokens::kSoundSuccess)},
    {Word::kRecordingStart, "recording-start", "Recording start", "Nudge up", Of(design_tokens::kSoundRecordingStart)},
    {Word::kRecordingStop, "recording-stop", "Recording stop", "Nudge down", Of(design_tokens::kSoundRecordingStop)},
};
inline constexpr size_t kSoundCount = sizeof(kSounds) / sizeof(kSounds[0]);

constexpr const Sound &SoundOf(Word word)
{
    return kSounds[static_cast<size_t>(word)].sound;
}

// Claude's mascot's moments, outside the six words: one a movement of a move, in widgets::MascotSfx's order — a page that
// shows the mascot mirrors its MascotSfx here and asserts the two agree — each DESIGN.md's `sound.mascot-*`
// (DESIGN → Motion → Sound; MASCOT §4 rule 10). devices::QueueMoment holds one until the next refresh starts.
enum class Moment : uint8_t { kBlink, kHappy, kChevron, kWink, kLook, kHop, kLand, kSway, kStretch, kHand, kWipe, kLaptopDown, kLidOpen, kLidShut, kFlag, kBurst, kPuff, kBead, kPop, kStep, kTurn, kHandDown, kCrouch, kTopple, kFlop };

inline constexpr Sound kMoments[] = {
    Of(design_tokens::kSoundMascotBlink),
    Of(design_tokens::kSoundMascotHappy),
    Of(design_tokens::kSoundMascotChevron),
    Of(design_tokens::kSoundMascotWink),
    Of(design_tokens::kSoundMascotLook),
    Of(design_tokens::kSoundMascotHop),
    Of(design_tokens::kSoundMascotLand),
    Of(design_tokens::kSoundMascotSway),
    Of(design_tokens::kSoundMascotStretch),
    Of(design_tokens::kSoundMascotHand),
    Of(design_tokens::kSoundMascotWipe),
    Of(design_tokens::kSoundMascotLaptopDown),
    Of(design_tokens::kSoundMascotLidOpen),
    Of(design_tokens::kSoundMascotLidShut),
    Of(design_tokens::kSoundMascotFlag),
    Of(design_tokens::kSoundMascotBurst),
    Of(design_tokens::kSoundMascotPuff),
    Of(design_tokens::kSoundMascotBead),
    Of(design_tokens::kSoundMascotPop),
    Of(design_tokens::kSoundMascotStep),
    Of(design_tokens::kSoundMascotTurn),
    Of(design_tokens::kSoundMascotHandDown),
    Of(design_tokens::kSoundMascotCrouch),
    Of(design_tokens::kSoundMascotTopple),
    Of(design_tokens::kSoundMascotFlop),
};
inline constexpr size_t kMomentCount = sizeof(kMoments) / sizeof(kMoments[0]);
static_assert(kMomentCount == static_cast<size_t>(Moment::kFlop) + 1, "a sound for every moment");

constexpr const Sound &SoundOf(Moment moment)
{
    return kMoments[static_cast<size_t>(moment)];
}

// Claude's mascot's music (DESIGN → Motion → Sound): kept in time with its move under every other sound, each DESIGN.md's
// `sound.mascot-*`; the typing loops while he types.
enum class Music : uint8_t { kTyping, kSpin, kSuccess, kWave, kThink };
inline constexpr Sound kMusic[] = {
    Of(design_tokens::kSoundMascotTyping), Of(design_tokens::kSoundMascotSpin), Of(design_tokens::kSoundMascotSuccess),
    Of(design_tokens::kSoundMascotWave),   Of(design_tokens::kSoundMascotThink),
};
static_assert(sizeof(kMusic) / sizeof(kMusic[0]) == static_cast<size_t>(Music::kThink) + 1, "a sound for every music");

constexpr const Sound &SoundOf(Music music)
{
    return kMusic[static_cast<size_t>(music)];
}

constexpr bool Loops(Music music)
{
    return music == Music::kTyping;
}

// A music that leads: a phrase heard whole — the spin's, the success's fanfare — so while it sounds a frame's moments are
// dropped, not played over it; a press's tick still cuts in. The rest — the typing, the wave's "hi"s, the thinking — sit
// under the moments (DESIGN → Motion → Sound).
constexpr bool Leads(Music music)
{
    return music == Music::kSpin || music == Music::kSuccess;
}

}  // namespace devices::sound
