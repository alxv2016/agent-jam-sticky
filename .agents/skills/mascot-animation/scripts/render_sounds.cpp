/*
 * The deck's 8-bit sounds as WAV files, off the device: the same synth (the host's synth_source), at the 16 kHz the
 * device plays it at.
 *
 *   render_sounds all <dir>                 every SoundId to <dir>/<nn>_<name>.wav
 *   render_sounds track <events.log> <wav>  a scenario's mascot moments ("<s> s  sfx <name>" lines, as the simulator
 *                                           writes them) mixed as the board mixes them: three effect voices, a free
 *                                           one or the next in turn
 *
 * Louder than the board (x3) so a laptop's speakers carry it; the balance between sounds is the board's. Built and run
 * by render_sounds.sh.
 *
 * A HOST WHOSE VOICE IS A BUZZER (the host's side: synth_kind "buzzer", MASCOT_SYNTH_BUZZER): its sounds are its note
 * rules' frames — a square switched at each frame's pitch and duty, as the coil plays them — and a track is told at the
 * flipbook's pace (MASCOT §6): the moments of each frame, MASCOT_FRAME_MS long, played in turn as the next frame starts,
 * within the host's budget, the rest dropped, each frame's cut by the next's. `all` writes every moment.
 */
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#ifndef MASCOT_SYNTH_BUZZER
// The host's synth, its header and namespace from the host's side (render_sounds.sh passes them).
#ifndef MASCOT_SYNTH_HEADER
#define MASCOT_SYNTH_HEADER "chiptune.h"
#endif
#ifndef MASCOT_SYNTH_NAMESPACE
#define MASCOT_SYNTH_NAMESPACE chiptune
#endif
#include MASCOT_SYNTH_HEADER

using namespace MASCOT_SYNTH_NAMESPACE;

namespace {

constexpr uint32_t kRate = 16000; // the codec's rate on the device
constexpr float kBoost = 3.0f;
constexpr float kSoundLevel = 0.18f;  // the host speaker's kSoundLevel
constexpr float kLoopLevel = 0.10f;   // the host speaker's kLoopLevel: the working loop and the thinking
constexpr float kEffectLevel = 0.12f; // the host speaker's kEffectLevel: a moment of a move
constexpr size_t kEffectVoices = 3;   // kEffectVoices

/* Every SoundId's name, in its order; the effects' are the simulator's `sfx` names (its stand-in's names for each
 * moment; where they live, the host's side). */
constexpr const char *kNames[] = {
    "startup", "connected", "success", "attention", "working", "spin", "skip", "walk", "think", "hop-phrase",
    "hops-phrase", "wave", "dust", "timed-out", "submitted", "boop", "volume", "usage-limit",
    // the effects, from kFxBlink
    "blink", "happy", "chevron", "wink", "look", "hop", "land", "sway", "stretch", "hand", "wipe", "laptop-down",
    "lid-open", "lid-shut", "flag", "burst", "puff", "bead", "pop", "step", "turn", "hand-down", "crouch",
    "topple", "flop"};
static_assert(std::size(kNames) == static_cast<size_t>(SoundId::kFxFlop) + 1, "a name for every SoundId");
constexpr size_t kFirstEffect = static_cast<size_t>(SoundId::kFxBlink);

int16_t level(float share) { return static_cast<int16_t>(share * kBoost * 32767.0f); }

bool wav(const char *path, const std::vector<int16_t> &s)
{
    FILE *f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::perror(path);
        return false;
    }
    const uint32_t data = static_cast<uint32_t>(s.size() * 2), riff = 36 + data, fmt = 16, byte_rate = kRate * 4;
    const uint16_t pcm = 1, channels = 2, align = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&riff, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f);
    std::fwrite(&pcm, 2, 1, f);
    std::fwrite(&channels, 2, 1, f);
    std::fwrite(&kRate, 4, 1, f);
    std::fwrite(&byte_rate, 4, 1, f);
    std::fwrite(&align, 2, 1, f);
    std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f);
    std::fwrite(&data, 4, 1, f);
    std::fwrite(s.data(), 2, s.size(), f);
    std::fclose(f);
    return true;
}

int all(const char *dir)
{
    for (size_t i = 0; i < std::size(kNames); i++) {
        const auto id = static_cast<SoundId>(i);
        Player p(kRate);
        const bool quiet = id == SoundId::kWorking || id == SoundId::kThink;
        p.start(sound(id), level(i >= kFirstEffect ? kEffectLevel : quiet ? kLoopLevel : kSoundLevel));
        std::vector<int16_t> out;
        std::array<int16_t, 512> block;
        const size_t limit = kRate * 7; // the working loop: three bars
        size_t n = 0;
        while ((n = p.render(block.data(), block.size() / 2)) > 0 && out.size() / 2 < limit) {
            out.insert(out.end(), block.begin(), block.begin() + 2 * n);
        }
        char path[1024];
        std::snprintf(path, sizeof path, "%s/%02zu_%s.wav", dir, i, kNames[i]);
        if (!wav(path, out)) {
            return 1;
        }
        std::printf("%s %.2f s\n", path, out.size() / 2.0 / kRate);
    }
    return 0;
}

