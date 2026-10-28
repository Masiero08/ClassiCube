#!/usr/bin/env python3
"""Generates the original 16x16 textures for diamond, Nether and End blocks (src/LateGame.c).
All art here is procedurally drawn by this script; no third-party textures are used.
Usage: python3 gen_textures.py <output .h> [preview .png]"""
import random, sys, math
from PIL import Image

S = 16

def clamp(v): return max(0, min(255, int(v)))
def shade(c, f): return tuple(clamp(x * f) for x in c[:3]) + ((c[3],) if len(c) > 3 else (255,))
def new(): return Image.new("RGBA", (S, S), (0, 0, 0, 0))

def noise(base, amount, seed):
    rnd = random.Random(seed); im = new(); px = im.load()
    for y in range(S):
        for x in range(S):
            px[x, y] = shade(base, 1 + rnd.uniform(-amount, amount))
    return im

def blobs(im, colors, count, seed, size=2):
    """Small irregular clusters, used for ore spots"""
    rnd = random.Random(seed); px = im.load()
    for _ in range(count):
        cx, cy = rnd.randrange(1, S - 2), rnd.randrange(1, S - 2)
        for _ in range(size + 2):
            x = clamp(cx + rnd.randrange(-1, 2)); y = clamp(cy + rnd.randrange(-1, 2))
            x = min(x, S - 1); y = min(y, S - 1)
            px[x, y] = rnd.choice(colors)
    return im

