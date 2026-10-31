#!/usr/bin/env python3
"""Generates the original 16x16 textures for chests, the lit furnace, metal bars and armour (src/Equipamento.c).
All art here is procedurally drawn by this script; no third-party textures are used.
Usage: python3 gen_textures.py <output .h> [preview .png]"""
import random, sys
from PIL import Image

S = 16

def clamp(v): return max(0, min(255, int(v)))
def shade(c, f): return tuple(clamp(x * f) for x in c[:3]) + (255,)
def new(): return Image.new("RGBA", (S, S), (0, 0, 0, 0))

def noise(base, amount, seed):
    rnd = random.Random(seed); im = new(); px = im.load()
    for y in range(S):
        for x in range(S):
            px[x, y] = shade(base, 1 + rnd.uniform(-amount, amount))
    return im

textures = {}
ORDER = []
def T(name, im): textures[name] = im; ORDER.append(name)

# ---------------------------------------------------------------- Chest (bau)
WOOD   = (150, 100, 52)
WOOD_D = (98, 62, 30)
WOOD_L = (182, 130, 72)
BAND   = (70, 50, 32)
LATCH  = (215, 200, 120)

def planks(seed, vertical=False):
    im = noise(WOOD, 0.08, seed); p = im.load()
    for y in range(S):
        for x in range(S):
            if (y if not vertical else x) % 4 == 0: p[x, y] = shade(WOOD_D, 1.05)
            elif (x * 7 + y * 3 + seed) % 11 == 0: p[x, y] = shade(WOOD_L, 0.95)
    return im

top = planks(31); p = top.load()
for i in range(S):
    p[i, 0] = p[0, i] = p[i, 15] = p[15, i] = BAND
for i in range(1, 15):
    p[i, 1] = p[1, i] = WOOD_L
T("bau_top", top)

side = planks(32); p = side.load()
for i in range(S):
    p[i, 0] = p[0, i] = p[i, 15] = p[15, i] = BAND
for x in range(S): p[x, 6] = BAND; p[x, 5] = WOOD_D        # gap between lid and box
for y in range(4, 10):                                       # latch
    for x in range(6, 10):
        p[x, y] = LATCH if (x in (6, 9) or y in (4, 9)) else shade(LATCH, 0.75)
p[7, 7] = p[8, 7] = (60, 50, 30, 255)
T("bau_side", side)

# ---------------------------------------------------------------- Lit furnace
fur = noise((110, 110, 112), 0.1, 18); p = fur.load()
for i in range(S): p[i, 0] = p[i, 15] = (70, 70, 72, 255)
for x in range(3, 13): p[x, 7] = (80, 80, 82, 255)
rnd = random.Random(5)
for y in range(8, 14):
    for x in range(4, 12):
        t = (y - 8) / 5.0
        p[x, y] = (255, clamp(110 + t * 120 + rnd.randint(-20, 20)), clamp(20 + t * 60), 255)
for x in range(4, 12):                                       # flames licking out of the mouth
    if x % 2 == 0: p[x, 7] = (255, 170, 40, 255)
for x in (5, 8, 10): p[x, 6] = (255, 120, 30, 255)
T("fornalha_acesa", fur)

# ---------------------------------------------------------------- Metal bars
def bar(base):
    im = new(); p = im.load()
    light, dark = shade(base, 1.3), shade(base, 0.6)
    # simple trapezoid: top face narrower than the bottom face
    for y in range(5, 12):
        x0 = 4 - min(y - 5, 2); x1 = 12 + min(y - 5, 2)
        for x in range(x0, x1):
            c = base
            if y <= 6: c = light
            if y >= 10: c = dark
            p[x, y] = c
        p[x0, y] = dark; p[x1 - 1, y] = dark
    for x in range(4, 12): p[x, 4] = dark
    for x in range(2, 14): p[x, 12] = shade(base, 0.45)
    p[6, 6] = p[7, 6] = (255, 255, 255, 255)
    return im

T("barra_ferro",   bar((205, 205, 210)))
T("barra_ouro",    bar((240, 200, 60)))
T("barra_titanio", bar((140, 160, 190)))

