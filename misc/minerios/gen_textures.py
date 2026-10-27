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

def speckle(im, colors, count, seed):
    rnd = random.Random(seed); px = im.load()
    for _ in range(count):
        px[rnd.randrange(S), rnd.randrange(S)] = rnd.choice(colors)
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


def lucky_block():
    """Bloco da sorte: dourado, com um ponto de interrogacao vermelho e brilhos"""
    base = (235, 185, 40)
    im = noise(base, 0.05, 31); px = im.load()
    for i in range(S):
        px[i, 0] = shade(base, 1.35); px[0, i] = shade(base, 1.35)
        px[i, 1] = shade(base, 1.15); px[1, i] = shade(base, 1.15)
        px[i, S - 1] = shade(base, 0.5); px[S - 1, i] = shade(base, 0.5)
        px[i, S - 2] = shade(base, 0.72); px[S - 2, i] = shade(base, 0.72)
    q = ["  ####  ",
         " ##  ## ",
         " ##  ## ",
         "     ## ",
         "    ##  ",
         "   ##   ",
         "   ##   ",
         "        ",
         "   ##   ",
         "   ##   "]
    red, dark = (200, 30, 40, 255), (110, 15, 20, 255)
    for y, row in enumerate(q):
        for x, c in enumerate(row):
            if c != "#": continue
            px[4 + x, 3 + y] = red
            below = q[y + 1][x + 1] if y + 1 < len(q) and x + 1 < len(row) else " "
            if below != "#": px[5 + x, 4 + y] = dark
    for x, y in ((3, 3), (12, 11), (3, 12)): px[x, y] = (255, 250, 210, 255)
    return im


# ------------------------------------------------------------------ itens
def outline(im, color):
    """Contorno escuro em volta dos pixels opacos (para itens ficarem legiveis)"""
    px = im.load(); edge = []
    for y in range(S):
        for x in range(S):
            if px[x, y][3]: continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < S and 0 <= ny < S and px[nx, ny][3] and px[nx, ny] != color:
                    edge.append((x, y)); break
    for x, y in edge: px[x, y] = color
    return im

def sword(blade_light, blade_dark, guard, grip, length=9, width=1, gem=None):
    """Espada na diagonal: cabo embaixo a esquerda, ponta em cima a direita"""
    im = new(); px = im.load()
    # lamina: faixa diagonal solida (x + y perto de 15), do cabo ate a ponta
    for y in range(S):
        for x in range(S):
            p, t = x + y, x - y
            if abs(p - 15) <= width and -5 <= t <= -5 + 2 * length - 2:
                px[x, y] = blade_light if p < 15 else (blade_dark if p > 15 else shade(blade_light, 0.85))
    # ponta
    tx = 5 + length - 1
    if tx + 1 < S: px[tx + 1, 15 - tx - 2] = blade_light
    # guarda
    for d in range(-2, 3):
        x, y = 4 + d, 11 + d
        if 0 <= x < S and 0 <= y < S: px[x, y] = guard
    # cabo
    for i in range(3):
        px[3 - i, 12 + i] = grip
    px[0, 15] = guard
    if gem: px[4, 11] = gem
    return outline(im, (25, 25, 30, 255))

def item_sword_titanium():
    return sword((225, 235, 250, 255), (140, 160, 185, 255), (90, 100, 120, 255), (95, 60, 35, 255))

def item_sword_supreme():
    im = sword((255, 240, 150, 255), (220, 170, 40, 255), (200, 30, 60, 255), (60, 30, 90, 255), length=10, gem=(120, 255, 90, 255))
    px = im.load()
    for x, y in ((9, 5), (12, 2)): px[x, y] = (255, 255, 230, 255)
    return im

def item_big_bertha():
    """Espadona enorme: lamina larga que ocupa o tile todo"""
    im = new(); px = im.load()
    light, mid, dark, rune = (200, 205, 215, 255), (150, 155, 170, 255), (85, 90, 105, 255), (230, 40, 40, 255)
    for y in range(S):
        for x in range(S):
            p, t = x + y, x - y
            if abs(p - 15) <= 2 and -7 <= t <= 14 - abs(p - 15):
                px[x, y] = light if p < 15 else (mid if p == 15 else dark)
    for i in (2, 5, 8):
        px[6 + i, 9 - i] = rune
    for d in range(-3, 4):
        x, y = 3 + d, 12 + d
        if 0 <= x < S and 0 <= y < S: px[x, y] = (70, 50, 30, 255)
    for i in range(3):
        px[2 - i, 13 + i] = (40, 40, 45, 255)
    px[3, 12] = rune
    return outline(im, (20, 20, 25, 255))

