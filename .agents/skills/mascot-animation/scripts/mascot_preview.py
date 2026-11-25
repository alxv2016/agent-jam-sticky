#!/usr/bin/env python3
"""Draft previews of a mascot move — stills on a contact sheet and an animated GIF on the host's glass — for the
developer to approve before the move is built (the mascot-animation skill; MASCOT §1–§3 in references/character.md).

Pure Python, no packages: it reads the mascot's colours from the host's theme header (its Mascot* roles), the glass
from the host's board config, and the work loop's and the flag's pixel poses from the mascot's frames header, so a
draft paints what the firmware will. Every path and name is the host's (host_config.py: .agents/project.py → MASCOT,
else .agents/project/config.json → mascot: theme_h, cell_name, cell, board_h, width_define, height_define, corner_px,
home_column, frames_h, backdrop, background, glass, frame_ms, dither).

THE GLASS is the host's kind (`glass`): "frames", an LCD drawn every frame at 30 fps, or "flipbook", e-paper, where a
move is its key poses, each held one refresh of `frame_ms`, drawn one bit a pixel with its greys as the host's dithers
(`dither`, locked to the glass's pixels) — MASCOT §6. MASCOT_THEME_H points the colours at a draft's header, for a
role not yet in the host's design system.

A draft is a small script in the scratchpad that imports this module, describes the move as a function of time —
`pose(t) -> Pose` — and calls `sheet()`, `gif()` and `reach()`. examples/finish.py is the approved finish, a
worked example to copy from.

Coordinates are the pixel style's GIF grid: one pixel is half a mascot cell; at rest the body is columns 17–32, rows
10–21, the hands 4 x 4 half behind it (columns 15–18 and 31–34, rows 14–17), the eyes 2 x 2 at (19, 12) and (29, 12),
the legs 2 x 4 at columns 17, 21, 27, 31 on rows 22–25; the ground is row 26 and the body's centre line column 25.

THE CELL is the host's mascot cell, read from its theme header (cell_name); MASCOT_CELL overrides it to draft at
another size.

THE FRAMES are the host's frames header (frames_h); in a tree without them, set MASCOT_FRAMES_H to a copy of one. Only
a draft that uses the work loop's or the flag's poses reads them.

The glass is the host's panel, its corners rounded at corner_px: a pixel inside a corner arc is written but not seen.
A move is held to the glass — nothing above the top, below the foot or in a corner; only a move that travels (the
walk, the skip) leaves by a side, and comes back from the other. reach() reports all three.

  MASCOT_CELL=16 python3 mascot_preview.py check <draft.gif>   decodes every frame (a GIF a viewer cannot play)
"""
import math
import os
import re
import struct
import sys
import zlib
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host_config  # noqa: E402

ROOT = host_config.ROOT
_CFG = host_config.load()
THEME_H = Path(os.environ["MASCOT_THEME_H"]) if os.environ.get("MASCOT_THEME_H") else ROOT / _CFG["theme_h"]
BOARD_H = ROOT / _CFG["board_h"]
FRAMES_H = ROOT / (_CFG.get("frames_h") or "-")  # None: the host has no mascot widget yet
if not FRAMES_H.exists() and os.environ.get("MASCOT_FRAMES_H"):
    FRAMES_H = Path(os.environ["MASCOT_FRAMES_H"])
DEFAULT_BACKDROP = ROOT / (_CFG.get("backdrop") or "-")
DEFAULT_CELL = _CFG.get("cell") or 16  # where the theme header does not name the cell (yet)
GLASS = _CFG.get("glass", "frames")    # "frames" (an LCD, 30 fps) or "flipbook" (e-paper: key poses, a refresh each)
FRAME_MS = (_CFG.get("frame_ms") or 1000 / 30) if GLASS == "flipbook" else 1000 / 30
GLASS_CORNER = _CFG["corner_px"]  # the glass's corner radius, px
# Where the mascot stands: centred on the glass, or — where the host names a column, two of the theme's tokens for
# its left and right edges (home_column) — centred in that column, the glass's height centred too.
HOME_COLUMN = _CFG.get("home_column")

# ---- the grid --------------------------------------------------------------------------------------------------------
def _board_int(name):
    """An integer from the board config, as the firmware has it: a `#define <WIDTH_DEFINE> 460`, or a constant
    `inline constexpr int32_t kScreenWidth = 480;`."""
    text = BOARD_H.read_text()
    m = re.search(r"#define\s+" + name + r"\s+(\d+)", text) or re.search(r"\b" + name + r"\s*=\s*(\d+)\s*;", text)
    if not m:
        raise SystemExit(f"{name} not found in {BOARD_H}")
    return int(m.group(1))


def _cell():
    if os.environ.get("MASCOT_CELL"):
        return int(os.environ["MASCOT_CELL"])
    # The cell may sit in the theme header or, where the host's layout header sets it per glass, in that one.
    m = re.search(r"\b" + re.escape(_CFG.get("cell_name") or "-") + r"\s*=\s*(\d+)\s*;",
                  THEME_H.read_text() + BOARD_H.read_text())
    if m:
        return int(m.group(1))
    return DEFAULT_CELL


