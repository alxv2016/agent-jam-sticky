#!/usr/bin/env python3
"""The finish as the developer approved it, drafted with mascot_preview — a worked example to copy for a new move. Its
keys and beats are the host widget's kFinishOpen, kFinishWave, kFinishClose and finish_pose_at() (mascot.cpp).

  MASCOT_CELL=24 python3 .agents/skills/mascot-animation/scripts/examples/finish.py <out dir>

writes finish_sheet.png (the stills), finish.gif (at its pace) and finish_halfspeed.gif, and prints its reach on the
host's glass. It reads the flag from the mascot's frames (the host's frames_h, or mascot_preview's MASCOT_FRAMES_H
in a tree without them).
"""
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mascot_preview as mp  # noqa: E402

# ---- the flag: the sheet's outlines from mascot_frames.h, its checks bigger than the pixel ----------------------------
_src = mp.frames_text()
_block = _src[_src.index("kFlagFrames[] = {"):]
FLAGS = [(re.findall(r'"([^"]*)"', body), int(pole))
         for body, pole in re.findall(r"\{\{ // [^\n]*\n(.*?)\}, (\d+)\}", _block, re.S)]
SEQUENCE = [int(v) for v in re.search(r"kFlagSequence\[\] = \{([^}]*)\}", _src).group(1).split(",")]
POLE_ABOVE = 8
CHECK_PX = mp.PX * 3 // 2  # 3/2 of the pixel (mascot.cpp flag_check())


def flag(i, grip, raised=True):
    rows, pole = FLAGS[i]
    cells = {}
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            if ch != "x":
                continue
            if c == pole:
                if r < POLE_ABOVE:
                    cells[(0, r - POLE_ABOVE)] = mp.POLE
            else:
                cells[(c - pole, r - POLE_ABOVE)] = "paint"
    cells[(0, 0)] = mp.POLE

    def paint(x, y):
        return mp.CHECK if (math.floor(x / CHECK_PX) + math.floor(y / CHECK_PX)) % 2 else mp.FLAG
    # the pole's foot over the raised hand; rising or yanked down behind him, behind the hand too
    return mp.Prop(cells, grip, behind=True, over_anchor=(0, 0) if raised else None, paint=paint)

# ---- the track ------------------------------------------------------------------------------------------------------


STEP = 67
WAVE_FROM = 531
CYCLE = 9 * STEP
WAVE_END = WAVE_FROM + 5 * CYCLE
TOTAL = WAVE_END + 520

OPEN = [(0, 0, 1000, 1000, 0, 0, "lin"), (60, 0, 1000, 1000, 0, 0, "lin"),
        (150, 0, 1090, 880, -25, 0, "out"),     # the wind-up: squashed, leaning away from the raise
        (210, 0, 1100, 870, -30, 0, "inout"),   # tension
        (275, 5, 920, 1130, 0, 0, "out"),       # the pop: sprung up stretched
        (330, 0, 970, 1060, 0, 0, "in"),        # falling, still stretched
        (365, 0, 1140, 850, 0, 0, "out"),       # the impact: squashed
        (410, 0, 950, 1070, 0, 0, "inout"),     # the elastic settle: tall,
        (455, 0, 1040, 965, 0, 0, "inout"),     # squat,
        (500, 0, 985, 1015, 0, 0, "inout"),     # tall,
        (WAVE_FROM, 0, 1000, 1000, 40, 2, "inout")]
WAVE = [(0, 0, 1000, 1000, 40, 2, "lin"),
        (80, 0, 1000, 1000, -10, 0, "in"),      # swung across, faster and faster, onto the raised legs…
        (115, 0, 1130, 860, -55, -3, "out"),    # …the stomp: squished, the lean overshooting
        (175, 0, 965, 1045, -30, -2, "out"),    # rocking back, springing tall
        (235, 0, 1020, 985, -42, -2, "inout"),  # and over again
        (300, 0, 1000, 1000, -40, -2, "inout"),
        (380, 0, 1000, 1000, 10, 0, "in"),      # swung back…
        (415, 0, 1130, 860, 55, 3, "out"),      # …the other stomp
        (475, 0, 965, 1045, 30, 2, "out"),
        (535, 0, 1020, 985, 42, 2, "inout"),
        (CYCLE, 0, 1000, 1000, 40, 2, "inout")]
