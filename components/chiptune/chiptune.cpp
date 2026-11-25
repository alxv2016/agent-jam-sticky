#include "chiptune.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace chiptune {

namespace {

/* Equal-tempered pitches, Hz, rounded: the notes the sounds are written in. */
constexpr uint16_t G5 = 784;
constexpr uint16_t C6 = 1047, D6 = 1175, E6 = 1319, G6 = 1568;

constexpr Wave P50 = Wave::kPulse50;
constexpr Wave P25 = Wave::kPulse25;
constexpr Wave P12 = Wave::kPulse12;
constexpr Wave NOI = Wave::kNoise;
constexpr Wave TRI = Wave::kTriangle;

/* THE STARTUP IS A "TA-DAAAA" — the developer's ask, 2026-11-03, over the startup screen (AGENT_DECK 4.8): a short
 * "ta" on the fifth below, a breath, then the "daaaa", the major chord held as the NES holds one — an arpeggio round
 * its four notes, kTadaStepMs each, fading from kTadaFrom to kTadaTo as it rings — and the top note held last, bent up
 * a hair. 1.5 s. */
constexpr uint16_t C7 = 2093;
constexpr size_t kTadaRounds = 8;
constexpr uint16_t kTadaStepMs = 30;
constexpr uint8_t kTadaFrom = 235;
constexpr uint8_t kTadaTo = 160;
constexpr size_t kTadaNotes = 2 + 4 * kTadaRounds + 1;
constexpr std::array<Note, kTadaNotes> tada()
{
    std::array<Note, kTadaNotes> n = {};
    size_t i = 0;
    n[i++] = {P25, G5, 0, 100, 225};
    n[i++] = {P25, 0, 0, 40, 0};
    const uint16_t chord[] = {C6, E6, G6, C7};
    for (size_t r = 0; r < kTadaRounds; r++) {
        const auto v = static_cast<uint8_t>(kTadaFrom - (kTadaFrom - kTadaTo) * r / (kTadaRounds - 1));
        for (const uint16_t hz : chord) {
            n[i++] = {P50, hz, 0, kTadaStepMs, v};
        }
    }
    n[i++] = {P50, C7, C7 + 30, 360, kTadaTo};
    return n;
}
constexpr std::array<Note, kTadaNotes> kStartup = tada();

/* THE CONNECTION: the thin pulse's blip, a fifth up with a quick slide into the top note — something found. */
constexpr Note kConnected[] = {
    {P12, G5, 0, 70, 220},
    {P12, D6 - 60, D6, 260, 240},
};

/* THE SUCCESS: a fanfare — up the chord, a beat on the top, a step down and the top held. */
constexpr Note kSuccess[] = {
    {P25, G5, 0, 80, 210}, {P25, C6, 0, 80, 210}, {P25, E6, 0, 80, 220}, {P25, G6, 0, 160, 230},
    {P25, 0, 0, 40, 0},    {P25, E6, 0, 90, 210}, {P50, G6, 0, 380, 230},
};

/* THE ATTENTION: "dee-doo, dee-doo" — a drop of a minor third on the soft square, twice, a doorbell's call for the hand.
 * Not a jump up: a short note leaping to a held higher one is a game's coin, the developer's ear, 2026-11-02. */
constexpr Note kAttention[] = {
    {P50, G6, 0, 85, 220}, {P50, E6, 0, 130, 230}, {P50, 0, 0, 90, 0},
    {P50, G6, 0, 85, 220}, {P50, E6, 0, 200, 230},
};

/* THE WORKING LOOP IS TYPING — the developer's ask, 2026-11-02: "tap tap clack clack". A tap is a short, high burst
 * of the noise voice, a key's tick; a clack a longer, lower one, a key bottoming out, and the low one the space bar.
 * The rhythm is uneven, as a hand types: runs of taps, a clack to end a word, a pause to think — 2.4
 * seconds, again and again while an agent works, quiet under everything. The `hz` of a noise note is how fast its
 * shift register steps: higher is brighter. */
constexpr uint16_t kTap = 15000;   // a key's tick
constexpr uint16_t kClack = 5000;  // a key bottoming out
constexpr uint16_t kSpace = 2600;  // the space bar
constexpr Note kWorking[] = {
    {NOI, kTap, 0, 22, 150},   {NOI, 0, 0, 95, 0},    {NOI, kTap, 0, 22, 140},   {NOI, 0, 0, 120, 0},
    {NOI, kClack, 0, 38, 200}, {NOI, 0, 0, 80, 0},    {NOI, kClack, 0, 38, 190}, {NOI, 0, 0, 160, 0},
    {NOI, kTap, 0, 22, 150},   {NOI, 0, 0, 70, 0},    {NOI, kTap, 0, 22, 135},   {NOI, 0, 0, 85, 0},
    {NOI, kTap, 0, 22, 145},   {NOI, 0, 0, 110, 0},   {NOI, kSpace, 0, 50, 190}, {NOI, 0, 0, 260, 0},
    {NOI, kTap, 0, 22, 150},   {NOI, 0, 0, 100, 0},   {NOI, kClack, 0, 38, 195}, {NOI, 0, 0, 90, 0},
    {NOI, kTap, 0, 22, 140},   {NOI, 0, 0, 75, 0},    {NOI, kTap, 0, 22, 150},   {NOI, 0, 0, 95, 0},
    {NOI, kClack, 0, 38, 200}, {NOI, 0, 0, 85, 0},    {NOI, kClack, 0, 38, 185}, {NOI, 0, 0, 520, 0},
};

/* THE SPIN: the mascot's spin-jump (AGENT_DECK 9.3), a request allowed — its music, on the animation's beats: a warble
 * through the turn in the air from 490 ms, and a ta-da as he bounces back up off the landing at 800. Its body — the launch, the turns, the
 * impact — sounds as the moments the mascot tells (kFx*), as every move's does. */
constexpr Note kSpin[] = {
    {P25, 0, 0, 490, 0},    {P25, G6, 0, 30, 170}, {P25, E6, 0, 30, 160}, {P25, G6, 0, 30, 170}, {P25, E6, 0, 30, 160},
    {P25, G6, 0, 30, 170},  {P25, E6, 0, 30, 160}, {P25, G6, 0, 30, 170}, {P25, 0, 0, 100, 0},  {P25, C6, 0, 70, 210},
    {P50, G6, 0, 290, 230},
};

/* THE SKIP AND THE WALK as phrases — kept for the host's renders; on the board their steps, hops and landings sound as
 * the moments the mascot tells (kFxStep, kFxHop, kFxLand), as every move's do. The jump's sound with the skip, a like
 * sound with the walk, the developer's ask, 2026-11-02 — each on the animation's own beats (widgets/mascot.cpp's kSkip and kSkipHops, kWalk and kWalkLegs), as the spin's are.
 * A jump is a "bwip", the thin pulse sliding up, at a push-off; a landing a short high tick of noise, the last one home
 * a lower thud; a footstep a soft tick of noise, a little lower every other step, left and right. Made from the keys'
 * times: the skip's eight push-offs 60 ms after each wind-up ends and its landings, the walk's twelve steps 270 ms into
 * each 300 ms step, as the foot comes down, and its hop home — launched at 5160 ms, landed at 5680. */
constexpr Note kSkip[] = {
    {P12, 0, 0, 380, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 65, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 285, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 65, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 65, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 65, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 285, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 65, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 210, 0}, {NOI, 1500, 0, 50, 220},
};

constexpr Note kWalk[] = {
    {NOI, 0, 0, 970, 0}, {NOI, 2600, 0, 25, 120}, {NOI, 0, 0, 275, 0}, {NOI, 2200, 0, 25, 115},
    {NOI, 0, 0, 275, 0}, {NOI, 2600, 0, 25, 120}, {NOI, 0, 0, 275, 0}, {NOI, 2200, 0, 25, 115},
    {NOI, 0, 0, 675, 0}, {NOI, 2600, 0, 25, 120}, {NOI, 0, 0, 275, 0}, {NOI, 2200, 0, 25, 115},
    {NOI, 0, 0, 275, 0}, {NOI, 2600, 0, 25, 120}, {NOI, 0, 0, 275, 0}, {NOI, 2200, 0, 25, 115},
    {NOI, 0, 0, 275, 0}, {NOI, 2600, 0, 25, 120}, {NOI, 0, 0, 275, 0}, {NOI, 2200, 0, 25, 115},
    {NOI, 0, 0, 275, 0}, {NOI, 2600, 0, 25, 120}, {NOI, 0, 0, 275, 0}, {NOI, 2200, 0, 25, 115},
    {P12, 0, 0, 465, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 440, 0}, {NOI, 1500, 0, 50, 220},
};

/* THE THINKING: the idle's lean to look, one idle loop of theme::kMascotIdleMs (5 s) — the developer's ask, 2026-11-02: a
 * low "boop boop boop" like it's thinking, on the triangle, as he settles into his look left (from 700 ms) and again,
 * a step higher, into his look right (from 2900 ms); quiet, the rest of the loop silent. */
constexpr uint16_t E3 = 165, G3 = 196, A3 = 220;
constexpr Note kThink[] = {
    {TRI, 0, 0, 700, 0},   {TRI, G3, 0, 110, 200}, {TRI, 0, 0, 90, 0},     {TRI, E3, 0, 110, 190}, {TRI, 0, 0, 90, 0},
    {TRI, G3, 0, 130, 200}, {TRI, 0, 0, 1670, 0},  {TRI, A3, 0, 110, 200}, {TRI, 0, 0, 90, 0},     {TRI, G3, 0, 110, 190},
    {TRI, 0, 0, 90, 0},    {TRI, A3, 0, 130, 200},
};

/* THE HOPS AND THE DUST JUMP, as phrases on their keys' times — kept for the host's renders; on the board their
 * launches and landings sound as the moments the mascot tells (kFxHop, kFxLand), as every move's do. THE WAVE'S "HI"S:
 * a chirp on each of a hand's six swings from 330 ms, a tone apart by turns, the other hand the same from 2200 — the
 * one part of the wave its pose cannot tell, a hand swinging side to side; its raises and flaps are told (kFxHand,
 * kFxHandDown). */
constexpr Note kHop[] = {
    {P12, 0, 0, 680, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 490, 0}, {NOI, 4000, 0, 25, 140},
};

constexpr Note kHops[] = {
    {P12, 0, 0, 680, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 490, 0}, {NOI, 4000, 0, 25, 140},
    {P12, 0, 0, 1538, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 490, 0}, {NOI, 4000, 0, 25, 140},
};

constexpr Note kWave[] = {
    {P25, 0, 0, 330, 0}, {P25, 1760, 0, 40, 150}, {P25, 0, 0, 160, 0}, {P25, 1568, 0, 40, 145},
    {P25, 0, 0, 160, 0}, {P25, 1760, 0, 40, 150}, {P25, 0, 0, 160, 0}, {P25, 1568, 0, 40, 145},
    {P25, 0, 0, 160, 0}, {P25, 1760, 0, 40, 150}, {P25, 0, 0, 160, 0}, {P25, 1568, 0, 40, 145},
    {P25, 0, 0, 1160, 0}, {P25, 1760, 0, 40, 150}, {P25, 0, 0, 160, 0}, {P25, 1568, 0, 40, 145},
    {P25, 0, 0, 160, 0}, {P25, 1760, 0, 40, 150}, {P25, 0, 0, 160, 0}, {P25, 1568, 0, 40, 145},
    {P25, 0, 0, 160, 0}, {P25, 1760, 0, 40, 150}, {P25, 0, 0, 160, 0}, {P25, 1568, 0, 40, 145},
};

constexpr Note kDust[] = {
    {P12, 0, 0, 290, 0}, {P12, 380, 880, 80, 200}, {NOI, 0, 0, 270, 0}, {NOI, 2200, 0, 45, 200},
    {P12, 0, 0, 55, 0}, {P12, 500, 1000, 60, 180}, {NOI, 0, 0, 250, 0}, {NOI, 2200, 0, 45, 200},
};

/* THE TIMED OUT: the answer did not come in time — the developer's ask, 2026-11-02: a gentle "womp womp", two notes
 * down on the soft square and the last sliding lower, deflated rather than alarming. 0.86 s. */
constexpr uint16_t A4 = 440, C5 = 523, E5 = 659, E4 = 330;
constexpr Note kTimedOut[] = {
    {P50, E5, 0, 150, 215}, {P50, 0, 0, 50, 0}, {P50, C5, 0, 150, 215}, {P50, 0, 0, 50, 0},
    {P50, A4, E4, 460, 225},
};

/* THE SUBMITTED: answered in the terminal, the request handed off — the developer's ask, 2026-11-02: a swoosh of noise
 * rising, then a bright, thin "ding" left to ring out. 0.4 s — the request screen's kSubmittedMs copies it, and its
 * closed page's kClosedCueMs waits it out: a longer sound changes both (AGENT_DECK 4.3). */
constexpr uint16_t E7 = 2637;
constexpr Note kSubmitted[] = {
    {NOI, 3000, 12000, 120, 110},
    {P12, E7, 0, 280, 210},
};

/* THE BOOP: an agent at work — the developer's ask, 2026-11-03: one round, low note on the full square, sagging a little,
 * on an effect voice over whatever plays. 0.12 s. */
constexpr Note kBoop[] = {{P50, 560, 440, 120, 255}};

/* THE VOLUME'S BLOOP: the settings' Sound turned — the developer's ask, 2026-11-04, so the ear hears how loud 12 % is
 * against 20 %: one round note on the full square, sliding up an octave, 440 to 880 Hz, at full level, so each step
 * sounds as loud as the volume it chose. 0.07 s, under the steps' 90 ms gap, so a spin is a run of bloops. */
constexpr Note kVolume[] = {{P50, 440, 880, 70, 255}};

/* THE USAGE LIMIT: the plan's limit hit, the turn stopped — the developer's ask and pick, 2026-11-10, of three drafts:
 * the sad trombone, "wah wah wah waaah". Four notes a semitone apart down the full square, D4 to B3, each sagging a
 * little; the last held and wobbling, a run of short notes sliding either side of it, fading. Low and slow, so it is
 * never mistaken for the timed out's quicker, higher "womp womp". 1.39 s. */
constexpr uint16_t D4 = 294, Cs4 = 277, C4 = 262, B3 = 247;
constexpr Note kUsageLimit[] = {
    {P50, D4, 286, 230, 225},  {P50, 0, 0, 40, 0},
    {P50, Cs4, 270, 230, 225}, {P50, 0, 0, 40, 0},
    {P50, C4, 255, 230, 225},  {P50, 0, 0, 40, 0},
    {P50, B3, 240, 70, 235},   {P50, 240, 250, 70, 230}, {P50, 250, 238, 70, 225}, {P50, 238, 248, 70, 215},
    {P50, 248, 236, 70, 200},  {P50, 236, 245, 70, 185}, {P50, 245, 225, 160, 170},
};

/* EVERY MOVEMENT ITS SOUND — the developer's rule, 2026-11-02 — on the effect voices over whatever plays, each as short
 * as its moment (widgets::MascotSfx). The eyes: a blink a tiny high pip; ^ ^ a two-note twinkle up; > < a squish, a
 * slide down; the wink a tink and a higher one; a glance a faint tick. The body: a hop a little "bwip", the thin pulse
 * sliding up; a landing a soft tick of noise; a lean a breath of noise; relief a soft "aah" rising; a hand raised a
 * chirp; the wipe a swish. The props: the laptop set down a dull thud; its lid flipped up a click sliding up; snapped
 * shut a clap; the flag a "fwip"; the confetti a sparkle up the chord; the dust a soft burst. The sweat: its beads a
 * plip sliding down, and bursting a bubble's pop, sliding up fast. */
constexpr uint16_t G7 = 3136, C8 = 4186; // E7 with the submitted, above
constexpr Note kFxBlink[] = {{P12, C7, 0, 18, 110}};
constexpr Note kFxHappy[] = {{P25, C7, 0, 40, 150}, {P25, E7, 0, 60, 160}};
constexpr Note kFxChevron[] = {{P50, 520, 260, 60, 150}};
constexpr Note kFxWink[] = {{P12, E7, 0, 25, 140}, {P12, 0, 0, 20, 0}, {P12, G7, 0, 35, 150}};
constexpr Note kFxLook[] = {{P12, 2600, 0, 10, 80}};
constexpr Note kFxHop[] = {{P12, 420, 820, 60, 160}};
constexpr Note kFxLand[] = {{NOI, 3500, 0, 22, 130}};
constexpr Note kFxSway[] = {{NOI, 5000, 0, 80, 60}};
constexpr Note kFxStretch[] = {{P50, 300, 500, 120, 110}};
constexpr Note kFxHand[] = {{P25, E6, 0, 22, 120}};
constexpr Note kFxWipe[] = {{NOI, 7000, 0, 70, 90}};
constexpr Note kFxLaptopDown[] = {{NOI, 1200, 0, 40, 170}};
constexpr Note kFxLidOpen[] = {{P25, 700, 1100, 35, 130}};
constexpr Note kFxLidShut[] = {{NOI, 2000, 0, 30, 170}};
constexpr Note kFxFlag[] = {{P12, 600, 1500, 70, 150}};
constexpr Note kFxBurst[] = {{P12, C7, 0, 15, 130}, {P12, E7, 0, 15, 130}, {P12, G7, 0, 15, 135}, {P12, C8, 0, 25, 140}};
constexpr Note kFxPuff[] = {{NOI, 2000, 0, 40, 150}};
constexpr Note kFxBead[] = {{P12, 1800, 1400, 30, 100}};
constexpr Note kFxPop[] = {{P12, 900, 2400, 25, 170}};
constexpr Note kFxStep[] = {{NOI, 2600, 0, 25, 115}};
constexpr Note kFxTurn[] = {{NOI, 6000, 0, 45, 90}};
constexpr Note kFxHandDown[] = {{NOI, 2500, 0, 40, 160}};
constexpr Note kFxCrouch[] = {{P50, 220, 160, 40, 110}};
/* The faceplant (AGENT_DECK 9.3): the topple a thin pulse falling 640 to 160 Hz, what falls sliding down; the flop the
 * biggest impact of any move, so the lowest and longest: a burst of noise, then a low thump sinking 140 to 70 Hz. */
constexpr Note kFxTopple[] = {{P25, 640, 160, 110, 150}};
constexpr Note kFxFlop[] = {{NOI, 1800, 0, 30, 235}, {P50, 140, 70, 75, 225}};

constexpr Sound kSounds[] = {
    {kStartup.data(), kStartup.size(), false},  {kConnected, std::size(kConnected), false},
    {kSuccess, std::size(kSuccess), false},     {kAttention, std::size(kAttention), false},
    {kWorking, std::size(kWorking), true},      {kSpin, std::size(kSpin), false},
    {kSkip, std::size(kSkip), false},           {kWalk, std::size(kWalk), false},
    {kThink, std::size(kThink), false},         {kHop, std::size(kHop), false},
    {kHops, std::size(kHops), false},           {kWave, std::size(kWave), false},
    {kDust, std::size(kDust), false},
    {kTimedOut, std::size(kTimedOut), false},
    {kSubmitted, std::size(kSubmitted), false},
    {kBoop, std::size(kBoop), false},
    {kVolume, std::size(kVolume), false},
    {kUsageLimit, std::size(kUsageLimit), false},
    {kFxBlink, std::size(kFxBlink), false},
    {kFxHappy, std::size(kFxHappy), false},
    {kFxChevron, std::size(kFxChevron), false},
    {kFxWink, std::size(kFxWink), false},
    {kFxLook, std::size(kFxLook), false},
    {kFxHop, std::size(kFxHop), false},
    {kFxLand, std::size(kFxLand), false},
    {kFxSway, std::size(kFxSway), false},
    {kFxStretch, std::size(kFxStretch), false},
    {kFxHand, std::size(kFxHand), false},
    {kFxWipe, std::size(kFxWipe), false},
    {kFxLaptopDown, std::size(kFxLaptopDown), false},
    {kFxLidOpen, std::size(kFxLidOpen), false},
    {kFxLidShut, std::size(kFxLidShut), false},
    {kFxFlag, std::size(kFxFlag), false},
    {kFxBurst, std::size(kFxBurst), false},
    {kFxPuff, std::size(kFxPuff), false},
    {kFxBead, std::size(kFxBead), false},
    {kFxPop, std::size(kFxPop), false},
    {kFxStep, std::size(kFxStep), false},
    {kFxTurn, std::size(kFxTurn), false},
    {kFxHandDown, std::size(kFxHandDown), false},
    {kFxCrouch, std::size(kFxCrouch), false},
    {kFxTopple, std::size(kFxTopple), false},
    {kFxFlop, std::size(kFxFlop), false},
};
static_assert(std::size(kSounds) == static_cast<size_t>(SoundId::kFxFlop) + 1, "a sound for every SoundId");

/* A note's shape: up in kAttackMs, decaying to kSustain of its peak by its end, off over its last kReleaseMs — the
 * pluck of a game's pulse channel, and no click at either end. */
constexpr uint32_t kAttackMs = 3;
constexpr uint32_t kReleaseMs = 8;
constexpr float kSustain = 0.35f;

/* -1..1 for a phase, the whole cycle 2^32. */
float wave_at(Wave w, uint32_t phase)
{
    switch (w) {
    case Wave::kPulse50: return phase < 0x80000000u ? 1.0f : -1.0f;
    case Wave::kPulse25: return phase < 0x40000000u ? 1.0f : -1.0f;
    case Wave::kPulse12: return phase < 0x20000000u ? 1.0f : -1.0f;
    case Wave::kNoise: return 0.0f; /* the shift register's, in render() */
    case Wave::kTriangle: {
        /* The NES triangle: 32 steps a cycle, down 15..0 and up 0..15. */
        const uint32_t step = phase >> 27;
        const uint32_t v = step < 16 ? 15 - step : step - 16;
        return static_cast<float>(v) / 7.5f - 1.0f;
    }
    }
    return 0.0f;
}

} // namespace