CELL = _cell()
if CELL % 2:
    raise SystemExit(f"MASCOT_CELL {CELL} is odd: the mascot's pixel is half a cell, so a cell must be even")
PX = CELL // 2               # a mascot pixel on the glass
BODY_COL, BODY_ROW = 17, 10  # the body's top-left at rest
PIVOT_COL = 25               # the body's centre line
HIP_ROW = 22                 # the body's bottom edge, where the legs start
GROUND_ROW = 26              # under the feet
LEG_COLS = (17, 21, 27, 31)
PANEL_W = _board_int(_CFG["width_define"])
PANEL_H = _board_int(_CFG["height_define"])
if HOME_COLUMN:
    _t = THEME_H.read_text() + BOARD_H.read_text()  # the column's tokens may sit in either: a layout header holds them
    HOME_LEFT, HOME_RIGHT = (int(re.search(r"\b" + n + r"\s*=\s*(\d+)\s*;", _t).group(1)) for n in HOME_COLUMN)
else:
    HOME_LEFT, HOME_RIGHT = 0, PANEL_W
# A POSE'S DISTANCES — lift, x, sway, crouch, swing — are px at the reference cell, 8 px (the widget's kFinishRefCell),
# as the firmware's keys are; the mascot scales them to its cell, and so does the draft.
KEY_SCALE = 2 * PX / 8

# ---- colours, from the theme ----------------------------------------------------------------------------------------
_SCALE_RE = re.compile(r"k(\w+)Scale\[kScaleSteps\]\s*=\s*\{([^}]*)\}", re.S)


def _hex(v):
    return (v >> 16, (v >> 8) & 255, v & 255)


def _theme():
    text = THEME_H.read_text()
    scales = {name.lower(): [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{6})", body)]
              for name, body in _SCALE_RE.findall(text)}
    # A role is `MascotBody()` or `mascot_body()`, as the host names its functions; both are kept as `MascotBody`.
    camel = lambda n: n if n.startswith("Mascot") else "".join(w.capitalize() for w in n.split("_"))
    fn = r"inline lv_color_t (Mascot\w+|mascot_\w+)\(\)\s*\{\s*return "
    roles = {camel(name): int(v, 16) for name, v in
             re.findall(fn + r"lv_color_hex\(0x([0-9A-Fa-f]{6})\);", text)}
    for name, src, n in re.findall(fn + r"(\w+)\((\d*)\);", text):
        if n and src.lower() in scales:
            roles[camel(name)] = scales[src.lower()][int(n)]
            _LABELS[camel(name)] = f"{src.upper()} {n}"
        elif not n and camel(src) in roles:
            roles[camel(name)] = roles[camel(src)]
            _LABELS[camel(name)] = _LABELS.get(camel(src), camel(src))
    # A host whose colours are panel levels names a role as a constant with its hex in a comment — e-paper's
    # `inline constexpr uint8_t kColorMascotBody = kColorScalePrimary0;  // #000000`.
    for name, src, v in re.findall(r"inline constexpr \w+ kColor(Mascot\w+)\s*=\s*(\w+);\s*//\s*#([0-9A-Fa-f]{6})", text):
        roles[name] = int(v, 16)
        m = re.match(r"kColorScale([A-Z][a-z]+)([A-Z][a-z]+|\d+)$", src)
        _LABELS[name] = f"{m.group(1).upper()} {m.group(2).upper()}" if m else src
    return scales, roles


_LABELS = {}  # each role's step as the theme gives it — "ORANGE 5", "PRIMARY 5" — for the model sheet


_S, _ROLES = _theme()


def role_label(name):
    """A mascot role's step as the host's theme names it — role_label('MascotBody') is "PRIMARY 5" here, "ORANGE 5" on
    a host whose scale is called Orange — or the role's own name where the theme gives it a raw colour."""
    return _LABELS.get(name, name)


def role(name):
    """A mascot role as (r, g, b): role('MascotBody') is ui_theme::MascotBody()."""
    if name not in _ROLES:
        raise SystemExit(f"the theme header has no {name}() — the mascot's roles are the host's design system's "
                         "(the host's side: SKILL.md → The host's side)")
    return _hex(_ROLES[name])


def step(scale, n):
    """A neutral step as (r, g, b): step('neutral', 5) is ui_theme::Neutral(5). The mascot's hues are its roles, not
    a scale — this board's Primary is blue — so ask for those by role()."""
    if scale != "neutral":
        raise SystemExit(f"step('{scale}', …): the mascot wears its own roles — use role('MascotBody') and the rest")
    return _hex(_S["neutral"][n])