CLOSE = [(0, 0, 1000, 1000, 40, 2, "lin"),
         (90, 0, 1070, 920, -10, 0, "out"),     # the flag yanked down: a dip
         (170, 6, 950, 1090, 0, 0, "out"),      # a little hop of satisfaction
         (230, 0, 980, 1040, 0, 0, "in"),
         (260, 0, 1130, 860, 0, 0, "out"),      # the landing: squashed
         (305, 0, 955, 1065, 0, 0, "inout"), (350, 0, 1035, 970, 0, 0, "inout"),
         (395, 0, 990, 1010, 0, 0, "inout"), (440, 0, 1000, 1000, 0, 0, "inout"),
         (520, 0, 1000, 1000, 0, 0, "lin")]


def pose(t):
    if t < WAVE_FROM:
        lift, wide, tall, lean, x = mp.track(OPEN, t)
    elif t < WAVE_END:
        lift, wide, tall, lean, x = mp.track(WAVE, (t - WAVE_FROM) % CYCLE)
    else:
        lift, wide, tall, lean, x = mp.track(CLOSE, t - WAVE_END)
    eyes = "happy" if 90 <= t < WAVE_END + 350 else "open"
    right, left_dy, props, raised = (31, 14), 0, [], None
    if 60 <= t < 225:
        right = (31, 15)                                   # cocked
    elif 225 <= t < 245:
        right, props = (31, 9), [flag(8, (33, 13), raised=False)]   # rising from behind him, edge-on
    elif 245 <= t < WAVE_FROM:
        right = (31, 7)
        i = 8 if t < 320 else 9 if t < 365 else min((t - 365) // STEP, 2)
        props, left_dy = [flag(int(i), (33, 7))], (1 if t < 365 else 0)
    elif WAVE_FROM <= t < WAVE_END:
        u = (t - WAVE_FROM) % CYCLE
        f = 3 + int(u // STEP)                             # the sheet's frames 4–12
        across = 4 <= f <= 7
        dx, dy = (-1 if across else 0), (-1 if f in (4, 9) else 0)
        right, props = (31 + dx, 7 + dy), [flag(SEQUENCE[f], (33 + dx, 7 + dy))]
        left_dy = -1 if across else 1
        raised = "R" if 80 <= u < 380 else "L"
    elif t >= WAVE_END:
        u = t - WAVE_END
        if u < 60:
            right, props, raised = (31, 7), [flag(0, (33, 7))], "L"
        elif u < 110:
            right, props = (31, 9), [flag(8, (33, 11), raised=False)]   # yanked down behind him
        elif u < 260:
            right = (31, 15)
    return mp.Pose(mp.front(eyes, left_dy, right), props, raised=raised, lift=lift, wide=wide, tall=tall, lean=lean,
                   x=x)

# ---- the pace: half speed, then up to speed (mascot.cpp finish_track_at()) -------------------------------------------


SLOW = 2 * (WAVE_FROM + CYCLE)
RAMP = 8 * CYCLE // 3
LENGTH = SLOW + RAMP + (TOTAL - SLOW // 2 - 3 * RAMP // 4)


def clock(real):
    if real < SLOW:
        return real / 2
    d = real - SLOW
    if d < RAMP:
        return SLOW / 2 + d / 2 + d * d / (4 * RAMP)
    return min(SLOW / 2 + 3 * RAMP / 4 + d - RAMP, TOTAL)


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    out.mkdir(parents=True, exist_ok=True)
    stage = mp.Stage()
    w = WAVE_FROM + 4 * CYCLE
    mp.sheet(stage, pose, [0, 180, 275, 365, 450, w, w + 115, w + 200, w + 415, w + 500, WAVE_END + 90,
                           WAVE_END + 170, WAVE_END + 260, TOTAL], out / "finish_sheet.png", cols=7)
    print("finish.gif:", mp.gif(stage, pose, LENGTH, out / "finish.gif", clock=clock), "frames")
    print("finish_halfspeed.gif:", mp.gif(stage, pose, LENGTH, out / "finish_halfspeed.gif", speed=.5, clock=clock),
          "frames")
    r = mp.reach(stage, pose, TOTAL)
    print(f"reach: rows {r['top'][0]}-{r['bottom'][0]}, columns {r['left'][0]}-{r['right'][0]} — "
          f"{'on the glass' if r['on_glass'] else 'CUT OFF above, below or in a corner'} ({mp.PANEL_W} x {mp.PANEL_H},"
          f" cell {mp.CELL}{', corner at ' + str(r['corner']) if r['corner'] else ''})")