const Sound &sound(SoundId id) { return kSounds[static_cast<size_t>(id)]; }

void Player::start(const Sound &sound, int16_t level, uint32_t from_ms)
{
    sound_ = sound.count > 0 ? &sound : nullptr;
    level_ = level;
    note_ = 0;
    phase_ = 0;
    /* Whole notes skipped, then into the one `from_ms` falls in. */
    while (sound_ != nullptr && from_ms >= sound_->notes[note_].ms) {
        from_ms -= sound_->notes[note_].ms;
        if (++note_ == sound_->count) {
            sound_ = nullptr;
        }
    }
    begin_note();
    at_ = std::min(from_ms * rate_ / 1000, len_);
}

void Player::begin_note()
{
    at_ = 0;
    len_ = sound_ != nullptr ? sound_->notes[note_].ms * rate_ / 1000 : 0;
}

size_t Player::render(int16_t *out, size_t frames)
{
    size_t n = 0;
    const uint32_t attack = kAttackMs * rate_ / 1000;
    const uint32_t release = kReleaseMs * rate_ / 1000;
    while (n < frames && sound_ != nullptr) {
        if (at_ >= len_) {
            if (++note_ == sound_->count) {
                if (!sound_->loops) {
                    sound_ = nullptr;
                    break;
                }
                note_ = 0;
            }
            begin_note();
            continue;
        }
        const Note &note = sound_->notes[note_];
        float sample = 0.0f;
        if (note.hz != 0) {
            const float t = static_cast<float>(at_) / static_cast<float>(len_);
            const float hz = note.hz_end != 0 ? note.hz + (note.hz_end - note.hz) * t : note.hz;
            const uint32_t before = phase_;
            phase_ += static_cast<uint32_t>(hz * 4294967296.0f / static_cast<float>(rate_));
            const bool noise = note.wave == Wave::kNoise;
            if (noise && phase_ < before) {
                /* A step of the NES's long noise: bit 0 XOR bit 1 shifted in at the top of 15 bits. */
                const uint16_t bit = (lfsr_ ^ (lfsr_ >> 1)) & 1u;
                lfsr_ = static_cast<uint16_t>((lfsr_ >> 1) | (bit << 14));
            }
            /* A pulse or the triangle decays to kSustain and holds; a noise burst dies away, as a key's click does. */
            float env = noise ? (1.0f - t) * (1.0f - t) : 1.0f - (1.0f - kSustain) * t;
            if (at_ < attack) {
                env *= static_cast<float>(at_) / static_cast<float>(attack);
            }
            const uint32_t left = len_ - at_;
            if (left < release) {
                env *= static_cast<float>(left) / static_cast<float>(release);
            }
            const float v = noise ? ((lfsr_ & 1u) != 0 ? 1.0f : -1.0f) : wave_at(note.wave, phase_);
            sample = v * env * static_cast<float>(note.volume) / 255.0f;
        }
        const auto s = static_cast<int16_t>(std::clamp(sample * level_, -32767.0f, 32767.0f));
        out[2 * n] = s;
        out[2 * n + 1] = s;
        at_++;
        n++;
    }
    return n;
}

} // namespace chiptune
