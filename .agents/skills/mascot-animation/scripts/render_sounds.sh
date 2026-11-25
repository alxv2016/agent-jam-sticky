#!/usr/bin/env bash
# Builds render_sounds.cpp against the host's synth (the host's side, read by host_config.py: synth_include, synth_source)
# and runs it: hear the deck's sounds off the device.
#   render_sounds.sh all <dir>                 every sound to <dir>
#   render_sounds.sh track <events.log> <wav>  a scenario's mascot moments as the board would mix them
# The binary goes to $TMPDIR; run from anywhere.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../../../.." && pwd)"
bin="${TMPDIR:-/tmp}/deck_render_sounds"
cfgpy="$here/host_config.py"
opt() { python3 "$cfgpy" "$1"; }  # a key of the host's side: empty when the host leaves it out
cfg() {  # a key the build needs: refused when the host has none yet
    local v; v="$(opt "$1")"
    [ -n "$v" ] || { echo "render_sounds.sh: the host names no $1 yet (the host's side)" >&2; exit 2; }
    printf '%s\n' "$v"
}
defs=()  # the synth's header and namespace, where the host's differ from the defaults in render_sounds.cpp
[ -n "$(opt synth_header)" ] && defs+=("-DMASCOT_SYNTH_HEADER=\"$(opt synth_header)\"")
[ -n "$(opt synth_namespace)" ] && defs+=("-DMASCOT_SYNTH_NAMESPACE=$(opt synth_namespace)")
inc="$(cfg synth_include)" || exit 2
srcs=()  # the synth's source; a buzzer's note rules are header-only
[ -n "$(opt synth_source)" ] && srcs+=("$repo/$(opt synth_source)")
incs=(-I "$repo/$inc")
for extra in $(opt synth_extra_include); do incs+=(-I "$repo/$extra"); done
if [ "$(opt synth_kind)" = buzzer ]; then  # the flipbook's pace: a frame's moments as the next one starts (MASCOT §6)
    defs+=(-DMASCOT_SYNTH_BUZZER "-DMASCOT_FRAME_MS=$(opt frame_ms)")
fi
c++ -std=c++20 -O2 -Wall -Wextra -Werror ${defs[@]+"${defs[@]}"} "${incs[@]}" \
    "$here/render_sounds.cpp" ${srcs[@]+"${srcs[@]}"} -o "$bin"
[ "${1:-}" = all ] && mkdir -p "${2:?usage: render_sounds.sh all <dir>}"
exec "$bin" "$@"