def bricks(base, mortar, rows=4, seed=0):
    im = noise(base, 0.1, seed); px = im.load(); h = S // rows
    for r in range(rows):
        y0 = r * h
        for x in range(S): px[x, y0] = mortar
        off = 0 if r % 2 == 0 else S // 4
        for k in range(2):
            x = (off + k * S // 2) % S
            for y in range(y0, y0 + h): px[x, y] = mortar
    return im

def gem(fill, light, dark):
    """A faceted gem sprite (transparent background)"""
    im = new(); px = im.load()
    for y in range(3, 13):
        w = 6 - abs(y - 7) if y <= 7 else max(0, 6 - (y - 7) * 6 // 5)
        for x in range(8 - w, 8 + w):
            c = fill
            if x < 8 and y < 8: c = light
            if x >= 8 and y >= 8: c = dark
            px[x, y] = c
    for y in range(3, 13):
        for x in range(S):
            if px[x, y][3] and (x == 0 or not px[x - 1, y][3] or x == S - 1 or not px[x + 1, y][3]):
                px[x, y] = shade(dark, 0.8)
    px[6, 5] = (255, 255, 255, 255)
    return im

def swirl(base, other, seed):
    im = new(); px = im.load(); rnd = random.Random(seed)
    for y in range(S):
        for x in range(S):
            a = math.sin((x + y) * 0.7) + math.cos((x - y) * 0.5 + rnd.uniform(-0.3, 0.3))
            px[x, y] = base if a > 0.2 else other
            px[x, y] = shade(px[x, y], 1 + rnd.uniform(-0.1, 0.1))[:3] + (200,)
    return im

textures = {}
ORDER = []
def T(name, im): textures[name] = im; ORDER.append(name)

stone = noise((125, 125, 125), 0.12, 1)
T("diamond_ore", blobs(stone, [(90, 230, 225, 255), (40, 180, 190, 255), (180, 255, 250, 255)], 6, 2, size=5))
T("diamond", gem((70, 215, 215, 255), (190, 255, 250, 255), (30, 140, 160, 255)))
blk = noise((95, 225, 215), 0.06, 3); p = blk.load()
for i in range(S):
    p[i, 0] = p[0, i] = (200, 255, 250, 255); p[i, S - 1] = p[S - 1, i] = (40, 150, 150, 255)
for i in range(3, 13): p[i, i] = (170, 250, 245, 255)
T("diamond_block", blk)

T("netherrack", blobs(noise((115, 45, 45), 0.18, 4), [(80, 25, 25, 255), (140, 60, 55, 255)], 10, 5))
T("nether_bricks", bricks((60, 25, 30), (25, 10, 15, 255), seed=6))
ss = noise((85, 65, 50), 0.15, 7); p = ss.load()
for (x, y) in [(3, 4), (10, 3), (6, 10), (12, 11)]:
    p[x, y] = p[x + 1, y] = (40, 28, 22, 255); p[x, y + 1] = (50, 35, 28, 255)
T("soul_sand", ss)
T("glowstone", blobs(noise((200, 150, 70), 0.2, 8), [(255, 230, 140, 255), (150, 100, 50, 255)], 12, 9))
T("quartz_ore", blobs(noise((115, 45, 45), 0.18, 10), [(240, 235, 225, 255), (210, 200, 190, 255)], 6, 11, size=5))
T("quartz", gem((235, 230, 220, 255), (255, 255, 255, 255), (170, 160, 150, 255)))
T("nether_portal", swirl((120, 40, 200, 255), (70, 10, 140, 255), 12))
T("end_stone", blobs(noise((220, 220, 160), 0.08, 13), [(190, 190, 130, 255)], 8, 14))
ep = noise((20, 30, 30), 0.3, 15); p = ep.load(); rnd = random.Random(16)
for _ in range(14): p[rnd.randrange(S), rnd.randrange(S)] = (120, 220, 200, 255)
T("end_portal", ep)
egg = new(); p = egg.load(); rnd = random.Random(17)
for y in range(2, 16):
    r = 5.5 * math.sin(math.pi * (y - 1) / 15) ** 0.7
    for x in range(S):
        if abs(x - 7.5) <= r:
            p[x, y] = (40, 10, 50, 255) if rnd.random() > 0.2 else (120, 40, 150, 255)
T("dragon_egg", egg)
pearl = new(); p = pearl.load()
for y in range(S):
    for x in range(S):
        d = math.hypot(x - 7.5, y - 7.5)
        if d < 5.5:
            p[x, y] = (30, 110, 100, 255) if d > 3 else (60, 190, 170, 255)
p[6, 6] = (200, 255, 240, 255)
T("ender_pearl", pearl)

# Food, furnace and bed (survival)
def meat(raw):
    im = new(); p = im.load()
    flesh = (205, 70, 70, 255) if raw else (150, 85, 45, 255)
    fat   = (240, 190, 180, 255) if raw else (205, 150, 95, 255)
    for y in range(S):
        for x in range(S):
            d = math.hypot((x - 7) / 6.0, (y - 6.5) / 4.5)
            if d < 1.0:
                p[x, y] = shade(flesh, 1.15 - d * 0.35)
                if 0.8 < d: p[x, y] = fat
    for y in range(10, 15):                                  # bone handle
        p[11 + (y - 10) // 2, y] = (235, 230, 215, 255)
    p[13, 14] = p[14, 14] = (250, 248, 240, 255)
    if not raw:
        for (x, y) in [(4, 5), (7, 4), (9, 7), (5, 8)]: p[x, y] = (100, 55, 25, 255)
    return im
T("raw_meat", meat(True))
T("cooked_meat", meat(False))

fur = noise((110, 110, 112), 0.1, 18); p = fur.load()
for i in range(S): p[i, 0] = p[i, 15] = (70, 70, 72, 255)
for y in range(8, 14):                                       # fire mouth
    for x in range(4, 12):
        p[x, y] = (30, 25, 25, 255)
for x in range(5, 11): p[x, 12] = (255, 140, 40, 255); p[x, 13] = (255, 200, 80, 255)
for x in (6, 8, 9): p[x, 11] = (240, 90, 30, 255)
for x in range(3, 13): p[x, 7] = (80, 80, 82, 255)
T("furnace_side", fur)
ft = noise((120, 120, 122), 0.08, 19); p = ft.load()
for i in range(S): p[i, 0] = p[0, i] = (150, 150, 152, 255); p[i, 15] = p[15, i] = (70, 70, 72, 255)
T("furnace_top", ft)

bt = new(); p = bt.load()
for y in range(S):
    for x in range(S):
        p[x, y] = (175, 35, 40, 255) if y > 5 else (235, 235, 240, 255)   # blanket and pillow
        if y > 5 and (x + y) % 6 == 0: p[x, y] = (150, 25, 32, 255)
        if x in (0, 15) or y in (0, 15): p[x, y] = (120, 85, 50, 255)
for x in range(1, 15): p[x, 6] = (215, 60, 65, 255)
T("bed_top", bt)
bs = new(); p = bs.load()
for y in range(7, 16):
    for x in range(S):
        p[x, y] = (175, 35, 40, 255) if y < 11 else (130, 92, 55, 255)
        if y >= 11 and x in (0, 1, 14, 15): p[x, y] = (95, 65, 38, 255)
for x in range(S): p[x, 7] = (215, 60, 65, 255)
T("bed_side", bs)

# Stick (graveto) for tool handles in the crafting grid
st = new(); p = st.load()
for i in range(3, 14):                                       # diagonal stick, light top edge
    x, y = 15 - i, i
    p[x, y] = (150, 105, 60, 255); p[x - 1, y] = (110, 75, 40, 255)
    if i % 4 == 0: p[x, y] = (175, 130, 80, 255)
p[12, 3] = (90, 60, 32, 255)
T("graveto", st)

# Ender dragon model skin (64x64, box layout matches the parts in src/LateGame.c)
def dragon_skin():
    im = Image.new("RGBA", (64, 64), (0, 0, 0, 0)); px = im.load(); rnd = random.Random(20)
    def area(x0, y0, w, h, base, spots):
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                c = base if (x + y) % 4 else shade(base, 0.8)
                if rnd.random() < 0.08: c = spots
                px[x, y] = shade(c, 1 + rnd.uniform(-0.08, 0.08))
    dark = (28, 22, 38, 255); spot = (70, 50, 90, 255)
    area(0, 0, 64, 13, (60, 55, 75, 255), (90, 80, 110, 255))   # wings
    area(0, 13, 52, 24, dark, spot)                             # body
    area(0, 37, 32, 15, dark, spot)                             # tail
    area(32, 37, 28, 14, dark, spot)                            # head
    area(0, 52, 20, 10, dark, spot)                             # neck
    area(20, 52, 8, 8, dark, spot)                              # legs
    for ex in (41, 44, 47, 50, 53, 56):                         # glowing purple eyes on the head sides
        px[ex, 47] = (220, 80, 255, 255); px[ex, 48] = (160, 40, 220, 255)
    for x in range(0, 64, 4):                                   # wing bones
        for y in range(0, 13): px[x, y] = (35, 30, 45, 255)
    return im

DRAGON = dragon_skin()

def write_header(path):
    with open(path, "w", newline="\r\n") as f:
        f.write("/* Generated by misc/late_game/gen_textures.py - do not edit */\n")
        f.write("#define LATE_TEX_COUNT %d\n" % len(ORDER))
        f.write("#define LATE_TEX_SIZE  %d\n" % S)
        for i, n in enumerate(ORDER):
            f.write("#define LTEX_%s %d\n" % (n.upper(), i))
        f.write("static const cc_uint8 late_texData[LATE_TEX_COUNT][LATE_TEX_SIZE * LATE_TEX_SIZE * 4] = {\n")
        for n in ORDER:
            data = list(textures[n].convert("RGBA").tobytes())
            f.write("\t{ /* %s */\n" % n)
            for i in range(0, len(data), 64):
                f.write("\t\t" + ",".join(str(b) for b in data[i:i + 64]) + ",\n")
            f.write("\t},\n")
        f.write("};\n")
        import io
        buf = io.BytesIO(); DRAGON.save(buf, "PNG"); data = buf.getvalue()
        f.write("static const cc_uint8 late_dragonPng[%d] = {\n" % len(data))
        for i in range(0, len(data), 32):
            f.write("\t" + ",".join(str(b) for b in data[i:i + 32]) + ",\n")
        f.write("};\n")

def write_preview(path):
    im = Image.new("RGBA", (S * len(ORDER), S), (40, 40, 40, 255))
    for i, n in enumerate(ORDER): im.alpha_composite(textures[n], (i * S, 0))
    im.resize((im.width * 6, im.height * 6), Image.NEAREST).save(path)

if __name__ == "__main__":
    write_header(sys.argv[1])
    if len(sys.argv) > 2: write_preview(sys.argv[2])