BLACK = (0, 0, 0)
BACKGROUND = _hex(int((_CFG.get("background") or "#000000").lstrip("#"), 16))  # the glass behind the mascot
EYE = BACKGROUND               # the eyes are holes in the body: the background shows through
BODY = role("MascotBody")
SHADE = role("MascotShade")    # a side turned away
TINT = role("MascotTint")      # a hand in front of the body
LAPTOP = role("MascotLaptop")
FLAG = role("MascotFlag")
CHECK = role("MascotFlagCheck")
POLE = role("MascotFlagPole")
CHARS = {"o": BODY, "s": SHADE, "l": TINT, "k": EYE, "g": LAPTOP}


def frames_text():
    """The mascot's frames header, for a draft that uses the work loop's or the flag's poses."""
    if not FRAMES_H.exists():
        raise SystemExit("no frames header: the mascot's frames are the host widget's (config.json → mascot → "
                         "frames_h); in a tree without them set MASCOT_FRAMES_H to a copy of one")
    return FRAMES_H.read_text()

# ---- PNG and GIF, by hand --------------------------------------------------------------------------------------------


def read_png(path):
    d = Path(path).read_bytes()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", path
    i, idat = 8, b""
    while i < len(d):
        n = struct.unpack(">I", d[i:i + 4])[0]
        t, c = d[i + 4:i + 8], d[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            w, h, bd, ct, _, _, il = struct.unpack(">IIBBBBB", c)
        elif t == b"IDAT":
            idat += c
    assert bd == 8 and il == 0, (bd, il)
    bpp = {2: 3, 6: 4, 0: 1, 4: 2}[ct]
    raw, stride = zlib.decompress(idat), w * bpp
    out, prev, p = [], bytearray(stride), 0
    for _ in range(h):
        f, line = raw[p], bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out.append([tuple(line[x * bpp:x * bpp + 3]) if bpp >= 3 else (line[x * bpp],) * 3 for x in range(w)])
        prev = line
    return out


def write_png(path, img):
    h, w = len(img), len(img[0])
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in img)

    def chunk(t, c):
        return struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c) & 0xFFFFFFFF)
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def _lzw(indices, min_size=8):
    clear, eoi = 1 << min_size, (1 << min_size) + 1
    out, bits, nbits, size = bytearray(), 0, 0, min_size + 1

    def emit(code):
        nonlocal bits, nbits
        bits |= code << nbits
        nbits += size
        while nbits >= 8:
            out.append(bits & 255)
            bits >>= 8
            nbits -= 8
    table, nxt = {(i,): i for i in range(clear)}, eoi + 1
    emit(clear)
    w = (indices[0],)
    for k in indices[1:]:
        wk = w + (k,)
        if wk in table:
            w = wk
            continue
        emit(table[w])
        if nxt < 4096:
            table[wk] = nxt
            nxt += 1
            if nxt > (1 << size) and size < 12:
                size += 1
        else:
            emit(clear)
            table, nxt, size = {(i,): i for i in range(clear)}, eoi + 1, min_size + 1
        w = (k,)
    emit(table[w])
    emit(eoi)
    if nbits:
        out.append(bits & 255)
    return bytes(out)


def write_gif(path, frames, delays_cs):
    """Frames of equal size, each its delay in 1/100 s; only the changed box of each frame is stored."""
    h, w = len(frames[0]), len(frames[0][0])
    pal = {}
    for f in frames:
        for row in f:
            for px in row:
                pal.setdefault(px, len(pal))
    assert len(pal) <= 256, f"{len(pal)} colours; a GIF holds 256"
    o = bytearray(b"GIF89a" + struct.pack("<HHBBB", w, h, 0xF7, 0, 0))
    for c in list(pal) + [(0, 0, 0)] * (256 - len(pal)):
        o += bytes(c)
    o += b"\x21\xFF\x0bNETSCAPE2.0\x03\x01\x00\x00\x00"
    prev = None
    for n, f in enumerate(frames):
        if prev is None:
            x0, y0, x1, y1 = 0, 0, w, h
        else:
            ys = [y for y in range(h) if f[y] != prev[y]] or [0]
            xs = [x for y in ys for x in range(w) if f[y][x] != prev[y][x]] or [0]
            x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
        o += b"\x21\xF9\x04" + struct.pack("<BHBB", 0x04, delays_cs[n], 0, 0)
        o += b"\x2C" + struct.pack("<HHHHB", x0, y0, x1 - x0, y1 - y0, 0)
        data = _lzw([pal[f[y][x]] for y in range(y0, y1) for x in range(x0, x1)])
        o += b"\x08"
        for i in range(0, len(data), 255):
            o += bytes([len(data[i:i + 255])]) + data[i:i + 255]
        o += b"\x00"
        prev = f
    o += b"\x3B"
    Path(path).write_bytes(o)


