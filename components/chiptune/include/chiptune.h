#pragma once

#include <cstddef>
#include <cstdint>

/*
 * THE DECK'S SOUNDS, IN THE 8-BIT VIDEO GAME STYLE (AGENT_DECK 17) — the developer's ask, 2026-11-02: square waves at
 * the NES pulse channels' duty cycles, its stepped triangle for the bass, quick arpeggios and pitch slides, each note
 * on a short attack and a decay, so a sound reads as a game's and never as a phone's. Plain C++, so the board's speaker
 * task plays them and a host program renders the same samples to WAV.
 *
 * One Player renders one sound at a time, 16-bit stereo, the same sample in both channels. Start a sound and pull
 * blocks until done(); a sound marked `loops` starts again from the top instead of ending.
 */
namespace chiptune {

enum class Wave : uint8_t {
    kPulse50,  // a square: the fullest of the pulse voices
    kPulse25,  // the NES's bright, nasal voice
    kPulse12,  // the thinnest, for a coin-like blip
    kTriangle, // the NES's bass: a 16-step triangle, soft and round
    kNoise,    // the NES's noise: a 15-bit shift register clocked at the note's `hz`, for a key's tap and clack; a noise
               // note decays to nothing over its length rather than holding
};

struct Note {
    Wave wave;
    uint16_t hz;      // where it starts; 0 is a rest
    uint16_t hz_end;  // where it slides to by its end; 0 for no slide
    uint16_t ms;      // its length, the decay inside it
    uint8_t volume;   // 0-255 of the sound's level
};

struct Sound {
    const Note *notes;
    size_t count;
    bool loops;
};

enum class SoundId : uint8_t {
    kStartup,   // the deck coming up, over the startup screen: a "ta-daaaa", a short note and the chord held
    kConnected, // the bridge reached: a two-note rise, a coin's blip
    kSuccess,   // a session finished: a short fanfare
    kAttention, // a request, a question, an agent waiting: "dee-doo, dee-doo", a doorbell's call
    kWorking,   // an agent at work: typing — taps and clacks on the noise voice, 2.4 s, looping
    kSpin,      // the mascot's spin-jump, a request allowed: its music only — a warble through the turn and a ta-da as
                // he bounces up off the landing; its launch, turns and impact are moments (kFx*)
    // kSkip to kDust but kThink and kWave: kept for the host's renders; on the board their movements are moments
    kSkip,      // the mascot's skip: a jump's "bwip" at each push-off, a tick at each landing, a thud home
    kWalk,      // the mascot's walk: a soft footstep on each foot down, then a jump's "bwip" and a thud for the hop home
    kThink,     // the mascot's idle: a low "boop boop boop" as he leans to look one way, and again the other, thinking
    kHop,       // one hop of the needs-you jump: the "bwip" of the launch, the tick of the landing
    kHops,      // a needs-you turn of two hops
    kWave,      // the wave: a "hi" on each swing of one hand, then of the other; its raises and flaps are moments
    kDust,      // the dust jump: two launches and two dusty landings
    kTimedOut,  // a request or question timed out: a sad "womp womp", two notes down and a slide lower
    kSubmitted, // a request or question answered in the terminal: a swoosh up and a bright "ding", handed off
    kBoop,      // an agent at work, new on the deck or back from idle or done: a round, low "boop", on an effect voice
    kVolume,    // the settings' Sound turned, a step: a round "bloop" rising, at the level the cues play at
    kUsageLimit, // the plan's usage limit hit: the sad trombone, "wah wah wah waaah", the last note wobbling
    // THE EFFECTS, on the three effect voices over whatever plays, one a moment of a move (widgets::MascotSfx, in its
    // order):
    kFxBlink,      // a blink: a tiny pip
    kFxHappy,      // the eyes gone ^ ^: a two-note twinkle
    kFxChevron,    // the eyes squeezed > <: a little squish
    kFxWink,       // the work's wink: a tink
    kFxLook,       // a glance aside: a faint tick
    kFxHop,        // off the ground: a little "bwip"
    kFxLand,       // back on it: a soft tick
    kFxSway,       // a lean or a sway setting in: a breath of noise
    kFxStretch,    // a stretch of relief: a soft rising "aah"
    kFxHand,       // a hand raised: a chirp
    kFxWipe,       // the brow wiped: a swish
    kFxLaptopDown, // the laptop set down: a dull thud
    kFxLidOpen,    // its lid flipped up: a click rising
    kFxLidShut,    // its lid snapped shut: a clap
    kFxFlag,       // the flag popped up: a "fwip"
    kFxBurst,      // the confetti bursting: a sparkle
    kFxPuff,       // a puff of dust: a soft burst of noise
    kFxBead,       // the sweat's beads appearing: a plip
    kFxPop,        // the beads bursting: a bubble's pop
    kFxStep,       // a leg lifted, a step: a soft tick of noise
    kFxTurn,       // a turn to a new view: a quick whoosh
    kFxHandDown,   // a hand slammed down, a flap: a "fwap"
    kFxCrouch,     // down into a squat, a wind-up: a low "hmp"
    kFxTopple,     // tipping over forward, face first: a falling "wheoo"
    kFxFlop,       // his face landing flat on the laptop's base: a low thump
};

/* The sound by name. */
const Sound &sound(SoundId id);

class Player {
public:
    explicit Player(uint32_t sample_rate) : rate_(sample_rate) {}

    /* From its first note, or `from_ms` into it — a sound kept in time with an animation that began before it — at
     * `level` (0-32767) for a full-volume note. Past its end, it is done at once. */
    void start(const Sound &sound, int16_t level, uint32_t from_ms = 0);
    void stop() { sound_ = nullptr; }
    bool done() const { return sound_ == nullptr; }
    bool playing(const Sound &sound) const { return sound_ == &sound; }

    /* Up to `frames` stereo frames into `out` (2 x frames samples); returns how many it wrote, 0 once done. */
    size_t render(int16_t *out, size_t frames);

private:
    void begin_note();

    uint32_t rate_;
    const Sound *sound_ = nullptr;
    int16_t level_ = 0;
    size_t note_ = 0;
    uint32_t at_ = 0;    // samples into the note
    uint32_t len_ = 0;   // the note's length in samples
    uint32_t phase_ = 0; // 32-bit phase accumulator: a whole cycle is 2^32
    uint16_t lfsr_ = 1;  // the noise voice's shift register, never 0
};

} // namespace chiptune