// A moment the log told: when, and which effect.
struct Moment {
    uint32_t ms;
    size_t id;
};

// The "sfx" lines of a simulator's log, in order; false, said, on an unknown moment or none.
bool read_moments(const char *log, std::vector<Moment> &moments)
{
    std::ifstream in(log);
    if (!in) {
        std::perror(log);
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        double at = 0;
        std::string unit, what, name;
        if (!(words >> at >> unit >> what >> name) || (unit != "s" && unit != "ms") || what != "sfx") {
            continue;
        }
        const auto ms = static_cast<uint32_t>(unit == "s" ? at * 1000.0 + 0.5 : at);
        size_t id = kFirstEffect;
        while (id < std::size(kNames) && name != kNames[id]) {
            id++;
        }
        if (id == std::size(kNames)) {
            std::fprintf(stderr, "%s: unknown moment \"%s\"\n", log, name.c_str());
            return false;
        }
        moments.push_back({ms, id});
    }
    if (moments.empty()) {
        std::fprintf(stderr, "%s: no sfx lines\n", log);
        return false;
    }
    return true;
}

// The moments mixed as the deck mixes them: each on a free effect voice as its time comes, the oldest stolen when none
// is free. Returns how many were started.
size_t mix_moments(const std::vector<Moment> &moments, std::vector<int16_t> &out, size_t total)
{
    std::array<Player, kEffectVoices> voices = {Player(kRate), Player(kRate), Player(kRate)};
    std::array<int16_t, 2> frame;
    size_t next = 0, steal = 0, counts = 0;
    for (size_t f = 0; f < total; f++) {
        while (next < moments.size() && static_cast<size_t>(moments[next].ms) * kRate / 1000 <= f) {
            size_t v = 0;
            while (v < voices.size() && !voices[v].done()) {
                v++;
            }
            if (v == voices.size()) {
                v = steal++ % voices.size();
            }
            voices[v].start(sound(static_cast<SoundId>(moments[next].id)), level(kEffectLevel));
            next++;
            counts++;
        }
        int32_t l = 0, r = 0;
        for (Player &p : voices) {
            if (!p.done() && p.render(frame.data(), 1) == 1) {
                l += frame[0];
                r += frame[1];
            }
        }
        out[2 * f] = static_cast<int16_t>(std::clamp<int32_t>(l, INT16_MIN, INT16_MAX));
        out[2 * f + 1] = static_cast<int16_t>(std::clamp<int32_t>(r, INT16_MIN, INT16_MAX));
    }
    return counts;
}

int track(const char *log, const char *path)
{
    std::vector<Moment> moments;
    if (!read_moments(log, moments)) {
        return 1;
    }
    const size_t total = static_cast<size_t>(moments.back().ms + 1500) * kRate / 1000;
    std::vector<int16_t> out(2 * total, 0);
    const size_t counts = mix_moments(moments, out, total);
    if (!wav(path, out)) {
        return 1;
    }
    std::printf("%s %.2f s, %zu moments\n", path, total / static_cast<double>(kRate), counts);
    return 0;
}

} // namespace

#else  // MASCOT_SYNTH_BUZZER

#ifndef MASCOT_SYNTH_HEADER
#define MASCOT_SYNTH_HEADER "sounds.h"
#endif
#ifndef MASCOT_SYNTH_NAMESPACE
#define MASCOT_SYNTH_NAMESPACE devices::sound
#endif
#ifndef MASCOT_FRAME_MS
#define MASCOT_FRAME_MS 460
#endif
#include MASCOT_SYNTH_HEADER

using namespace MASCOT_SYNTH_NAMESPACE;

