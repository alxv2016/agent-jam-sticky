#!/usr/bin/env python3
"""The mascot's model sheet — the character-design reference a new move is drawn from (MASCOT §1, §2):
the front, three-quarter and side views on the pixel grid with their construction lines and measures, the expressions,
the hands and the legs in each of their positions, and the palette by role. Drawn from the theme's colours and the
work loop's frames, so it shows what the firmware draws. On a flipbook glass (e-paper, host_config's `glass`) the
sheet's paper, grid and lines are the LCD sheet's in grey, the views in the panel's true greys as a greyscale full refresh
draws them, a row of the views at one bit a pixel as every partial refresh draws them — the greys as the host's
dithers, at the glass's own pixels — and each role's swatch both ways (MASCOT §6).

  python3 .agents/skills/mascot-animation/scripts/model_sheet.py \
      .agents/skills/mascot-animation/references/mascot_model_sheet.png        # an LCD host, the agent deck's colours
  python3 .agents/skills/mascot-animation/scripts/model_sheet.py \
      .agents/skills/mascot-animation/references/mascot_model_sheet_eink.png   # a flipbook host, as this sheet is

The scale line reads the pixel from the host's cell (half a cell: 8 px at the deck's 16 px cell, 12 px at the Sticky's 24).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mascot_preview as mp  # noqa: E402

S = 10                       # a mascot pixel on the sheet
EPAPER = mp.GLASS == "flipbook"
if EPAPER:                   # the deck's sheet in grey: each colour's luma, the construction lines' blue included
    PAPER, GRID, GRID_5, LINE, INK, DIM = (24,) * 3, (40,) * 3, (52,) * 3, (140,) * 3, (210,) * 3, (130,) * 3
else:
    PAPER = (24, 24, 24)
    GRID = (40, 40, 40)
    GRID_5 = (52, 52, 52)
    LINE = (90, 150, 220)    # construction lines
    INK = (210, 210, 210)
    DIM = (130, 130, 130)
SHADE, TINT, BODY, EYE = mp.SHADE, mp.TINT, mp.BODY, mp.EYE

# ---- a 3 x 5 pixel font --------------------------------------------------------------------------------------------
FONT = {
    "A": "010101111101101", "B": "110101110101110", "C": "011100100100011", "D": "110101101101110",
    "E": "111100110100111", "F": "111100110100100", "G": "011100101101011", "H": "101101111101101",
    "I": "111010010010111", "J": "001001001101010", "K": "101101110101101", "L": "100100100100111",
    "M": "101111111101101", "N": "110101101101101", "O": "010101101101010", "P": "110101110100100",
    "Q": "010101101110011", "R": "110101110101101", "S": "011100010001110", "T": "111010010010010",
    "U": "101101101101111", "V": "101101101101010", "W": "101101111111101", "X": "101101010101101",
    "Y": "101101010010010", "Z": "111001010100111", "0": "111101101101111", "1": "010110010010111",
    "2": "110001010100111", "3": "110001010001110", "4": "101101111001001", "5": "111100110001110",
    "6": "011100111101111", "7": "111001010010010", "8": "111101111101111", "9": "111101111001110",
    " ": "000000000000000", "-": "000000111000000", "+": "000010111010000", ".": "000000000000010",
    ",": "000000000010100", ":": "000010000010000", "/": "001001010100100", "(": "010100100100010",
    ")": "010001001001010", "=": "000111000111000", "^": "010101000000000", "#": "101111101111101",
    "°": "010101010000000", "×": "000101010101000", "·": "000000010000000", "'": "010010000000000",
    "%": "101001010100101",
}


def text(img, x, y, s, colour=INK, k=2):
    for ch in s.upper():
        g = FONT.get(ch, FONT[" "])
        for i, bit in enumerate(g):
            if bit == "1":
                for dy in range(k):
                    for dx in range(k):
                        px, py = x + (i % 3) * k + dx, y + (i // 3) * k + dy
                        if 0 <= py < len(img) and 0 <= px < len(img[0]):
                            img[py][px] = colour
        x += 4 * k
    return x


def fill(img, x0, y0, x1, y1, colour):
    for y in range(max(y0, 0), min(y1, len(img))):
        row = img[y]
        for x in range(max(x0, 0), min(x1, len(row))):
            row[x] = colour

# ---- the views ------------------------------------------------------------------------------------------------------


def legs(cells, cols, rows=(22, 26), colour=BODY):
    for c in cols:
        mp.rect(cells, c, rows[0], 2, rows[1] - rows[0], colour)


def front(eyes="open"):
    cells = mp.front(eyes)
    legs(cells, mp.LEG_COLS)
    return cells


def three_quarter():
    """Work pose 5 without its laptop, moved home: the front 12 wide, the side 4 in shade on the glass's right, the near
    hand on the side in the tint with its shadow, the far hand behind the body."""
    cells = {(c - 5, r): v for (c, r), v in mp.work_pose(5, skip="g").items()}  # by the frame's letter, not its
    # colour: a host may give the laptop the same grey as a part of the mascot
    mp.rect(cells, 15, 14, 2, 4, BODY)  # the far hand, peeking past the front's edge as at rest
    # the legs stand front, back, front, back, evenly 2 pixels apart, a pixel in from the front face's edge: the second
    # and the last behind, in shade, the last a pixel inside the side face — the developer's drawing, 2026-10-29
    for c in range(17, 33):
        for r in range(22, 26):
            cells.pop((c, r), None)
    for c, colour in ((18, BODY), (22, SHADE), (26, BODY), (30, SHADE)):
        mp.rect(cells, c, 22, 2, 4, colour)
    return cells


def side():
    """Facing the glass's left as the three-quarter does, turned on to 90° — the side face 10 deep, lit as it turns to
    the viewer; one eye a pixel in from the leading edge; the near hand on the side in the tint with its shadow; two
    legs, the front pair and the back pair, the far legs behind them — the developer's yes, 2026-10-29."""
    cells = {}
    mp.rect(cells, 20, 10, 10, 12, BODY)
    mp.rect(cells, 21, 12, 2, 2, EYE)
    mp.rect(cells, 24, 14, 4, 4, TINT)
    mp.rect(cells, 28, 15, 1, 3, SHADE)
    legs(cells, (21, 27))
    return cells