def check_gif(path):
    """Decodes every frame with a standard LZW decoder; returns the frame count, raises on a broken file."""
    d = Path(path).read_bytes()
    assert d[:6] == b"GIF89a", "not a GIF89a"
    p, n = 13 + 768, 0
    while d[p] != 0x3B:
        if d[p] == 0x21:
            p += 2
            while d[p]:
                p += d[p] + 1
            p += 1
            continue
        assert d[p] == 0x2C, f"bad block at {p}"
        _, _, fw, fh, _ = struct.unpack("<HHHHB", d[p + 1:p + 10])
        p += 10
        ms, data = d[p], bytearray()
        p += 1
        while d[p]:
            data += d[p + 1:p + 1 + d[p]]
            p += d[p] + 1
        p += 1
        clear, eoi = 1 << ms, (1 << ms) + 1
        size, pos, count, prev = ms + 1, 0, 0, None
        table, nxt = {i: [i] for i in range(clear)}, eoi + 1
        while pos + size <= len(data) * 8:
            code = sum(((data[(pos + i) >> 3] >> ((pos + i) & 7)) & 1) << i for i in range(size))
            pos += size
            if code == clear:
                table, nxt, size, prev = {i: [i] for i in range(clear)}, eoi + 1, ms + 1, None
                continue
            if code == eoi:
                break
            if prev is None:
                entry = table[code]
            else:
                entry = table[code] if code < nxt else table[prev] + [table[prev][0]]
                table[nxt] = table[prev] + [entry[0]]
                nxt += 1
                if nxt == (1 << size) and size < 12:
                    size += 1
            count += len(entry)
            prev = code
        assert count == fw * fh, f"frame {n}: {count} pixels, want {fw * fh}"
        n += 1
    return n

# ---- motion ---------------------------------------------------------------------------------------------------------


def ease(kind, x):
    """The firmware's cubic eases (mascot.cpp ease()): 'lin', 'in', 'out', 'inout'."""
    if kind == "in":
        return x ** 3
    if kind == "out":
        return 1 - (1 - x) ** 3
    if kind == "inout":
        return 4 * x ** 3 if x < .5 else 1 - 4 * (1 - x) ** 3
    return x


def track(keys, t):
    """Keys (t, v1, v2, …, ease) — the ease takes the segment into its key, as in mascot.cpp — at time t."""
    i = 0
    while i + 2 < len(keys) and keys[i + 1][0] <= t:
        i += 1
    a, b = keys[i], keys[i + 1]
    x = ease(b[-1], min(max((t - a[0]) / (b[0] - a[0]), 0), 1))
    return [av + (bv - av) * x for av, bv in zip(a[1:-1], b[1:-1])]

# ---- the blink: every move's, read off the clock as mascot.cpp's blink_at() -------------------------------------------


def _mix(x):
    """lowbias32, as mascot.cpp mix()."""
    x &= 0xFFFFFFFF
    x ^= x >> 16
    x = (x * 0x7FEB352D) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x846CA68B) & 0xFFFFFFFF
    x ^= x >> 16
    return x


def blink_at(now):
    """The eyes shut at `now` ms on the host's clock: the global blink every move has — about every 3 s, with no cap
    on a gap (one in a hundred runs past 11 s), one in four a double, 110 ms shut. A draft passes `front("blink")` when
    this is true and the eyes would be open, so the preview blinks as the board will (pick `now` as the draft's t plus
    any offset)."""
    now = int(now)
    for back in (0, 1):
        slot = now // 1000 - back
        h = _mix((slot ^ 0x9E3779B9) & 0xFFFFFFFF)
        if h % 20 >= 7:
            continue
        into = now - (slot * 1000 + (h >> 8) % 600)
        if into < 0:
            continue
        twice = ((h >> 4) & 3) == 0
        if into < 110 or (twice and 230 <= into < 340):
            return True
    return False

# ---- the character --------------------------------------------------------------------------------------------------


def rect(cells, c, r, w, h, colour):
    for y in range(r, r + h):
        for x in range(c, c + w):
            cells[(x, y)] = colour


def look(cells, dx, dy):
    """The eyes of `cells` moved `dx`, `dy` pixels within the body — a look; body colour where they were."""
    eyes = [(c, r) for (c, r), v in cells.items() if v == EYE and BODY_COL <= c < BODY_COL + 16 and 10 <= r < 22]
    for c, r in eyes:
        cells[(c, r)] = BODY
    for c, r in eyes:
        cells[(c + dx, r + dy)] = EYE
    return cells