def item_armor_titanium():
    im = new(); px = im.load()
    light, mid, dark = (220, 232, 248, 255), (155, 175, 200, 255), (90, 105, 130, 255)
    rows = {2: (2, 5, 10, 13), 3: (1, 6, 9, 14), 4: (1, 14), 5: (1, 14), 6: (3, 12), 7: (3, 12),
            8: (3, 12), 9: (3, 12), 10: (3, 12), 11: (3, 12), 12: (3, 12), 13: (4, 11)}
    for y, r in rows.items():
        segs = [(r[i], r[i + 1]) for i in range(0, len(r), 2)]
        for a, b in segs:
            for x in range(a, b + 1):
                px[x, y] = light if x < 7 else (mid if x < 10 else dark)
    for y in range(4, 13): px[7, y] = dark
    for x, y in ((5, 6), (10, 6), (5, 10), (10, 10)): px[x, y] = (240, 250, 255, 255)
    return outline(im, (30, 35, 45, 255))

# ---------------------------------------------------------------- dimensoes
def swirl(c1, c2, c3, seed):
    im = new(); px = im.load(); rnd = random.Random(seed)
    import math
    for y in range(S):
        for x in range(S):
            dx, dy = x - 7.5, y - 7.5
            a = math.atan2(dy, dx); d = (dx * dx + dy * dy) ** 0.5
            v = (math.sin(a * 3 + d * 0.9) + 1) / 2
            c = c1 if v < 0.33 else (c2 if v < 0.66 else c3)
            px[x, y] = shade(c, 0.9 + rnd.random() * 0.2)
    for i in range(S):
        px[i, 0] = px[0, i] = px[i, S - 1] = px[S - 1, i] = shade(c1, 0.45)
    return im

def portal_utopia():  return swirl((40, 160, 70), (120, 230, 90), (250, 220, 90), 21)
def portal_crystal(): return swirl((90, 40, 150), (190, 90, 230), (250, 170, 240), 22)

def crystal_stone():
    im = noise((175, 150, 215), 0.06, 31); px = im.load()
    for y in range(S):
        for x in range(S):
            if (x + 2 * y) % 9 == 0: px[x, y] = shade((175, 150, 215), 1.25)
            elif (2 * x - y) % 11 == 0: px[x, y] = shade((175, 150, 215), 0.78)
    return im

def crystal_grass_top():
    im = noise((110, 225, 220), 0.12, 32)
    return speckle(im, [(250, 170, 235, 255), (200, 255, 250, 255)], 14, 33)

def crystal_grass_side():
    im = crystal_stone(); px = im.load(); rnd = random.Random(34)
    top = crystal_grass_top().load()
    for x in range(S):
        h = 3 + rnd.randrange(3)
        for y in range(h): px[x, y] = top[x, y]
    return im

def crystal_log():
    im = noise((230, 230, 245), 0.05, 35); px = im.load()
    for x in range(S):
        for y in range(S):
            if x % 4 == 0: px[x, y] = (175, 185, 225, 255)
            elif (x + y) % 7 == 0: px[x, y] = (250, 210, 245, 255)
    return im

def crystal_leaves():
    im = new(); px = im.load(); rnd = random.Random(36)
    for y in range(S):
        for x in range(S):
            if rnd.random() < 0.18: continue
            px[x, y] = rnd.choice([(255, 150, 220, 255), (240, 120, 200, 255), (255, 200, 240, 255), (200, 110, 220, 255)])
    return im

def crystal_cluster():
    im = new(); px = im.load()
    def spike(x0, h, c):
        for y in range(h):
            yy = 15 - y
            w = 1 if y < h - 2 else 0
            for dx in range(-w, w + 1):
                px[x0 + dx, yy] = shade(c, 1.2 if dx < 0 else (1.0 if dx == 0 else 0.75))
    spike(4, 9, (170, 120, 255)); spike(8, 13, (120, 220, 255)); spike(12, 8, (255, 140, 230))
    return outline(im, (60, 30, 90, 255))

# ------------------------------------------------------------ mosquito (modelo)
# pecas: nome, x1,y1,z1, x2,y2,z2, u,v, anim, cor
MOSQ_PARTES = [
    ("abdomen", -1, 6, 1,   1, 8, 8,   0, 0,  0, (70, 55, 45)),
    ("torax",   -2, 6, -2,  2, 9, 1,   20, 0, 0, (55, 45, 40)),
    ("cabeca",  -1, 7, -4,  1, 9, -2,  0, 12, 0, (40, 35, 35)),
    ("ferrao",   0, 7, -9,  1, 8, -4,  14, 12, 0, (25, 20, 20)),
    ("asa_e",    2, 9, -2,  9, 10, 2,  0, 20, 1, (200, 215, 235)),
    ("asa_d",   -9, 9, -2, -2, 10, 2,  24, 20, 2, (200, 215, 235)),
    ("perna1", 2, 1, -2, 3, 6, -1, 40, 0, 3, (35, 30, 30)),
    ("perna2", 2, 1, 0, 3, 6, 1, 44, 0, 3, (35, 30, 30)),
    ("perna3", 1, 1, 3, 2, 6, 4, 48, 0, 3, (35, 30, 30)),
    ("perna4", -3, 1, -2, -2, 6, -1, 52, 0, 4, (35, 30, 30)),
    ("perna5", -3, 1, 0, -2, 6, 1, 56, 0, 4, (35, 30, 30)),
    ("perna6", -2, 1, 3, -1, 6, 4, 60, 0, 4, (35, 30, 30)),
]