def back_three_quarter():
    """Turned on past the side, about 135° (the spin's turn): the back 12 wide, no eyes, the side 4 on the glass's left
    in shade, the near hand on the back, wholly in the tint — the developer's ask, 2026-11-24, over its half in the
    body's colour (2026-10-29)."""
    cells = {}
    mp.rect(cells, 21, 10, 12, 12, BODY)
    mp.rect(cells, 17, 10, 4, 12, SHADE)
    mp.rect(cells, 31, 14, 4, 4, TINT)
    for c, colour in ((19, SHADE), (23, BODY), (27, SHADE), (31, BODY)):
        mp.rect(cells, c, 22, 2, 4, colour)
    return cells


def back():
    """From behind (the spin's turn): the body 16 wide, no eyes; the hands half in front of it now, each wholly in the
    tint — the developer's ask, 2026-11-24, over their outer halves in the body's colour (2026-10-29)."""
    cells = {}
    mp.rect(cells, 17, 10, 16, 12, BODY)
    mp.rect(cells, 15, 14, 4, 4, TINT)
    mp.rect(cells, 31, 14, 4, 4, TINT)
    legs(cells, mp.LEG_COLS)
    return cells

# ---- the sheet ------------------------------------------------------------------------------------------------------


def draw_cells(img, ox, oy, cells, box, k=S):
    c0, r0, c1, r1 = box
    for (c, r), colour in cells.items():
        if c0 <= c < c1 and r0 <= r < r1:
            fill(img, ox + (c - c0) * k, oy + (r - r0) * k, ox + (c - c0 + 1) * k, oy + (r - r0 + 1) * k, colour)


def grid(img, ox, oy, box, k=S):
    c0, r0, c1, r1 = box
    for c in range(c0, c1 + 1):
        fill(img, ox + (c - c0) * k, oy, ox + (c - c0) * k + 1, oy + (r1 - r0) * k, GRID_5 if c % 5 == 0 else GRID)
    for r in range(r0, r1 + 1):
        fill(img, ox, oy + (r - r0) * k, ox + (c1 - c0) * k, oy + (r - r0) * k + 1, GRID_5 if r % 5 == 0 else GRID)


def hline(img, ox, oy, box, row, label, k=S):
    c0, r0, c1, _ = box
    y = oy + (row - r0) * k
    for x in range(ox, ox + (c1 - c0) * k, 6):
        fill(img, x, y, x + 3, y + 1, LINE)
    text(img, ox + (c1 - c0) * k + 4, y - 4, label, LINE)


