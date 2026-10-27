#!/usr/bin/env python3
"""Gera as texturas 16x16 originais do mod Minerios & Monstros (src/Minerios.c).
Tudo e desenhado por este script; nenhuma textura de terceiros e usada.
Os minerios sao camadas com transparencia: o jogo desenha a pedra do pacote
de texturas por baixo, entao combinam com qualquer pacote.
Uso: python3 gen_textures.py <saida .h> [previa .png]"""
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

# ---------------------------------------------------------------- minerios
def gem(px, cx, cy, light, mid, dark):
    """Pedrinha lapidada em losango 3x3 com brilho"""
    for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
        px[cx + dx, cy + dy] = mid
    px[cx + 1, cy] = dark; px[cx, cy + 1] = dark
    px[cx, cy] = light
    px[cx - 1, cy] = mid; px[cx, cy - 1] = shade(light, 1.1)

def big_gem(px, cx, cy, light, mid, dark):
    """Pedra lapidada em losango 5x5"""
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            d = abs(dx) + abs(dy)
            if d > 2: continue
            if d == 2: px[cx + dx, cy + dy] = dark if dx + dy >= 0 else mid
            else: px[cx + dx, cy + dy] = mid if dx + dy >= 0 else light
    px[cx - 1, cy - 1] = (255, 230, 235, 255)

def ore_ruby():
    im = new(); px = im.load()
    light, mid, dark = (255, 110, 130), (210, 20, 50), (115, 5, 28)
    for cx, cy in ((4, 4), (11, 8), (5, 12)):
        big_gem(px, cx, cy, light, mid, dark)
    for cx, cy in ((12, 2), (13, 13)):
        gem(px, cx, cy, light, mid, dark)
    return im

def chunk(px, pts, light, mid, dark):
    for i, (x, y) in enumerate(pts):
        px[x, y] = mid
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    for x, y in pts:
        if (x, y + 1) not in pts or (x + 1, y) not in pts: px[x, y] = dark
    px[min(xs), min(ys)] = light

def ore_titanium():
    im = new(); px = im.load()
    light, mid, dark = (235, 245, 255), (165, 185, 205), (85, 100, 120)
    def rect(x0, y0, w, h, skip=()):
        return [(x, y) for y in range(y0, y0 + h) for x in range(x0, x0 + w) if (x - x0, y - y0) not in skip]
    shapes = [
        rect(2, 2, 4, 3, skip=((3, 0),)),
        rect(10, 2, 3, 3, skip=((0, 2),)),
        rect(6, 7, 4, 4, skip=((0, 0), (3, 3))),
        rect(12, 11, 3, 3),
        rect(2, 12, 3, 2),
    ]
    for s in shapes: chunk(px, s, light, mid, dark)
    return im

def blob(px, cx, cy, r, core, glow, edge):
    for y in range(S):
        for x in range(S):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if d <= r * 0.45: px[x, y] = core
            elif d <= r * 0.8: px[x, y] = glow
            elif d <= r: px[x, y] = edge

def ore_uranium():
    im = new(); px = im.load()
    core, glow, edge = (230, 255, 150), (110, 240, 40), (40, 140, 20)
    for cx, cy, r in ((4, 4, 2.2), (11, 5, 1.8), (7, 10, 2.4), (13, 12, 1.5), (2, 13, 1.4)):
        blob(px, cx, cy, r, core, glow, edge)
    return im

def egg(px, cx, cy, shell, spot, shadow):
    # ovo 4x5: mais fino em cima
    rows = [(cx, cx + 1), (cx - 1, cx + 2), (cx - 1, cx + 2), (cx - 1, cx + 2), (cx, cx + 1)]
    for j, (x0, x1) in enumerate(rows):
        for x in range(x0, x1 + 1):
            px[x, cy + j] = shell
        px[x1, cy + j] = shadow
    px[cx, cy + 1] = shade(shell, 1.08)
    px[cx + 1, cy + 2] = spot; px[cx - 1, cy + 3] = spot

def ore_egg():
    im = new(); px = im.load()
    egg(px, 4, 2, (245, 235, 205), (150, 60, 190), (190, 175, 145))
    egg(px, 11, 8, (240, 230, 200), (60, 160, 80), (185, 170, 140))
    egg(px, 4, 10, (250, 225, 210), (210, 90, 40), (195, 165, 150))
    return im

