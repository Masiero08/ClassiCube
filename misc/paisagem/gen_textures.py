#!/usr/bin/env python3
"""Generates the original 16x16 textures for the nature blocks of src/Paisagem.c
(tall grass, flowers, birch, berry bushes, sugar cane, pumpkins, cherry and pine leaves).
All art here is procedurally drawn by this script; no third-party textures are used.
Usage: python3 gen_textures.py <output .h> [preview .png]"""
import random, sys, math
from PIL import Image

S = 16

def clamp(v): return max(0, min(255, int(v)))
def shade(c, f): return tuple(clamp(x * f) for x in c[:3]) + ((c[3],) if len(c) > 3 else (255,))
def new(): return Image.new("RGBA", (S, S), (0, 0, 0, 0))
def rgba(c): return tuple(c[:3]) + (255,)

def noise(base, amount, seed):
    rnd = random.Random(seed); im = new(); px = im.load()
    for y in range(S):
        for x in range(S):
            px[x, y] = shade(base, 1 + rnd.uniform(-amount, amount))
    return im

def blade(px, x0, h, lean, col, rnd):
    """A grass blade growing up from the bottom row, leaning sideways"""
    for i in range(h):
        y = S - 1 - i
        x = int(round(x0 + lean * i * i / max(1, h * 2)))
        if 0 <= x < S:
            px[x, y] = shade(col, 0.75 + 0.45 * i / h + rnd.uniform(-0.05, 0.05))

def tall_grass():
    im = new(); px = im.load(); rnd = random.Random(10)
    base = (88, 168, 60)
    for x0 in [1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14]:
        blade(px, x0, rnd.randint(7, 14), rnd.uniform(-1.6, 1.6), base, rnd)
    return im

def fern():
    im = new(); px = im.load(); rnd = random.Random(11)
    col = (58, 128, 52)
    for stem_x, lean in [(5, -2.2), (8, 0.3), (11, 2.0)]:
        for i in range(13):
            y = S - 1 - i
            x = int(round(stem_x + lean * i / 12))
            px[min(S - 1, max(0, x)), y] = shade(col, 0.8 + i / 40)
            # leaflets on both sides, smaller near the tip
            w = max(0, 3 - i // 5)
            if i > 2 and i % 2 == 0:
                for k in range(1, w + 1):
                    for xx in (x - k, x + k):
                        if 0 <= xx < S: px[xx, min(S - 1, y + (1 if k == w else 0))] = shade(col, 1.05)
    return im

def flower(petal, center, seed, heads):
    im = new(); px = im.load(); rnd = random.Random(seed)
    stem = (60, 140, 50, 255)
    for hx, hy in heads:
        for y in range(hy + 2, S):
            px[hx, y] = stem
        # a leaf on the stem
        ly = rnd.randint(hy + 4, S - 3)
        lx = hx + rnd.choice((-1, 1))
        if 0 <= lx < S: px[lx, ly] = shade(stem, 1.15)
        for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, -1)]:
            x, y = hx + dx, hy + dy
            if 0 <= x < S and 0 <= y < S: px[x, y] = shade(petal, 1 + rnd.uniform(-0.1, 0.1))
        px[hx, hy] = rgba(center)
    return im

def tulip(petal, seed):
    im = new(); px = im.load()
    stem = (60, 140, 50, 255)
    for y in range(7, S): px[8, y] = stem
    for y, x in [(12, 6), (11, 7), (13, 9), (12, 10)]: px[x, y] = shade(stem, 1.15)
    # cup shaped bloom
    for y in range(2, 8):
        w = 2 if y < 6 else 1
        for x in range(8 - w, 8 + w + 1):
            f = 1.15 if x < 8 else 0.9
            if y == 2 and x == 8: continue
            px[x, y] = shade(petal, f - (y - 2) * 0.03)
    px[7, 2] = px[9, 2] = shade(petal, 1.2)
    return im

def birch_side():
    im = noise((226, 224, 214), 0.04, 20); px = im.load(); rnd = random.Random(21)
    dark = (40, 38, 36, 255)
    for _ in range(7):
        y = rnd.randrange(S); x0 = rnd.randrange(S); w = rnd.randint(2, 5)
        for x in range(x0, x0 + w): px[x % S, y] = dark
        if rnd.random() < 0.5 and y + 1 < S: px[x0 % S, y + 1] = dark
    for y in range(S): px[0, y] = shade(px[0, y], 0.85); px[S - 1, y] = shade(px[S - 1, y], 0.85)
    return im

def log_top(bark, inner, seed):
    im = new(); px = im.load(); rnd = random.Random(seed)
    for y in range(S):
        for x in range(S):
            d = math.hypot(x - 7.5, y - 7.5)
            if d > 6.6: px[x, y] = shade(bark, 1 + rnd.uniform(-0.08, 0.08))
            else:
                ring = 0.92 if int(d) % 2 else 1.05
                px[x, y] = shade(inner, ring + rnd.uniform(-0.04, 0.04))
    return im