def mosquito_png():
    W, H = 64, 32
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load()
    for n, x1, y1, z1, x2, y2, z2, u, v, anim, cor in MOSQ_PARTES:
        sx, sy, sz = x2 - x1, y2 - y1, z2 - z1
        w, h = 2 * (sx + sz), sz + sy
        rnd = random.Random(sum(map(ord, n)))
        for yy in range(v, min(H, v + h)):
            for xx in range(u, min(W, u + w)):
                c = shade(cor, 0.85 + rnd.random() * 0.3)
                if n == "abdomen" and (xx - u) % 3 == 0: c = shade((235, 235, 235), 1.0)  # listras
                if n.startswith("asa"):
                    c = (cor[0], cor[1], cor[2], 255) if (xx + yy) % 3 else (150, 165, 190, 255)
                px[xx, yy] = c
    # olhos vermelhos na frente da cabeca (face -Z fica em u+sz .. u+sz+sx, v+sz .. )
    for xx, yy in ((2, 14), (3, 14)):
        px[xx, yy] = (220, 30, 30, 255)
    import io
    buf = io.BytesIO(); im.save(buf, "PNG"); return buf.getvalue(), im

TEXTURES = [
    ("ORE_RUBY",       ore_ruby),
    ("ORE_TITANIUM",   ore_titanium),
    ("ORE_URANIUM",    ore_uranium),
    ("ORE_EGG",        ore_egg),
    ("BLOCK_RUBY",     block_ruby),
    ("BLOCK_TITANIUM", block_titanium),
    ("BLOCK_URANIUM",  block_uranium),
    ("SWORD_TITANIUM", item_sword_titanium),
    ("SWORD_SUPREME",  item_sword_supreme),
    ("BIG_BERTHA",     item_big_bertha),
    ("ARMOR_TITANIUM", item_armor_titanium),
    ("PORTAL_UTOPIA",  portal_utopia),
    ("PORTAL_CRYSTAL", portal_crystal),
    ("CRYSTAL_STONE",  crystal_stone),
    ("CRYSTAL_GRASS_TOP",  crystal_grass_top),
    ("CRYSTAL_GRASS_SIDE", crystal_grass_side),
    ("CRYSTAL_LOG",    crystal_log),
    ("CRYSTAL_LEAVES", crystal_leaves),
    ("CRYSTAL_CLUSTER", crystal_cluster),
    ("LUCKY_BLOCK",    lucky_block),
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
        fp.write("/* 1 = partes transparentes mostram a pedra do pacote (minerios) */\n")
        fp.write("static const cc_uint8 minerios_texSobrePedra[MINERIOS_TEX_COUNT] = { %s };\n"
                 % ", ".join("1" if n.startswith("ORE") else "0" for n, _ in imgs))

        png, _ = mosquito_png()
        fp.write("\n/* Modelo do mosquito: x1,y1,z1, x2,y2,z2 (1/16 de bloco), u,v na textura, anim (1/2 = asas) */\n")
        fp.write("#define MOSQUITO_PARTES %d\n" % len(MOSQ_PARTES))
        fp.write("static const cc_int8 mosquito_partes[MOSQUITO_PARTES][9] = {\n")
        for n, x1, y1, z1, x2, y2, z2, u, v, anim, cor in MOSQ_PARTES:
            fp.write("\t{ %d, %d, %d, %d, %d, %d, %d, %d, %d }, /* %s */\n" % (x1, y1, z1, x2, y2, z2, u, v, anim, n))
        fp.write("};\n")
        fp.write("static const cc_uint8 mosquito_png[%d] = {\n" % len(png))
        for i in range(0, len(png), 24):
            fp.write("\t" + ",".join(str(b) for b in png[i:i + 24]) + ",\n")
        fp.write("};\n")
    if len(sys.argv) > 2:
        stone = noise((125, 125, 125), 0.12, 1)
        prev = Image.new("RGBA", (S * len(imgs), S))
        for i, (n, im) in enumerate(imgs):
            tile = Image.alpha_composite(stone, im) if n.startswith("ORE") else im
            prev.paste(tile, (i * S, 0))
        prev.resize((S * len(imgs) * 8, S * 8), Image.NEAREST).save(sys.argv[2])
        mosquito_png()[1].resize((64 * 8, 32 * 8), Image.NEAREST).save(sys.argv[2].replace(".png", "_mosquito.png"))

if __name__ == "__main__":
    main()