# ----------------------------------------------------------------- blocos
def block_ruby():
    im = new(); px = im.load()
    base = (190, 20, 50)
    for y in range(S):
        for x in range(S):
            # losangos lapidados
            u, v = (x + y) % 8, (x - y) % 8
            f = 1.0
            if u == 0 or v == 0: f = 0.62
            elif u < 3 and v < 4: f = 1.32
            elif u > 5: f = 0.82
            px[x, y] = shade(base, f)
    for x, y in ((3, 5), (11, 13), (11, 5), (3, 13)): px[x, y] = (255, 200, 210, 255)
    for i in range(S):
        px[i, 0] = shade(base, 1.4); px[0, i] = shade(base, 1.4)
        px[i, S - 1] = shade(base, 0.5); px[S - 1, i] = shade(base, 0.5)
    return im

def block_titanium():
    base = (150, 165, 185)
    im = noise(base, 0.03, 7); px = im.load()
    for y in range(S):
        for x in range(S):
            lx, ly = x % 8, y % 8
            if lx == 0 or ly == 0: px[x, y] = shade(base, 1.25)
            elif lx == 7 or ly == 7: px[x, y] = shade(base, 0.62)
            elif (lx + ly) % 5 == 0: px[x, y] = shade(base, 1.07)
    for ox in (0, 8):
        for oy in (0, 8):
            for rx, ry in ((2, 2), (5, 2), (2, 5), (5, 5)):
                px[ox + rx, oy + ry] = (225, 235, 245, 255)
                px[ox + rx + 1, oy + ry + 1] = shade(base, 0.7)
    return im

def block_uranium():
    im = noise((25, 55, 25), 0.15, 3); px = im.load()
    core, glow = (220, 255, 140), (100, 235, 40)
    for y in range(S):
        for x in range(S):
            if x % 5 == 2 or y % 5 == 2: px[x, y] = shade(glow, 0.55)
    for cx in (2, 7, 12):
        for cy in (2, 7, 12):
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if 0 <= cx + dx < S and 0 <= cy + dy < S:
                        px[cx + dx, cy + dy] = glow
            px[cx, cy] = core
    for i in range(S):
        px[i, 0] = (60, 110, 45, 255); px[0, i] = (60, 110, 45, 255)
        px[i, S - 1] = (12, 30, 12, 255); px[S - 1, i] = (12, 30, 12, 255)
    return im

TEXTURES = [
    ("ORE_RUBY",       ore_ruby),
    ("ORE_TITANIUM",   ore_titanium),
    ("ORE_URANIUM",    ore_uranium),
    ("ORE_EGG",        ore_egg),
    ("BLOCK_RUBY",     block_ruby),
    ("BLOCK_TITANIUM", block_titanium),
    ("BLOCK_URANIUM",  block_uranium),
]

def main():
    out = sys.argv[1]
    imgs = [(n, f()) for n, f in TEXTURES]
    with open(out, "w", newline="\r\n") as fp:
        fp.write("/* Gerado por misc/minerios/gen_textures.py - nao editar */\n")
        fp.write("#define MINERIOS_TEX_COUNT %d\n" % len(imgs))
        fp.write("#define MINERIOS_TEX_SIZE  %d\n" % S)
        for i, (n, _) in enumerate(imgs):
            fp.write("#define MTX_%s %d\n" % (n, i))
        fp.write("static const cc_uint8 minerios_texData[MINERIOS_TEX_COUNT][MINERIOS_TEX_SIZE * MINERIOS_TEX_SIZE * 4] = {\n")
        for n, im in imgs:
            data = im.tobytes()
            fp.write("\t/* %s */ {\n" % n)
            for r in range(S):
                row = data[r * S * 4:(r + 1) * S * 4]
                fp.write("\t\t" + ",".join(str(b) for b in row) + ",\n")
            fp.write("\t},\n")
        fp.write("};\n")
    if len(sys.argv) > 2:
        stone = noise((125, 125, 125), 0.12, 1)
        prev = Image.new("RGBA", (S * len(imgs), S))
        for i, (n, im) in enumerate(imgs):
            tile = Image.alpha_composite(stone, im) if n.startswith("ORE") else im
            prev.paste(tile, (i * S, 0))
        prev.resize((S * len(imgs) * 8, S * 8), Image.NEAREST).save(sys.argv[2])

if __name__ == "__main__":
    main()