def vline(img, ox, oy, box, col, k=S):
    c0, r0, _, r1 = box
    x = ox + (col - c0) * k
    for y in range(oy, oy + (r1 - r0) * k, 6):
        fill(img, x, y, x + 1, y + 3, LINE)


def dim(img, x0, x1, y, label):
    fill(img, x0, y, x1, y + 1, INK)
    fill(img, x0, y - 3, x0 + 1, y + 4, INK)
    fill(img, x1 - 1, y - 3, x1, y + 4, INK)
    w = len(label) * 8
    text(img, (x0 + x1 - w) // 2, y + 6, label, INK)


def view(img, ox, oy, title, cells, box, measures, centre=None, note=None):
    text(img, ox, oy - 22, title, INK, 2)
    grid(img, ox, oy, box)
    draw_cells(img, ox, oy, cells, box)
    for row, label in ((10, "TOP 10"), (12, "EYES 12"), (14, "HANDS 14"), (22, "HIPS 22"), (26, "GROUND 26")):
        hline(img, ox, oy, box, row, label)
    if centre is not None:
        vline(img, ox, oy, box, centre)
    c0, r0, c1, r1 = box
    for c in range(c0, c1, 5):
        text(img, ox + (c - c0) * S + 1, oy + (r1 - r0) * S + 4, str(c), DIM, 1)
    for r in range(r0, r1, 2):
        text(img, ox - 14, oy + (r - r0) * S + 2, str(r), DIM, 1)
    y = oy + (r1 - r0) * S + 16
    for x0, x1, label in measures:
        dim(img, ox + (x0 - c0) * S, ox + (x1 - c0) * S, y, label)
        y += 22
    if note:
        for i, line in enumerate(note):
            text(img, ox, y + 4 + i * 14, line, DIM, 2)


def strip(img, ox, oy, title, items, box, k=6):
    text(img, ox, oy - 18, title, INK, 2)
    c0, r0, c1, r1 = box
    w = (c1 - c0) * k
    for i, (label, cells) in enumerate(items):
        x = ox + i * (w + 14)
        fill(img, x, oy, x + w, oy + (r1 - r0) * k, mp.BACKGROUND if EPAPER else (32, 32, 32))
        if EPAPER:  # the panel's edge, so the glass reads as one panel beside the others
            frame(img, x, oy, x + w, oy + (r1 - r0) * k, GRID_5)
        draw_cells(img, x, oy, cells, box, k)
        text(img, x, oy + (r1 - r0) * k + 4, label, DIM, 1)


def frame(img, x0, y0, x1, y1, colour):
    fill(img, x0, y0, x1, y0 + 1, colour)
    fill(img, x0, y1 - 1, x1, y1, colour)
    fill(img, x0, y0, x0 + 1, y1, colour)
    fill(img, x1 - 1, y0, x1, y1, colour)


def one_bit_strip(img, ox, oy, items, box):
    """The views as every partial refresh draws them: each mascot pixel `mp.PX` glass pixels, one bit a pixel, the
    greys as the host's dithers locked to where the mascot stands on the glass — at 1:1, a sheet pixel a glass pixel."""
    text(img, ox, oy - 18, "ON A PARTIAL REFRESH  ·  ONE BIT, THE GREYS AS DITHERS  ·  1:1 GLASS PIXELS", INK, 2)
    c0, r0, c1, r1 = box
    stage = mp.Stage(backdrop="/nonexistent")
    gx0, gy0 = round(stage.x0 + c0 * mp.PX), round(stage.y0 + r0 * mp.PX)
    w, h = (c1 - c0) * mp.PX, (r1 - r0) * mp.PX
    per_row = max(1, (len(img[0]) - ox + 14) // (w + 14))  # at 1:1, as many as the sheet's width holds, then the next row
    for i, (label, cells) in enumerate(items):
        glass = [[mp.BACKGROUND] * w for _ in range(h)]
        for (c, r), colour in cells.items():
            if c0 <= c < c1 and r0 <= r < r1:
                for y in range((r - r0) * mp.PX, (r - r0 + 1) * mp.PX):
                    glass[y][(c - c0) * mp.PX:(c - c0 + 1) * mp.PX] = [colour] * mp.PX
        shot = mp.one_bit(glass, gx0, gy0)
        x = ox + (i % per_row) * (w + 14)
        y0 = oy + (i // per_row) * (h + 30)
        for y, row in enumerate(shot):
            img[y0 + y][x:x + w] = row
        frame(img, x - 1, y0 - 1, x + w + 1, y0 + h + 1, GRID_5)
        text(img, x, y0 + h + 6, label, DIM, 1)
    return oy + ((len(items) - 1) // per_row) * (h + 30) + h


ONE_BIT_BOX = (12, 7, 39, 27)


def one_bit_height(W, n=5, ox=40):
    """How tall the one-bit row is at this glass's pixel, wrapped to the sheet's width."""
    w, h = (ONE_BIT_BOX[2] - ONE_BIT_BOX[0]) * mp.PX, (ONE_BIT_BOX[3] - ONE_BIT_BOX[1]) * mp.PX
    per_row = max(1, (W - ox + 14) // (w + 14))
    return ((n - 1) // per_row) * (h + 30) + h


def main(out):
    W = 1220 if EPAPER else 1180
    # The palette below the legs, or below the one-bit row on e-paper, which grows with the glass's pixel.
    y4 = 964 + (one_bit_height(W) + 70 if EPAPER else 0)
    H = y4 + (430 if EPAPER else 330)
    img = [[PAPER] * W for _ in range(H)]
    text(img, 24, 18, "CLAUDE'S MASCOT  ·  MODEL SHEET", INK, 3)
    text(img, 24, 44, f"1 PIXEL = {mp.PX} PX ON THE GLASS = HALF A MASCOT CELL.  GIF GRID COORDINATES.  LIGHT FROM THE VIEWER.",
         DIM, 2)

    box = (12, 7, 39, 27)
    top = 110
    view(img, 40, top, "FRONT", front(), box, [(17, 33, "BODY 16"), (15, 35, "HANDS OUT 2 EACH SIDE")], centre=25,
         note=["THE REST POSE. EYES 2X2,", "HANDS 4X4 HALF BEHIND, 4 LEGS", "2X4: FRONT, BACK, FRONT, BACK."])
    view(img, 420, top, "THREE-QUARTER  ·  25° TO ITS LEFT", three_quarter(), box,
         [(17, 29, "FRONT 12"), (29, 33, "SIDE 4")], centre=23,
         note=["SIDE IN SHADE. NEAR HAND ON", "THE SIDE IN TINT, ITS SHADOW", "BEHIND IT. FAR HAND BEHIND.",
               "LEGS EVENLY 2 APART FROM COL 18,", "THE BACK ONES (2ND, 4TH) IN SHADE."])
    view(img, 800, top, "SIDE  ·  90° TO ITS LEFT", side(), box, [(20, 30, "DEPTH 10")], centre=25,
         note=["THE SIDE LIT AS IT TURNS TO", "THE VIEWER, ONE EYE, NEAR HAND", "IN TINT, 2 LEGS: THE FRONT",
               "PAIR, THE BACK PAIR."])

    eb = (17, 10, 33, 17)
    y2 = 470
    strip(img, 40, y2, "EYES", [("OPEN", mp.front("open")), ("BLINK", mp.front("blink")),
                                ("WINK", mp.front("wink")), ("HAPPY ^ ^", mp.front("happy")),
                                ("CHEVRON > <", mp.front("chevron")), ("DIZZY", mp.front("dizzy"))], eb, k=7)
    hb = (12, 4, 38, 22)
    hands = [("REST", mp.front()), ("RAISED", mp.front(right=(31, 6))), ("BOTH UP", _both((15, 6), (31, 6))),
             ("COCKED", mp.front(right=(31, 15))), ("OUT WIDE", _both((13, 16), (33, 16))),
             ("FLAP HALF", _both((14, 9), (32, 9)))]
    strip(img, 40, 580, "HANDS", hands, hb, k=4)
    strip(img, 800, y2, "BEHIND  ·  THE SPIN'S TURN", [("BACK THREE-QUARTER", back_three_quarter()), ("BACK", back())],
          box, k=6)
    text(img, 800, y2 + 150, "NO EYES. THE HANDS HALF IN FRONT", DIM, 2)
    text(img, 800, y2 + 168, "NOW, EACH WHOLLY IN TINT.", DIM, 2)
    text(img, 800, y2 + 186, "MIRRORED, A VIEW FACES THE OTHER WAY.", DIM, 2)
    legs_strip(img, 40, 720)

    if EPAPER:  # the legs' panels end at about 900, their words under them
        one_bit_strip(img, 40, 964, [("FRONT", front()), ("THREE-QUARTER", three_quarter()), ("SIDE", side()),
                                     ("BACK THREE-QUARTER", back_three_quarter()), ("BACK", back())], ONE_BIT_BOX)
    text(img, 40, y4 - 20, "PALETTE BY ROLE" + ("  ·  LEFT: THE GREY, A GREYSCALE FULL REFRESH  ·  RIGHT: ONE BIT" if EPAPER
                                                 else ""), INK, 2)
    if EPAPER:
        _palette_epaper(img, y4)
    else:
        _palette_deck(img, y4)
    mp.write_png(out, img)


def _palette_deck(img, y4):
    lab = mp.role_label
    roles = [("MASCOT_BODY", BODY, lab("MascotBody")), ("MASCOT_SHADE", SHADE, lab("MascotShade")),
             ("MASCOT_TINT", TINT, lab("MascotTint")),
             ("EYES: BACKGROUND", EYE, "BLACK"), ("MASCOT_LAPTOP", mp.LAPTOP, "NEUTRAL 5"),
             ("MASCOT_FLAG", mp.FLAG, lab("MascotFlag")), ("FLAG_CHECK", mp.CHECK, lab("MascotFlagCheck")),
             ("FLAG_POLE", mp.POLE, "NEUTRAL 6"), ("CONFETTI", (0xEA, 0x88, 0xA8), "SUB TINT 3"),
             ("CONFETTI", (0x7A, 0xAA, 0xDC), "SUB TINT 0"), ("MASCOT_DUST", mp.step("neutral", 5), "NEUTRAL 5"),
             ("DUST_FAINT", mp.step("neutral", 3), "NEUTRAL 3"), ("MASCOT_SWEAT", (0x7A, 0xAA, 0xDC), "SUB TINT 0"),
             ("FADED_BODY", mp.step("neutral", 3), "NEUTRAL 3"), ("FADED_SHADE", mp.step("neutral", 2), "NEUTRAL 2"),
             ("FADED_TINT", mp.step("neutral", 4), "NEUTRAL 4")]
    for i, (name, colour, step) in enumerate(roles):
        x, y = 40 + (i % 5) * 222, y4 + (i // 5) * 70
        fill(img, x, y, x + 40, y + 40, colour)
        fill(img, x, y, x + 40, y + 1, DIM)
        fill(img, x, y + 39, x + 40, y + 40, DIM)
        text(img, x + 48, y + 6, name, INK, 2)
        text(img, x + 48, y + 24, step + "  #%02X%02X%02X" % colour, DIM, 1)
    text(img, 40, y4 + 290, "A PART IN FRONT OF THE BODY IS TINTED, A SIDE TURNED AWAY SHADED - NEVER THE BODY'S", DIM, 2)
    text(img, 40, y4 + 308, "OWN COLOUR OVER THE BODY, NEVER AN OUTLINE. A SUB-AGENT TAKES ITS OWN BODY, SHADE, TINT.", DIM, 2)


# Every role a flipbook host's theme may give, in the sheet's order; a role the theme leaves out is left off the sheet.
EPAPER_ROLES = ("MascotBody", "MascotShade", "MascotTint", "MascotLaptop", "MascotFlag", "MascotFlagCheck",
                "MascotFlagPole", "MascotDust", "MascotDustFaint", "MascotSweat", "MascotConfettiA",
                "MascotConfettiB", "MascotFadedBody", "MascotFadedShade", "MascotFadedTint")


def _palette_epaper(img, y4):
    def snake(name):
        return "".join("_" + c if c.isupper() else c for c in name).lstrip("_").upper()
    roles = [("EYES: BACKGROUND", EYE, "BACKGROUND")]
    roles += [(snake(n), mp.role(n), mp.role_label(n)) for n in EPAPER_ROLES if n in mp._ROLES]
    for i, (name, colour, step) in enumerate(roles):
        x, y = 40 + (i % 4) * 292, y4 + (i // 4) * 70
        fill(img, x, y, x + 32, y + 40, colour)
        swatch = mp.one_bit([[colour] * 32 for _ in range(40)], 0, 0)
        for yy, row in enumerate(swatch):
            img[y + yy][x + 32:x + 64] = row
        frame(img, x, y, x + 64, y + 40, DIM)
        text(img, x + 72, y + 6, name, INK, 2)
        text(img, x + 72, y + 24, step + "  #%02X%02X%02X" % colour, DIM, 1)
    y = y4 + ((len(roles) + 3) // 4) * 70 + 10
    for i, line in enumerate((
            "A PART IN FRONT OF THE BODY IS TINTED A LEVEL LIGHTER, A SIDE TURNED AWAY SHADED A LEVEL DARKER - NEVER",
            "THE BODY'S OWN GREY OVER THE BODY, NEVER AN OUTLINE. THE EYES ARE HOLES TO THE GLASS. A PARTIAL REFRESH",
            "DRAWS EACH GREY AS ITS DITHER, SO THE ONE-BIT ROW IS WHAT A FLIPBOOK FRAME SHOWS; THE TRUE GREYS ONLY ON A",
            "STILL, GREYSCALE FULL REFRESH. A SUB-AGENT CANNOT TAKE A HUE: IT IS TOLD BY ITS WORDS.")):
        text(img, 40, y + i * 18, line, DIM, 2)


def _both(left, right):
    cells = mp.front(right=right)
    for c in range(15, 19):
        for r in range(14, 18):
            if not (17 <= c <= 32 and 10 <= r <= 21):
                cells.pop((c, r), None)
    mp.rect(cells, left[0], left[1], 4, 4, BODY)
    return cells


def legs_strip(img, ox, oy, width=176):
    """The legs as the glass draws them — the raised knee's half pixel included — through the preview's renderer, each
    panel the mascot's grid from column 13 to 37 and row 5 to 28, zoomed to about `width` px whatever mp.PX is: row 5 a
    pixel clear of the body's top lifted a jump's rise (row 6), so a pose in the air shows whole — the developer's ask,
    2026-11-24, the tucked and jump panels having cut it off."""
    text(img, ox, oy - 18, "LEGS", INK, 2)
    stage = mp.Stage(backdrop="/nonexistent", clear=(0, 0, 0, 0))
    poses = [("STANDING", mp.Pose(mp.front())),
             ("RAISED: A PIXEL SHORTER, KNEE OUT HALF A PIXEL", mp.Pose(mp.front(), raised="L")),
             ("SQUAT: SHORTER, STRAIGHT", mp.Pose(mp.front(), crouch=8)),
             ("TUCKED: SHORTER, KNEES OUT", mp.Pose(mp.front(), raised="B", lift=12)),
             ("JUMP: FEET SWUNG OUT, CURVED", mp.Pose(mp.front(), lift=12, swing=3))]
    x0, x1 = round(stage.x0 + 13 * mp.PX), round(stage.x0 + 37 * mp.PX)
    y0, y1 = round(stage.y0 + 5 * mp.PX), round(stage.y0 + 28 * mp.PX)
    k = max(1, round(width / (x1 - x0)))
    # Five panels wider than the sheet at the glass's own pixels (a cell past 16 px): every `step`-th pixel, `step` a
    # divisor of the mascot pixel, so each still draws whole.
    step = 1
    room = len(img[0]) - ox
    while len(poses) * ((x1 - x0) // step * k + 18) > room and any(mp.PX % d == 0 for d in range(step + 1, mp.PX + 1)):
        step = next(d for d in range(step + 1, mp.PX + 1) if mp.PX % d == 0)
    for i, (label, pose) in enumerate(poses):
        shot = [row[::step] for row in mp.crop(stage.render(pose), (x0, y0, x1, y1))[::step]]
        pw, ph = len(shot[0]) * k, len(shot) * k
        x = ox + i * (pw + 18)
        for y, row in enumerate(shot):
            for xx, colour in enumerate(row):
                fill(img, x + xx * k, oy + y * k, x + (xx + 1) * k, oy + (y + 1) * k, colour)
        # A caption longer than its panel breaks at its comma, onto a second line.
        lines = label.split(", ", 1) if len(label) * 5 > pw + 18 and ", " in label else [label]
        for j, line in enumerate(lines):
            text(img, x, oy + ph + 6 + 10 * j, line + ("," if j < len(lines) - 1 else ""), DIM, 1)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "model_sheet.png")
