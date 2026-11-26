// The glyphs of the buttons: BYTE-90's icon art at one bit on the `spacing.icon` box, compiled into the widgets component
// by marks/make_marks.py (DESIGN → Components → Icons). A glyph has one meaning everywhere, and another glyph needs the
// person's yes and a place in DESIGN first.

#pragma once

#include <cstdint>

namespace widgets {

// The glyphs a button may carry beside its words: Record (the microphone), Play, Back (the arrow), Save (the check) and
// Retry (the refresh) — and the others of the set, which only an icon button takes: kWifi and kSettings are the agent deck's own
// art, and kPower is BYTE-90's power glyph — the foot's three buttons. kNone is no glyph. The bridge link's two faces are no glyph of a button: the link indicator draws them
// (link_indicator.h).
enum class Glyph : uint8_t { kNone, kBack, kClose, kPlay, kPause, kStop, kSoundOn, kSoundOff, kCheck, kMic, kRefresh, kEyeOpen, kEyeSlashed,
                             kWifi, kSettings, kPower };

// The glyphs an icon button — the one with no word — may be: DESIGN → Exceptions → The icon button's closed set. Sound on
// and off is the toggle icon button's own pair. A glyph outside the set does not convert, so it does not compile. kClear is
// Close's one deleting use, the cross inside a text field that empties it; kEyeOpen and kEyeSlashed are a password field's
// show and hide, each the ACTION a press takes — the open eye while the text is hidden, the slashed eye while it shows. kWifi and
// kSettings open the Wi-Fi page and the settings from the agent deck's foot, and kPower turns the deck off from it (DESIGN → Exceptions →
// The icon button).
enum class IconAction : uint8_t { kBack, kClose, kPlay, kPause, kStop, kClear, kEyeOpen, kEyeSlashed, kWifi, kSettings, kPower };

}  // namespace widgets