def leaves(base, seed, holes=0.18, accents=None):
    rnd = random.Random(seed); im = new(); px = im.load()
    for y in range(S):
        for x in range(S):
            if rnd.random() < holes: continue
            c = shade(base, 1 + rnd.uniform(-0.22, 0.18))
            if accents and rnd.random() < 0.08: c = rgba(rnd.choice(accents))
            px[x, y] = c
    return im

def bush(berry, seed):
    im = new(); px = im.load(); rnd = random.Random(seed)
    leaf = (52, 120, 48)
    for y in range(4, S):
        for x in range(S):
            d = math.hypot((x - 7.5) / 7.5, (y - 10) / 6.2)
            if d < 1 and rnd.random() < 0.85: px[x, y] = shade(leaf, 0.8 + rnd.uniform(0, 0.4))
    for _ in range(7):
        x, y = rnd.randrange(3, 13), rnd.randrange(6, 14)
        px[x, y] = rgba(berry); px[x, y - 1] = shade(berry, 1.3)
    return im

def berries(berry):
    im = new(); px = im.load()
    for cx, cy in [(6, 9), (10, 9), (8, 12), (8, 7)]:
        for dx in range(-1, 2):
            for dy in range(-1, 2):
                if abs(dx) + abs(dy) < 2: px[cx + dx, cy + dy] = shade(berry, 0.85)
        px[cx - 1, cy - 1] = shade(berry, 1.4)
    for y in range(3, 7): px[8, y] = (70, 110, 40, 255)
    px[9, 4] = px[10, 4] = (80, 150, 60, 255)
    return im

def sugar_cane():
    im = new(); px = im.load(); rnd = random.Random(30)
    col = (130, 190, 90)
    for sx in (3, 8, 12):
        for y in range(S):
            node = (y + sx) % 5 == 0
            px[sx, y] = shade(col, 0.75 if node else 1 + rnd.uniform(-0.06, 0.06))
            px[sx + 1, y] = shade(col, 0.65 if node else 0.88)
        ly = rnd.randrange(2, 12)
        px[sx - 1, ly] = px[sx - 2 if sx > 1 else sx - 1, ly - 1] = shade(col, 1.1)
    return im

def pumpkin_side():
    im = noise((222, 128, 30), 0.05, 40); px = im.load()
    for x in (1, 5, 10, 14):
        for y in range(S): px[x, y] = shade(px[x, y], 0.78)
    for y in (0, S - 1):
        for x in range(S): px[x, y] = shade(px[x, y], 0.85)
    return im

def pumpkin_top():
    im = pumpkin_side(); px = im.load()
    for y in range(S):
        for x in range(S):
            d = math.hypot(x - 7.5, y - 7.5)
            px[x, y] = shade(px[x, y], 1.05 - d * 0.02)
    for x, y in [(7, 7), (8, 7), (7, 8), (8, 8), (8, 6), (9, 5)]:
        px[x, y] = (96, 72, 32, 255)
    return im

def moss_stone():
    im = noise((122, 122, 122), 0.13, 50); px = im.load(); rnd = random.Random(51)
    for _ in range(40):
        x, y = rnd.randrange(S), rnd.randrange(S)
        px[x, y] = shade((80, 130, 60), 1 + rnd.uniform(-0.2, 0.2))
    return im

textures = {}
ORDER = []
def T(name, im): textures[name] = im; ORDER.append(name)

T("tall_grass",   tall_grass())
T("fern",         fern())
T("tulip",        tulip((235, 110, 40), 60))
T("blue_flower",  flower((70, 120, 230), (240, 220, 90), 61, [(5, 5), (11, 3)]))
T("daisies",      flower((245, 245, 240), (240, 200, 50), 62, [(4, 7), (9, 3), (12, 8)]))
T("birch_side",   birch_side())
T("birch_top",    log_top((226, 224, 214), (200, 180, 130), 22))
T("birch_leaves", leaves((120, 170, 70), 23))
T("berry_bush",   bush((200, 30, 50), 70))
T("berries",      berries((205, 30, 55)))
T("sugar_cane",   sugar_cane())
T("pumpkin_side", pumpkin_side())
T("pumpkin_top",  pumpkin_top())
T("cherry_leaves", leaves((240, 160, 200), 80, 0.15, [(255, 220, 235), (210, 110, 160)]))
T("pine_leaves",  leaves((40, 92, 52), 81, 0.12))
def pine_bark():
    im = noise((88, 60, 38), 0.08, 82); px = im.load(); rnd = random.Random(83)
    for x in range(0, S, 3):
        for y in range(S):
            if rnd.random() < 0.8: px[x, y] = shade(px[x, y], 0.7)
    return im
T("pine_log_side", pine_bark())

def write_header(path):
    with open(path, "w", newline="\r\n") as f:
        f.write("/* Generated by misc/paisagem/gen_textures.py - do not edit */\n")
        f.write("#define PAISAGEM_TEX_COUNT %d\n" % len(ORDER))
        f.write("#define PAISAGEM_TEX_SIZE  %d\n" % S)
        for i, n in enumerate(ORDER):
            f.write("#define PTEX_%s %d\n" % (n.upper(), i))
        f.write("static const cc_uint8 paisagem_texData[PAISAGEM_TEX_COUNT][PAISAGEM_TEX_SIZE * PAISAGEM_TEX_SIZE * 4] = {\n")
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
