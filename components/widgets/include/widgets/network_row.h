// A Wi-Fi network row: a network's name over its facts, and its signal at the right end. The connected network and the one
// being joined are the chosen inversion; a press on a row is the request to join it (DESIGN → Components → The kit).

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace widgets {

// A network in range is at rest; the one in use is connected (a check before "Connected"); the one being joined shows
// "Connecting…" and no check. A failed join is no row state: the row goes back to rest and a dialog says why.
enum class NetworkState : uint8_t { kRest, kConnected, kConnecting };

// How the row in use or being joined is drawn: the chosen inversion, or — where a screen asks for the outlined look, the agent deck's
// settings and Wi-Fi page — a white row in a `spacing.stroke.default` black edge with every word and glyph black (DESIGN → Components →
// The kit → Network row). A network at rest is the same either way.
enum class NetworkLook : uint8_t { kInverted, kOutlined };

// What a name too long for the row does: end in three full stops (`kDots`, the default), or rest and step as a stepped line (`kStepped`,
// widgets/marquee.h; the agent deck's Wi-Fi page and settings), at its start after its passes, the row being a control. `key` names the
// row across a page built again, `MarqueeKey(scope, owner, field)`; 0 for none.
enum class NetworkName : uint8_t { kDots, kStepped };

// A row `spacing.list-row-height` high, full width, `rounded.lg`, in the focus group with its ring outside it: the name in
// `caption-strong`, up to 28 characters and then three full stops; under it the facts line in `caption` — an open network says
// "Open", a secured one carries the lock before "Locked", the connected one a check before "Connected" and then its lock
// or "Open", the joining one "Connecting…" — and, at the right end, the signal: its glyph alone, centred on the row's height, with no
// word. `signal` is 0 to 3 bars lit; the unlit bars stay in the light dither (the dark dither on an inverted row),
// the lit ones black (white on an inverted row). The caller owns what a click does (LV_EVENT_CLICKED), and puts the row in a
// list view's root when there are more than a page of them.
// `look` is the row's for good: SetNetworkRow keeps it. On the outlined look the row's padding gives up the edge's width, so the words stand
// where they stand at rest; the unlit bars are the light dither on the white ground, as at rest; the lit bars, the check, the padlock and
// every word are black.
lv_obj_t *MakeNetworkRow(lv_obj_t *parent, const char *name, bool locked, int signal, NetworkState state,
                         NetworkLook look = NetworkLook::kInverted, NetworkName name_fit = NetworkName::kDots, uint32_t key = 0);
// A network's facts changed: its lock, its signal or its state. The row keeps its place, its focus and its look.
void SetNetworkRow(lv_obj_t *row, bool locked, int signal, NetworkState state);

}  // namespace widgets