# ---------------------------------------------------------------- Armour
# '#' outline, 'l' light, 'b' base, 'd' dark, 'x' detail (rivets/stitches)
SHAPES = {
    "capacete": [
        "................",
        "................",
        "................",
        "....########....",
        "...#llllllll#...",
        "..#llbbbbbbbb#..",
        "..#lbbbxbbbbb#..",
        "..#bbbbbbbbbd#..",
        "..#bbb####bbd#..",
        "..#bb#....#bd#..",
        "..#bd#....#dd#..",
        "..####....####..",
        "................",
        "................",
        "................",
        "................",
    ],
    "peitoral": [
        "................",
        "..####....####..",
        ".#llb#....#bbd#.",
        "#lbbb######bbbd#",
        "#lbbbllllbbbbbd#",
        "#lbbbbbbbbbbbbd#",
        "####bbbxxbbbd###",
        "...#lbbbbbbbd#..",
        "...#lbbbbbbbd#..",
        "...#lbbxxbbbd#..",
        "...#lbbbbbbbd#..",
        "...#bbbbbbbbd#..",
        "...#ddddddddd#..",
        "...###########..",
        "................",
        "................",
    ],
    "calca": [
        "................",
        "...##########...",
        "...#lllllllx#...",
        "...#lbbbbbbd#...",
        "...#lbbbbbbd#...",
        "...#lbb##bbd#...",
        "...#lbd##lbd#...",
        "...#lbd##lbd#...",
        "...#lbd##lbd#...",
        "...#lbd##lbd#...",
        "...#lbd##lbd#...",
        "...#ldd##ldd#...",
        "...####..####...",
        "................",
        "................",
        "................",
    ],
    "botas": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "..####....####..",
        "..#lb#....#lb#..",
        "..#lb#....#lb#..",
        "..#lb#....#lb#..",
        "..#lbd#...#lbd#.",
        ".#lbbbd#.#lbbbd#",
        ".#ddddd#.#ddddd#",
        ".#######.#######",
        "................",
        "................",
        "................",
    ],
}

TIERS = {
    "la":       ((235, 232, 220), (120, 110, 100)),
    "ouro":     ((240, 195, 50),  (255, 245, 170)),
    "ferro":    ((200, 200, 205), (90, 90, 100)),
    "diamante": ((80, 220, 215),  (230, 255, 255)),
    "titanio":  ((130, 150, 185), (200, 120, 230)),
}

def armour(piece, tier):
    base, detail = TIERS[tier]
    cols = {"#": shade(base, 0.35), "l": shade(base, 1.25), "b": shade(base, 1.0),
            "d": shade(base, 0.7), "x": detail + (255,)}
    im = new(); p = im.load()
    for y, row in enumerate(SHAPES[piece]):
        for x, c in enumerate(row):
            if c in cols: p[x, y] = cols[c]
    if tier == "la":   # stitches on wool armour
        for y, row in enumerate(SHAPES[piece]):
            for x, c in enumerate(row):
                if c == "b" and (x + y) % 5 == 0: p[x, y] = shade(base, 0.85)
    return im

for tier in ("la", "ouro", "ferro", "diamante", "titanio"):
    for piece in ("capacete", "peitoral", "calca", "botas"):
        if tier == "titanio" and piece == "peitoral": continue   # Minerios already has this item
        T("%s_%s" % (piece, tier), armour(piece, tier))

# Shapes are also exported so the inventory screen can draw the worn armour on the player figure
def write_header(path):
    with open(path, "w", newline="\r\n") as f:
        f.write("/* Generated by misc/equipamento/gen_textures.py - do not edit */\n")
        f.write("#define EQUIP_TEX_COUNT %d\n" % len(ORDER))
        f.write("#define EQUIP_TEX_SIZE  %d\n" % S)
        for i, n in enumerate(ORDER):
            f.write("#define ETEX_%s %d\n" % (n.upper(), i))
        f.write("static const cc_uint8 equip_texData[EQUIP_TEX_COUNT][EQUIP_TEX_SIZE * EQUIP_TEX_SIZE * 4] = {\n")
        for n in ORDER:
            data = list(textures[n].convert("RGBA").tobytes())
            f.write("\t{ /* %s */\n" % n)
            for i in range(0, len(data), 64):
                f.write("\t\t" + ",".join(str(b) for b in data[i:i + 64]) + ",\n")
            f.write("\t},\n")
        f.write("};\n")

def write_preview(path):
    im = Image.new("RGBA", (S * len(ORDER), S), (40, 40, 40, 255))
    for i, n in enumerate(ORDER): im.alpha_composite(textures[n], (i * S, 0))
    im.resize((im.width * 6, im.height * 6), Image.NEAREST).save(path)

if __name__ == "__main__":
    write_header(sys.argv[1])
    if len(sys.argv) > 2: write_preview(sys.argv[2])