def front(eyes="open", left_dy=0, right=(31, 14), body=BODY):
    """The front view without legs (Pose.legs draws them): the body, the free left hand `left_dy` pixels down, the right
    hand's top-left at `right`, the eyes 'open', 'blink' (the lower row), 'happy' (^ ^), 'wink' (the right shut),
    'chevron' (> <) or 'dizzy' (spirals)."""
    cells = {}
    rect(cells, 15, 14 + left_dy, 4, 4, body)
    rect(cells, BODY_COL, BODY_ROW, 16, 12, body)
    rect(cells, right[0], right[1], 4, 4, body)
    for i, ex in enumerate((19, 29)):
        kind = ("blink" if i == 1 else "open") if eyes == "wink" else eyes
        if kind == "open":
            rect(cells, ex, 12, 2, 2, EYE)
        elif kind == "blink":
            rect(cells, ex, 13, 2, 1, EYE)
        elif kind == "happy":
            rect(cells, ex, 12, 2, 1, EYE)
            rect(cells, ex - 1, 13, 1, 1, EYE)
            rect(cells, ex + 2, 13, 1, 1, EYE)
        elif kind == "chevron":  # > <: squeezed shut, each pointing in toward the other, 4 wide and 3 tall
            tip, back = (ex + 1, ex - 1) if i == 0 else (ex - 1, ex + 1)
            rect(cells, back, 12, 2, 1, EYE)
            rect(cells, tip, 13, 2, 1, EYE)
            rect(cells, back, 14, 2, 1, EYE)
        elif kind == "dizzy":  # @ @: a square spiral 4 x 4 round each eye, both turning the same way
            for r, row in enumerate(DIZZY):
                for c, ch in enumerate(row):
                    if ch == "@":
                        cells[(ex - 1 + c, 11 + r)] = EYE
    return cells


# the dizzy eye: in along the top, down the right, back along the bottom, up and into the middle
DIZZY = ("@@@@", "...@", ".@.@", ".@@@")


def work_pose(n, skip=""):
    """Pose n (1-based) of the work loop, from mascot_frames.h, as cells with its legs and its laptop: the turned
    three-quarter views (4–16) show the shaded side `s` and the lit hand `l` in front of the body."""
    src = frames_text()
    blocks = re.findall(r"\{\{ // ([^\n]*)\n(.*?)\}\}", src, re.S)
    rows = re.findall(r'"([^"]*)"', blocks[n - 1][1])
    top = int(re.search(r"kPixelTop = (\d+)", src).group(1))
    left = int(re.search(r"kPixelLeft = (\d+)", src).group(1))
    cells = {}
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            if ch in CHARS and ch not in skip:
                cells[(left + c, top + r)] = CHARS[ch]
    return cells


@dataclass
class Prop:
    """Something carried or set down: `cells` relative to its anchor, the anchor at a GIF cell. Rigid — the body's
    squash carries the anchor, not its pixels; `behind` draws it under the body; `square` draws its pixels that size
    (a pattern bigger than the pixel goes in `paint`: a function (x_px, y_px) -> colour for cells marked 'paint')."""
    cells: dict
    anchor: tuple
    behind: bool = True
    over_anchor: tuple = None  # one cell of it drawn over everything (a pole over the hand holding it)
    paint: object = None
    world: bool = False  # in the glass's frame, not carried by the body — confetti in the air — drawn over all


@dataclass
class Pose:
    """The mascot at an instant. `cells` the body's pixels; `legs` 'planted' (drawn upright from the tilted hips to the
    ground, the raised ones shorter and bent) or None when `cells` hold their own; `raised` 'L', 'R', 'B' (both, tucked
    in the air) or None; physics:
    `lift` px up, `wide`/`tall` per mille about the feet, `lean` 0.1° clockwise about `pivot` ('hips' or 'right_foot'),
    `x` px of sway. Planted legs go with the 'hips' pivot; a pose tilted about the right foot carries its legs in
    `cells`, legs=None, and they tilt with it as the work's do."""
    cells: dict
    props: list = field(default_factory=list)
    legs: str = "planted"
    raised: str = None
    knee: float = .5
    lift: float = 0
    wide: float = 1000
    tall: float = 1000
    lean: float = 0
    x: float = 0
    pivot: str = "hips"
    crouch: float = 0  # px the body drops into a squat, the feet planted, the legs shorter
    feet_out: bool = False  # raised legs bend by the outer feet swinging out a whole pixel, not the knee's half
    swing: float = 0  # px the feet swing out at the knee — the outer legs that far, the inner half — the jump's curve
    knee_dir: int = 0  # the raised knees all bend this way, -1 left, 1 right — walking, toward where it goes; 0 out
    sway: float = 0   # px the body leans over its planted feet, the legs slanting to follow (the idle's look); a
                      # negative crouch with it raises the body as the slanting legs stretch

# ---- the stage ------------------------------------------------------------------------------------------------------


def _body_top_left(img):
    """The resting body's top-left on a deck page: the extent of the body's colour across the glass — so the draft
    stands where the firmware put the mascot, wherever the layout moves it."""
    def near(px):  # the shot holds the panel's RGB565, so the body's colour is rounded: 8 steps of red and blue, 4 of green
        return abs(px[0] - BODY[0]) < 8 and abs(px[1] - BODY[1]) < 4 and abs(px[2] - BODY[2]) < 8
    # Within the mascot's column where the host names one: the page beside it can share the body's colour.
    edge = HOME_RIGHT if HOME_COLUMN else None
    pts = [(x, y) for y, row in enumerate(img) for x, px in enumerate(row[:edge]) if near(px)]
    if not pts:
        return None
    # The hands stand out past the body either side alike, so the colour's middle is the body's centre line.
    left, right = min(x for x, _ in pts), max(x for x, _ in pts)
    return (left + right + 1) // 2 - (PIVOT_COL - BODY_COL) * PX, min(y for _, y in pts)


