#!/usr/bin/env python3
"""Generates the original 16x16 textures for the modern blocks (src/ModernBlocks.c).
All art here is procedurally drawn by this script; no third-party textures are used.
Usage: python3 gen_textures.py <output .h> [preview .png]"""
import random, sys
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

def speckle(im, colors, count, seed):
    rnd = random.Random(seed); px = im.load()
    for _ in range(count):
        x, y = rnd.randrange(S), rnd.randrange(S)
        px[x, y] = rnd.choice(colors)
    return im

def bricks(base, mortar, rows=4, seed=0, amount=0.08):
    im = noise(base, amount, seed); px = im.load(); h = S // rows
    for r in range(rows):
        y0 = r * h
        for x in range(S): px[x, y0] = mortar
        off = 0 if r % 2 == 0 else S // 4
        for k in range(2):
            x = (off + k * S // 2) % S
            for y in range(y0, y0 + h): px[x, y] = mortar
    return im

def tiles(base, line, n=2, seed=0):
    im = noise(base, 0.06, seed); px = im.load(); step = S // n
    for i in range(0, S, step):
        for j in range(S):
            px[i, j] = line; px[j, i] = line
    for i in range(0, S, step):
        for j in range(S):
            x2 = min(S - 1, i + step - 1)
            px[x2, j] = shade(px[x2, j], 0.85)
    return im

def planks(base, seed=0):
    im = noise(base, 0.05, seed); px = im.load(); rnd = random.Random(seed)
    for r in range(4):
        y0 = r * 4
        for x in range(S): px[x, y0 + 3] = shade(base, 0.7)
        cut = rnd.randrange(S)
        for y in range(y0, y0 + 3): px[cut, y] = shade(base, 0.75)
        for _ in range(3):
            px[rnd.randrange(S), y0 + rnd.randrange(3)] = shade(base, 0.88)
    return im

def log_side(bark, seed=0):
    im = noise(bark, 0.08, seed); px = im.load(); rnd = random.Random(seed)
    for x in range(0, S, 3):
        for y in range(S):
            if rnd.random() < 0.7: px[x, y] = shade(bark, 0.7)
    return im

def log_top(bark, inner, seed=0):
    im = noise(inner, 0.04, seed); px = im.load()
    for y in range(S):
        for x in range(S):
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d > 6.5: px[x, y] = shade(bark, 1.0)
            elif int(d) % 3 == 0: px[x, y] = shade(inner, 0.85)
    return im

def column_side(base, line, seed=0):
    im = noise(base, 0.06, seed); px = im.load()
    for x in range(S):
        if x % 4 == 0: 
            for y in range(S): px[x, y] = shade(line, 1)
    return im

def crystal(base, seed=0):
    im = noise(base, 0.12, seed); px = im.load(); rnd = random.Random(seed)
    for _ in range(6):
        x, y = rnd.randrange(S), rnd.randrange(S)
        for k in range(4):
            px[(x + k) % S, (y + k) % S] = shade(base, 1.35)
    return im

def glow(base, core, seed=0):
    im = noise(base, 0.08, seed); px = im.load()
    for y in range(3, 13):
        for x in range(3, 13):
            px[x, y] = shade(core, 1 + 0.03 * ((x * 7 + y * 3) % 5))
    for i in range(S):
        px[i, 0] = px[i, S-1] = px[0, i] = px[S-1, i] = shade(base, 0.7)
    return im

def lantern(seed=0):
    im = new(); px = im.load()
    metal = (60, 64, 72, 255); light = (255, 200, 90, 255); core = (255, 240, 170, 255)
    for x in range(7, 9): 
        for y in range(1, 4): px[x, y] = metal
    for x in range(5, 11): px[x, 4] = metal; px[x, 13] = metal
    for y in range(5, 13):
        px[5, y] = metal; px[10, y] = metal
        for x in range(6, 10): px[x, y] = light
    for y in range(7, 11):
        for x in range(7, 9): px[x, y] = core
    return im

# name, (top, side, bottom) textures as images
textures = {}
def T(name, im): textures[name] = im; return name

DEEP = (78, 78, 86)
T("deepslate_top",   speckle(noise(DEEP, 0.10, 1), [shade(DEEP, 0.75)], 30, 2))
T("deepslate_side",  column_side(DEEP, shade(DEEP, 0.8), 3))
T("cobbled_deepslate", speckle(noise(shade(DEEP, 0.9), 0.18, 4), [shade(DEEP, 0.6), shade(DEEP, 1.2)], 50, 5))
T("deepslate_bricks", bricks(shade(DEEP, 0.95), shade(DEEP, 0.55), 4, 6))
T("deepslate_tiles",  tiles(shade(DEEP, 0.85), shade(DEEP, 0.5), 4, 7))
T("tuff",       speckle(noise((108, 109, 102), 0.10, 8), [(90, 92, 84, 255), (128, 128, 118, 255)], 40, 9))
T("calcite",    speckle(noise((222, 223, 220), 0.04, 10), [(200, 202, 200, 255)], 25, 11))
T("granite",    speckle(noise((154, 106, 89), 0.10, 12), [(190, 140, 120, 255), (120, 80, 70, 255)], 45, 13))
T("diorite",    speckle(noise((190, 190, 192), 0.06, 14), [(130, 130, 132, 255), (230, 230, 232, 255)], 45, 15))
T("andesite",   speckle(noise((134, 134, 136), 0.08, 16), [(110, 110, 112, 255), (160, 160, 162, 255)], 45, 17))
COPPER = (192, 108, 80)
T("copper",     speckle(noise(COPPER, 0.08, 18), [shade(COPPER, 1.25)], 20, 19))
T("cut_copper", tiles(COPPER, shade(COPPER, 0.7), 2, 20))
OXI = (82, 162, 132)
T("oxidized_copper", speckle(noise(OXI, 0.10, 21), [shade(OXI, 1.2), shade(OXI, 0.8)], 30, 22))
T("amethyst",   crystal((133, 97, 191), 23))
T("blackstone", speckle(noise((44, 38, 46), 0.15, 24), [(60, 54, 64, 255)], 30, 25))
T("blackstone_bricks", bricks((52, 46, 56), (26, 22, 28, 255), 4, 26))
T("mud_bricks", bricks((140, 106, 80), (100, 76, 58, 255), 4, 27, 0.05))
CHERRY = (226, 178, 172)
T("cherry_planks", planks(CHERRY, 28))
T("cherry_log_side", log_side((54, 30, 40), 29))
T("cherry_log_top",  log_top((54, 30, 40), CHERRY, 30))
MANG = (118, 54, 48)
T("mangrove_planks", planks(MANG, 31))
BAMBOO = (196, 176, 82)
T("bamboo_planks", column_side(BAMBOO, shade(BAMBOO, 0.8), 32))
T("moss",       speckle(noise((90, 110, 44), 0.12, 33), [(110, 134, 54, 255), (70, 88, 34, 255)], 40, 34))
T("prismarine", speckle(noise((99, 156, 151), 0.12, 35), [(80, 130, 140, 255), (130, 180, 160, 255)], 40, 36))
T("sea_lantern", glow((172, 199, 190), (222, 236, 228), 37))
T("shroomlight", glow((240, 146, 70), (255, 190, 100), 38))
T("terracotta", noise((152, 94, 67), 0.05, 39))
T("packed_ice", speckle(noise((141, 180, 250), 0.05, 40), [(170, 200, 255, 255)], 20, 41))
T("basalt_side", column_side((80, 80, 86), (60, 60, 66), 42))
T("basalt_top",  log_top((70, 70, 76), (96, 96, 102), 43))
T("lantern",    lantern())

ORDER = list(textures.keys())

def write_header(path):
    with open(path, "w", newline="\r\n") as f:
        f.write("/* Generated by misc/modern_blocks/gen_textures.py - do not edit */\n")
        f.write("#define MODERN_TEX_COUNT %d\n" % len(ORDER))
        f.write("#define MODERN_TEX_SIZE  %d\n" % S)
        for i, n in enumerate(ORDER):
            f.write("#define MTEX_%s %d\n" % (n.upper(), i))
        f.write("static const cc_uint8 modern_texData[MODERN_TEX_COUNT][MODERN_TEX_SIZE * MODERN_TEX_SIZE * 4] = {\n")
        for n in ORDER:
            data = textures[n].tobytes()
            f.write("\t{ /* %s */\n" % n)
            for row in range(S):
                chunk = data[row * S * 4:(row + 1) * S * 4]
                f.write("\t\t" + ",".join(str(b) for b in chunk) + ",\n")
            f.write("\t},\n")
        f.write("};\n")

def write_preview(path):
    cols = 8; rows = (len(ORDER) + cols - 1) // cols
    im = Image.new("RGBA", (cols * S, rows * S), (0, 0, 0, 0))
    for i, n in enumerate(ORDER):
        im.paste(textures[n], ((i % cols) * S, (i // cols) * S))
    im.resize((cols * S * 8, rows * S * 8), Image.NEAREST).save(path)

if __name__ == "__main__":
    write_header(sys.argv[1])
    if len(sys.argv) > 2: write_preview(sys.argv[2])
