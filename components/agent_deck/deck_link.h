// The agent deck's side of the link to the bridge on the Mac (STICKY_AGENT_DECK §10): the bridge's records into the model, the person's
// answers out, as lines on the USB cable. LVGL only, so the host tests build it, and every call runs on LVGL's task under its lock. Ported
// from the agent deck's deck_link, its protocol unchanged — the bridge's own design doc, in its sections on the link, is its home.
//
// A line is kPrefix, the CRC-32 of the JSON in eight hex digits, a space, then one JSON object:
//
//     @deck 497c309c {"type":"ping"}
//
// In, from the bridge: "agent", "remove", "usage", "request", "question", "close" with the model's fields and limits; "reset" (the
// snapshot's first line: everything forgotten and sent again, the deck off standby); "standby" (another deck is the active one); "ping"
// (every kPingMs); "ack" (an answer reached it, with a question's "part"); "hello" (the bridge opened the port: say hello). Out, from
// the deck: "hello" (the deck started, lost the bridge or was asked), "decision" and "answer" (each sent again every kResendMs until
// its "ack", kMaxSends times at most) and "use" (the standby page's Connect, sent once). The Wi-Fi records come with the Wi-Fi port.

#pragma once

#include <cstddef>
#include <cstdint>

namespace agent_deck::link {

inline constexpr const char *kPrefix = "@deck ";
// How often the bridge pings, and how long the deck waits past that before it takes the bridge for gone: four pings, since the longest
// line takes about 4.4 s on the cable at 115200 baud.
inline constexpr uint32_t kPingMs = 2000;
inline constexpr uint32_t kLostMs = 4 * kPingMs;
inline constexpr uint32_t kResendMs = 1000;
inline constexpr int kMaxSends = 10;
// A decision, an answer or a hello, built on LVGL's stack.
inline constexpr size_t kMaxShortOut = 160;

// Writes one line to the cable; false when it could not.
using SendFn = bool (*)(const char *line, size_t len);

// THE HELLO: version 5 — 2 takes "question", 3 the state "idle" and `active`, 4 names the deck (`id`, `board`, `link`), 5 takes
// "standby" and sends "use". Each field is 1 to kMaxHelloField characters of a–z, 0–9 and '-'. `body` is the longest request body the
// deck takes, model::kBodyBytes.
inline constexpr int kHelloVersion = 5;
inline constexpr size_t kMaxHelloField = 16;
struct Hello {
    const char *id;     // the deck's own: its Wi-Fi MAC's last two bytes in hex
    const char *board;  // "sticky"
    const char *link;   // "usb" on the cable
};

// Once, under the LVGL lock: where lines go, who the deck is, and the timer that keeps the link's time; sends a hello. False when a
// field of `hello` breaks its rule, or with no timer to give.
bool Init(SendFn send, const Hello &hello);

// A line that came in, after kPrefix. False when its CRC or JSON is wrong, its JSON nests deeper than two levels, or it is not a
// record the deck knows: the line is dropped and nothing changes. A good line keeps the link up.
bool Take(const char *line, size_t len);

// The person's answer to request `id`, sent now and again until the bridge acknowledges it.
void Decided(uint32_t id, bool allow);
// The person's answer to part `part` of question `id`, bit n for option n, 0 to leave it to the terminal; sent the same way.
void Answered(uint32_t id, uint8_t part, uint8_t chosen);
// The standby page's Connect: "make me the active deck", sent once. False when the line could not be written.
bool Use();

// Whether the bridge has been heard within kLostMs.
bool Up();

// The CRC-32 the line carries (IEEE 802.3, as Python's zlib.crc32).
uint32_t Crc32(const char *data, size_t len);
// `json` as a line, kPrefix and CRC before it; its length, or 0 when it does not fit `cap`.
size_t Encode(char *out, size_t cap, const char *json);

}  // namespace agent_deck::link