class Stage:
    """A deck page to draw on: its picture, where the resting body's top-left is on it, and the box the resting
    mascot fills (cleared first). The default backdrop is the host's `backdrop` shot; where the simulator does not
    save it, a draft stands on a black glass with the mascot at its home; pass `backdrop=` a shot of the host's page
    under the mascot, the mascot found on it by its body's colour."""

    def __init__(self, backdrop=DEFAULT_BACKDROP, body_xy=None, clear=None):
        self.bg = read_png(backdrop) if Path(backdrop).exists() else _blank_glass()
        if body_xy is None:
            home = ((HOME_LEFT + HOME_RIGHT) // 2 - (PIVOT_COL - BODY_COL) * PX,
                    PANEL_H // 2 - 8 * PX)   # the body's centre line and its 16 rows centred
            body_xy = _body_top_left(self.bg) or home
        if clear is None:  # the resting mascot: its hands either side of the body, down to the ground
            clear = (body_xy[0] - 2 * PX, body_xy[1] - PX, body_xy[0] + 18 * PX, body_xy[1] + 16 * PX + 1)
        for y in range(max(clear[1], 0), min(clear[3], PANEL_H)):
            for x in range(max(clear[0], 0), min(clear[2], PANEL_W)):
                self.bg[y][x] = BACKGROUND
        self.x0 = body_xy[0] - BODY_COL * PX
        self.y0 = body_xy[1] - BODY_ROW * PX

    def render(self, p: Pose, margin=0):
        """The pose over the backdrop; with `margin`, on a canvas that many pixels wider on every side, black past the
        glass, so reach() sees what a pose draws beyond the edges — the glass itself at (margin, margin)."""
        if margin:
            pad = [BACKGROUND] * margin
            img = [[BACKGROUND] * (PANEL_W + 2 * margin) for _ in range(margin)]
            img += [pad + row[:] + pad for row in self.bg]
            img += [[BACKGROUND] * (PANEL_W + 2 * margin) for _ in range(margin)]
            return self._draw(p, img, margin)
        return self._draw(p, [row[:] for row in self.bg], 0)

    def _draw(self, p, img, margin):
        wide, tall = p.wide / 1000, p.tall / 1000
        ang = math.radians(p.lean / 10)
        ca, sa = math.cos(ang), math.sin(ang)
        k = KEY_SCALE
        gx = self.x0 + PIVOT_COL * PX + p.x * k     # the feet's centre on the ground
        bx = gx + p.sway * k                         # the body's centre line: swayed over its planted feet
        feet = self.y0 + GROUND_ROW * PX - p.lift * k
        gy = feet + p.crouch * k                     # where the body is drawn from: dropped by a squat, raised by a rise
        hx, hy = bx, gy - (GROUND_ROW - HIP_ROW) * PX * tall
        if p.pivot == "right_foot":
            hx, hy = self.x0 + 33 * PX + p.x * k, gy
        behind = [q for q in p.props if q.behind and not q.world]
        front_ = [q for q in p.props if not q.behind and not q.world]
        world = [q for q in p.props if q.world]
        for y in range(-margin, PANEL_H + margin):   # the whole glass, and `margin` past it on every side
            for xs in range(-margin, PANEL_W + margin):
                dx, dy = xs + .5 - hx, y + .5 - hy
                ux, uy = dx * ca + dy * sa, -dx * sa + dy * ca
                uy += hy - gy
                ux += hx - bx
                col = None
                for q in behind:
                    col = _prop_at(q, ux, uy, wide, tall) or col
                cell = (math.floor(PIVOT_COL + ux / wide / PX), math.floor(GROUND_ROW + uy / tall / PX))
                if cell in p.cells:
                    col = p.cells[cell]
                for q in front_:
                    col = _prop_at(q, ux, uy, wide, tall) or col
                for q in p.props:
                    if q.over_anchor and _prop_cell(q, ux, uy, wide, tall)[0] == q.over_anchor:
                        col = q.cells[q.over_anchor]
                if col is None and p.legs == "planted":
                    col = self._leg(p, xs, y, gx, bx, feet, hx, hy, ang, wide)
                for q in world:
                    cell = (math.floor((xs - self.x0) / PX) - q.anchor[0], math.floor((y - self.y0) / PX) - q.anchor[1])
                    col = q.cells.get(cell, col)
                if col is not None:
                    img[y + margin][xs + margin] = col
        return img

    def _leg(self, p, xs, y, gx, bx, gy, hx, hy, ang, wide):
        top = hy + (xs + .5 - hx) * math.tan(ang)
        if not (top < gy and top - 2 <= y < gy + 1):
            return None
        row = (y + .5 - top) / (gy - top) * 4
        cx = bx + (gx - bx) * min(max(row / 4, 0), 1)  # a swayed body's legs slant from its hips to the planted feet
        lx = PIVOT_COL + (xs + .5 - cx) / wide / PX
        for i, c in enumerate(LEG_COLS):
            up = (p.raised in ("L", "B") and i < 2) or (p.raised in ("R", "B") and i >= 2)
            if not 0 <= row < (3 if up else 4):
                continue
            out = -1 if i < 2 else 1
            if up and p.feet_out:  # the outer feet swung out a whole pixel on the last row: a clean bend
                shift = out if i in (0, len(LEG_COLS) - 1) and row >= 2 else 0
            else:                  # the knee bowed out on the middle row (the finish's slight bend), or all one way
                shift = (p.knee_dir or out) * p.knee if up and 1 <= row < 2 else 0
            if p.swing and row >= 2:  # the lower half, the foot, swung out: the legs curve outward (the rig's jump)
                outer = i in (0, len(LEG_COLS) - 1)
                shift += out * (p.swing if outer else p.swing / 2) * KEY_SCALE / PX
            if c + shift <= lx < c + shift + 2:
                return BODY
        return None


def _prop_cell(q, ux, uy, wide, tall):
    ax, ay = q.anchor
    x0 = (ax + .5 - PIVOT_COL) * PX * wide - PX / 2
    y0 = (ay + 1 - GROUND_ROW) * PX * tall - PX
    return (math.floor((ux - x0) / PX), math.floor((uy - y0) / PX)), (ux - x0, uy - y0)


def _prop_at(q, ux, uy, wide, tall):
    cell, (lx, ly) = _prop_cell(q, ux, uy, wide, tall)
    col = q.cells.get(cell)
    if col == "paint":
        return q.paint(lx, ly)
    return col



def _blank_glass():
    return [[BACKGROUND] * PANEL_W for _ in range(PANEL_H)]


# The whole glass; and where a move that stays home is seen — all of it when the mascot stands in its middle, or the
# glass's half holding the mascot's column and the start of the page beside it.
GLASS_BOX = (0, 0, PANEL_W, PANEL_H)
HOME_BOX = (0, 0, PANEL_W // 2, PANEL_H) if HOME_COLUMN else GLASS_BOX


def in_corner(x, y):
    """Whether glass pixel (x, y) lies outside the rounded square — written to the panel, never seen (the host's glass;
    config.json → mascot → corner_px)."""
    r = GLASS_CORNER
    cx = r if x < r else (PANEL_W - 1 - r if x > PANEL_W - 1 - r else None)
    cy = r if y < r else (PANEL_H - 1 - r if y > PANEL_H - 1 - r else None)
    if cx is None or cy is None:
        return False
    return (x - cx) ** 2 + (y - cy) ** 2 > r * r

# ---- the glass's one bit --------------------------------------------------------------------------------------------

# The host's dithers: a grey's hex → its 4 x 4 pattern in the screen's own coordinates, `#` black and `.` white, locked
# to the glass's pixels — row y & 3, column x & 3 (config: `dither`). Any other colour is black under mid-grey.
_DITHER = {_hex(int(k.lstrip("#"), 16)): v for k, v in (_CFG.get("dither") or {}).items()}


def one_bit(img, x0=0, y0=0):
    """`img` as the e-paper draws it on a partial refresh: one bit a pixel, each grey the host names as its dither,
    locked to the glass's pixels — `x0`, `y0` the glass's coordinates of img[0][0]. A colour the host has no dither for
    is black under mid-grey (Rec. 601 luma under 128), white from it."""
    out = []
    for y, row in enumerate(img):
        gy = y0 + y
        line = []
        for x, px in enumerate(row):
            pat = _DITHER.get(px)
            if pat is not None:
                black = pat[gy & 3][(x0 + x) & 3] == "#"
            else:
                black = (299 * px[0] + 587 * px[1] + 114 * px[2]) < 128 * 1000
            line.append(BLACK if black else (255, 255, 255))
        out.append(line)
    return out


def _as_glass(img, box):
    """A crop as the host's glass shows it: one bit on a flipbook's partial refresh, the colours as they are on an LCD."""
    return one_bit(img, box[0], box[1]) if GLASS == "flipbook" else img

# ---- the outputs ----------------------------------------------------------------------------------------------------


def zoom(img, k):
    return [[px for px in row for _ in range(k)] for row in img for _ in range(k)]


def crop(img, box):
    x0, y0, x1, y1 = box
    return [row[x0:x1] for row in img[y0:y1]]


def sheet(stage, pose, times, path, k=2, cols=4, box=HOME_BOX):
    """The stills at `times` (ms), side by side: the contact sheet the developer reads first."""
    w, h = (box[2] - box[0]) * k, (box[3] - box[1]) * k
    rows = (len(times) + cols - 1) // cols
    canvas = [[(40, 40, 40)] * (cols * (w + 4)) for _ in range(rows * (h + 4))]
    for i, t in enumerate(times):
        img = zoom(_as_glass(crop(stage.render(pose(t)), box), box), k)
        ox, oy = (i % cols) * (w + 4), (i // cols) * (h + 4)
        for y in range(h):
            canvas[oy + y][ox:ox + w] = img[y]
    write_png(path, canvas)


def gif(stage, pose, length_ms, path, k=2, box=HOME_BOX, speed=1.0, clock=None):
    """The move as the glass would show it: on an LCD a frame every 33.3 ms (the board's refresh), delays 3, 3, 4 cs;
    on a flipbook a frame every `frame_ms`, one bit a pixel, each held its refresh. `speed` 0.5 writes a half-speed
    copy to follow each move; `clock` maps real ms to the move's own (a pace that changes)."""
    frames, t = [], 0.0
    while t <= length_ms:
        tt = clock(t) if clock else t
        frames.append(zoom(_as_glass(crop(stage.render(pose(tt)), box), box), k))
        t += FRAME_MS
    if GLASS == "flipbook":
        held = max(2, round(FRAME_MS / 10 / speed))
        write_gif(path, frames, [held] * len(frames))
        return check_gif(path)
    pattern = (3, 3, 4) if speed == 1.0 else tuple(round(d / speed) for d in (3, 3, 4))
    write_gif(path, frames, [pattern[i % 3] for i in range(len(frames))])
    return check_gif(path)


def reach(stage, pose, length_ms, step_ms=None, margin=None):
    """How far the move's drawn pixels get, over the move: the highest and lowest rows and the leftmost and rightmost
    columns any frame draws, each with its time — in glass coordinates, so a row under 0 or a column past PANEL_W - 1
    is drawn beyond the glass and cut off there. It draws on a canvas `margin` px wider on every side (half the glass
    by default), so a draw past an edge is seen. `corner` is the first time a drawn pixel falls inside a corner arc
    (None when none does). `on_glass` is nothing cut off above or below and nothing in a corner: hold `top` >= 0,
    `bottom` < PANEL_H and `corner` None. Only a travelling move (the walk, the skip) may leave by a side, and then
    wholly, coming back from the other."""
    m = PANEL_H // 2 if margin is None else margin
    step_ms = step_ms or (FRAME_MS if GLASS == "flipbook" else 33)
    out = {"top": (PANEL_H + m, None), "bottom": (-m - 1, None), "left": (PANEL_W + m, None), "right": (-m - 1, None),
           "corner": None}
    bg = [[BACKGROUND] * (PANEL_W + 2 * m) for _ in range(m)] + [[BACKGROUND] * m + r + [BACKGROUND] * m for r in stage.bg] + \
        [[BACKGROUND] * (PANEL_W + 2 * m) for _ in range(m)]
    t = 0
    while t <= length_ms:
        img = stage.render(pose(t), margin=m)
        for y, (row, back) in enumerate(zip(img, bg)):
            for x, (px, b) in enumerate(zip(row, back)):
                if px != b:
                    gy, gx = y - m, x - m
                    if out["corner"] is None and 0 <= gx < PANEL_W and 0 <= gy < PANEL_H and in_corner(gx, gy):
                        out["corner"] = (gx, gy, t)
                    if gy < out["top"][0]:
                        out["top"] = (gy, t)
                    if gy > out["bottom"][0]:
                        out["bottom"] = (gy, t)
                    if gx < out["left"][0]:
                        out["left"] = (gx, t)
                    if gx > out["right"][0]:
                        out["right"] = (gx, t)
        t += step_ms
    out["on_glass"] = out["top"][0] >= 0 and out["bottom"][0] < PANEL_H and out["corner"] is None
    return out


def flipbook(stage, pose, length_ms):
    """A flipbook move's cost on e-paper (MASCOT §6): `frames`, the refreshes it takes, one every `frame_ms` — each a
    partial that counts toward the host's limit before a full refresh; `window`, the box (x0, y0, x1, y1) of every pixel
    any frame changes from the one before, in glass coordinates — the partial window the host refreshes, its size what
    each frame costs; `changed`, the pixels each frame flips, one bit a pixel. The first frame is measured against the
    resting page: pose(0) is the rest pose the page already shows, so it costs no refresh."""
    def frame(t):
        return one_bit(stage.render(pose(t)))
    prev, t, changed = frame(0), FRAME_MS, []
    x0, y0, x1, y1 = PANEL_W, PANEL_H, -1, -1
    while t <= length_ms:
        cur = frame(t)
        flips = 0
        for y, (a, b) in enumerate(zip(prev, cur)):
            for x, (pa, pb) in enumerate(zip(a, b)):
                if pa != pb:
                    flips += 1
                    x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x + 1), max(y1, y + 1)
        changed.append(flips)
        prev, t = cur, t + FRAME_MS
    return {"frames": len(changed), "frame_ms": FRAME_MS, "window": (x0, y0, x1, y1) if x1 >= 0 else None, "changed": changed}


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "check":
        print(f"{check_gif(sys.argv[2])} frames decoded OK")
    else:
        print(__doc__)
        sys.exit(2)