namespace {

constexpr uint32_t kRate = 48000;  // high enough for the band's 6 kHz and its square's edges
constexpr float kLevel = 0.30f;
constexpr uint32_t kHalfDuty = 512;  // the channel's half duty, 10 bits
constexpr uint32_t kFrameMs = MASCOT_FRAME_MS;

/* Every moment's name, in its order — the simulator's `sfx` names, the widget's MascotSfx. */
constexpr const char *kNames[] = {"blink", "happy", "chevron", "wink", "look", "hop", "land", "sway", "stretch",
                                  "hand", "wipe", "laptop-down", "lid-open", "lid-shut", "flag", "burst", "puff",
                                  "bead", "pop", "step", "turn", "hand-down", "crouch", "topple", "flop"};
static_assert(std::size(kNames) == kMomentCount, "a name for every moment");

/* `ms` of `sound` from its start onto `out` at `from`, the coil's square at each millisecond's frame. */
void render(const Sound &sound, uint32_t ms, std::vector<int16_t> &out, size_t from)
{
    double phase = 0;
    const auto amp = static_cast<int16_t>(kLevel * 32767.0f);
    for (size_t i = 0; i < static_cast<size_t>(ms) * kRate / 1000 && from + i < out.size() / 2; i++) {
        const Frame f = FrameAt(sound, static_cast<uint32_t>(i * 1000 / kRate), kHalfDuty);
        int16_t v = 0;
        if (f.duty != 0 && f.hz != 0) {
            phase += static_cast<double>(f.hz) / kRate;
            phase -= static_cast<int64_t>(phase);
            v = phase < f.duty / 1024.0 ? amp : static_cast<int16_t>(-amp);
        }
        out[2 * (from + i)] = v;
        out[2 * (from + i) + 1] = v;
    }
}

bool wav(const char *path, const std::vector<int16_t> &s)
{
    FILE *f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::perror(path);
        return false;
    }
    const uint32_t data = static_cast<uint32_t>(s.size() * 2), riff = 36 + data, fmt = 16, byte_rate = kRate * 4;
    const uint16_t pcm = 1, channels = 2, align = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&riff, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f);
    std::fwrite(&pcm, 2, 1, f);
    std::fwrite(&channels, 2, 1, f);
    std::fwrite(&kRate, 4, 1, f);
    std::fwrite(&byte_rate, 4, 1, f);
    std::fwrite(&align, 2, 1, f);
    std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f);
    std::fwrite(&data, 4, 1, f);
    std::fwrite(s.data(), 2, s.size(), f);
    std::fclose(f);
    return true;
}

int all(const char *dir)
{
    for (size_t i = 0; i < kMomentCount; i++) {
        const uint32_t ms = LengthMs(kMoments[i]) + 100;
        std::vector<int16_t> out(2 * static_cast<size_t>(ms) * kRate / 1000, 0);
        render(kMoments[i], ms, out, 0);
        char path[1024];
        std::snprintf(path, sizeof path, "%s/%02zu_%s.wav", dir, i, kNames[i]);
        if (!wav(path, out)) {
            return 1;
        }
        std::printf("%s %u ms\n", path, static_cast<unsigned>(LengthMs(kMoments[i])));
    }
    return 0;
}

/* A log's moments at the flipbook's pace: those told within a frame play in turn as the next frame starts. */
int track(const char *log, const char *path)
{
    std::ifstream in(log);
    if (!in) {
        std::perror(log);
        return 1;
    }
    std::vector<std::pair<uint32_t, size_t>> told;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        double at = 0;
        std::string unit, what, name;
        if (!(words >> at >> unit >> what >> name) || (unit != "s" && unit != "ms") || what != "sfx") {
            continue;
        }
        size_t id = 0;
        while (id < std::size(kNames) && name != kNames[id]) {
            id++;
        }
        if (id == std::size(kNames)) {
            std::fprintf(stderr, "%s: unknown moment \"%s\"\n", log, name.c_str());
            return 1;
        }
        told.push_back({static_cast<uint32_t>(unit == "s" ? at * 1000.0 + 0.5 : at), id});
    }
    if (told.empty()) {
        std::fprintf(stderr, "%s: no sfx lines\n", log);
        return 1;
    }
    const uint32_t frames = told.back().first / kFrameMs + 2;
    std::vector<int16_t> out(2 * static_cast<size_t>(frames + 1) * kFrameMs * kRate / 1000, 0);
    size_t next = 0, played = 0;
    static Note notes[kMomentNotesMax];
    for (uint32_t k = 1; k <= frames; k++) {
        MomentQueue q;
        while (next < told.size() && told[next].first < k * kFrameMs) {
            q.Push(kMoments[told[next].second]);
            next++;
        }
        size_t kept = 0;
        const size_t n = TakeMoments(q, design_tokens::kSoundMomentsMaxMs, notes, &kept);
        if (n > 0) {
            render(Sound{notes, n}, kFrameMs, out, static_cast<size_t>(k) * kFrameMs * kRate / 1000);
            played += kept;
        }
    }
    if (!wav(path, out)) {
        return 1;
    }
    std::printf("%s %.2f s, %zu moments told, %zu played at %u ms a frame\n", path, out.size() / 2.0 / kRate,
                told.size(), played, static_cast<unsigned>(kFrameMs));
    return 0;
}

} // namespace

#endif  // MASCOT_SYNTH_BUZZER

int main(int argc, char **argv)
{
    if (argc == 3 && std::strcmp(argv[1], "all") == 0) {
        return all(argv[2]);
    }
    if (argc == 4 && std::strcmp(argv[1], "track") == 0) {
        return track(argv[2], argv[3]);
    }
    std::fprintf(stderr, "usage: %s all <dir> | track <events.log> <out.wav>\n", argv[0]);
    return 2;
}
