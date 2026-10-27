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

# --------------------------------------------------- chefoes e dragao
def trofeu_ferrao():
    """Ferrao da Rainha dos Mosquitos: agulha roxa com base escura"""
    im = new(); px = im.load()
    for i in range(12):
        x, y = 2 + i, 13 - i
        px[x, y] = (200, 120, 255, 255) if i > 3 else (90, 40, 120, 255)
        if i < 9: px[x + 1, y] = (130, 60, 180, 255)
    for x, y in ((2, 11), (3, 12), (1, 13), (4, 14), (2, 14), (1, 12)):
        px[x, y] = (60, 25, 80, 255)
    px[14, 1] = (255, 230, 255, 255)
    return outline(im, (30, 10, 40, 255))

def trofeu_nucleo():
    """Nucleo de titanio: moldura metalica com um brilho ciano no meio"""
    im = new(); px = im.load()
    for y in range(3, 13):
        for x in range(3, 13):
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d > 4: px[x, y] = (150, 165, 185, 255) if (x + y) % 2 else (110, 120, 140, 255)
            elif d > 2.5: px[x, y] = (40, 120, 150, 255)
            else: px[x, y] = (150, 255, 255, 255) if d < 1.5 else (60, 200, 230, 255)
    for x, y in ((3, 3), (12, 3), (3, 12), (12, 12)): px[x, y] = (230, 240, 250, 255)
    return outline(im, (30, 35, 45, 255))

def trofeu_olho():
    """Olho de cristal: gema violeta com pupila clara"""
    im = new(); px = im.load()
    for y in range(S):
        for x in range(S):
            dx, dy = (x - 7.5) / 6.5, (y - 7.5) / 4.5
            r = dx * dx + dy * dy
            if r > 1: continue
            c = (150, 70, 220) if r > 0.55 else (200, 140, 255)
            if r < 0.18: c = (240, 250, 255)
            if r < 0.05: c = (40, 10, 60)
            px[x, y] = c + (255,)
    px[5, 5] = (255, 255, 255, 255)
    return outline(im, (40, 15, 70, 255))

def trofeu_coracao():
    """Coracao de uranio: coracao verde que brilha"""
    im = new(); px = im.load()
    shape = ["  ##   ##  ",
             " #### #### ",
             "###########",
             "###########",
             " ######### ",
             "  #######  ",
             "   #####   ",
             "    ###    ",
             "     #     "]
    for y, row in enumerate(shape):
        for x, c in enumerate(row):
            if c != "#": continue
            g = (90, 230, 50) if (x + y) % 3 else (170, 255, 110)
            if y < 3 and x in (2, 3, 7): g = (220, 255, 190)
            px[3 + x, 4 + y] = g + (255,)
    return outline(im, (15, 50, 10, 255))

def altar(cor, simbolo, seed):
    """Altar de chefao: pedra escura com uma runa colorida"""
    base = (70, 65, 75)
    im = noise(base, 0.12, seed); px = im.load()
    for i in range(S):
        px[i, 0] = px[0, i] = shade(base, 1.4)
        px[i, S - 1] = px[S - 1, i] = shade(base, 0.5)
        px[i, 3] = px[i, 12] = shade(base, 0.7)
    for y, row in enumerate(simbolo):
        for x, c in enumerate(row):
            if c == "#": px[4 + x, 5 + y] = cor + (255,)
    return im

def altar_rainha(): return altar((200, 120, 255), ["  #  #  ", "   ##   ", "########", "   ##   ", "  #  #  ", "        "], 41)
def altar_golem():  return altar((90, 220, 240),  ["########", "#  ##  #", "#      #", "# #### #", "#      #", "########"], 42)
def altar_aranha(): return altar((235, 170, 255), ["#  ##  #", " # ## # ", "  ####  ", " # ## # ", "#  ##  #", "        "], 43)
def altar_creeper(): return altar((110, 240, 60), ["##    ##", "##    ##", "   ##   ", "  ####  ", "  #  #  ", "        "], 44)

def item_devastadora():
    """Espada Devastadora: lamina enorme dourada e vermelha, com runas de todos os chefoes"""
    im = new(); px = im.load()
    for y in range(S):
        for x in range(S):
            p, t = x + y, x - y
            if abs(p - 15) <= 3 and -8 <= t <= 15 - abs(p - 15):
                c = (255, 215, 70) if p < 14 else ((255, 150, 40) if p <= 16 else (190, 50, 30))
                px[x, y] = c + (255,)
    for i, c in zip((2, 4, 6, 8), ((200, 120, 255), (90, 220, 240), (235, 170, 255), (110, 240, 60))):
        px[6 + i, 9 - i] = c + (255,)
    for d in range(-3, 4):
        x, y = 3 + d, 12 + d
        if 0 <= x < S and 0 <= y < S: px[x, y] = (40, 20, 50, 255)
    for i in range(3): px[2 - i, 13 + i] = (120, 40, 30, 255)
    return outline(im, (40, 10, 10, 255))

def ovo_dragao():
    """Ovo de dragao: ovo grande com escamas verdes e laranja"""
    im = new(); px = im.load()
    for y in range(1, 16):
        for x in range(S):
            dx, dy = (x - 7.5) / 5.5, (y - 9) / 6.8
            if dy < 0: dx *= 1.25
            if dx * dx + dy * dy > 1: continue
            c = (40, 140, 110) if (x + 2 * (y // 2)) % 4 else (25, 95, 80)
            if (y % 4 == 1) and x % 3 == 0: c = (240, 150, 40)
            if dx < -0.4 and dy < -0.1: c = shade(c, 1.35)[:3]
            px[x, y] = c + (255,)
    return outline(im, (10, 40, 30, 255))

# ------------------------------------------- skins proprias dos monstros
# Caixas de cada modelo padrao do ClassiCube (u, v, tamanho x, y, z, girada, papel).
# So a posicao das caixas vem do jogo; o desenho e todo feito aqui.
MODELOS = {
    "humano": (64, 64, [(0, 0, 8, 8, 8, 0, "cabeca"), (16, 16, 8, 12, 4, 0, "corpo"),
                        (40, 16, 4, 12, 4, 0, "braco"), (32, 48, 4, 12, 4, 0, "braco"),
                        (0, 16, 4, 12, 4, 0, "perna"), (16, 48, 4, 12, 4, 0, "perna")]),
    "galinha": (64, 32, [(0, 0, 4, 6, 3, 0, "cabeca"), (14, 0, 4, 2, 2, 0, "bico"), (14, 4, 2, 2, 2, 0, "barbela"),
                         (0, 9, 6, 6, 7, 1, "corpo"), (24, 13, 1, 4, 6, 0, "asa"), (32, 0, 6, 8, 0, 0, "pe")]),
    "porco": (64, 32, [(0, 0, 8, 8, 8, 0, "cabeca"), (28, 8, 10, 8, 16, 1, "corpo"), (0, 16, 4, 6, 4, 0, "perna")]),
    "carneiro": (64, 32, [(0, 0, 6, 6, 8, 0, "cabeca"), (28, 8, 8, 6, 16, 1, "corpo"), (0, 16, 4, 12, 4, 0, "perna")]),
    "creeper": (64, 32, [(0, 0, 8, 8, 8, 0, "cabeca"), (16, 16, 8, 12, 4, 0, "corpo"), (0, 16, 4, 6, 4, 0, "perna")]),
    "aranha": (64, 32, [(32, 4, 8, 8, 8, 0, "cabeca"), (0, 0, 6, 6, 6, 0, "corpo"), (0, 12, 10, 8, 12, 0, "corpo"),
                        (18, 0, 16, 2, 2, 0, "perna")]),
}

def caixa_rect(u, v, sx, sy, sz, girada):
    if girada: return u, v, 2 * (sy + sx), sy + sz
    return u, v, 2 * (sz + sx), sz + sy

def frente(u, v, sx, sy, sz):
    return u + sz, v + sz, sx, sy

def pintar_skin(modelo, cores, rosto, padrao, seed):
    W, H, caixas = MODELOS[modelo]
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load(); rnd = random.Random(seed)
    for (u, v, sx, sy, sz, girada, papel) in caixas:
        x0, y0, w, h = (u, v, 6, 8) if papel == "pe" else caixa_rect(u, v, sx, sy, sz, girada)
        base = cores.get(papel, cores["corpo"])
        for y in range(y0, min(H, y0 + h)):
            for x in range(x0, min(W, x0 + w)):
                c = shade(base, 0.86 + rnd.random() * 0.24)
                c = padrao(papel, x - x0, y - y0, c, rnd) or c
                px[x, y] = c
        if papel == "cabeca":
            fx, fy, fw, fh = frente(u, v, sx, sy, sz)
            rosto(px, fx, fy, fw, fh)
    return im

def png_bytes(im):
    import io
    buf = io.BytesIO(); im.save(buf, "PNG"); return buf.getvalue()

def olhos(cor, linha=0.35, brilho=None, boca=None, n=2):
    def f(px, x, y, w, h):
        ey = y + int(h * linha)
        if n == 2:
            xs = (x + max(1, w // 4) - 1, x + w - max(1, w // 4))
        else:
            xs = tuple(x + 1 + i * (w - 2) // (n - 1) for i in range(n))
        for ex in xs:
            px[ex, ey] = cor + (255,)
            if w >= 8: px[ex, ey + 1] = shade(cor, 0.7)
            if brilho: px[ex, ey - 1 if w >= 8 else ey] = brilho + (255,) if w >= 8 else px[ex, ey]
        if boca:
            by = y + int(h * 0.7)
            for bx in range(x + w // 3, x + w - w // 3): px[bx, by] = boca + (255,)
    return f

def listras(cor, cada=3, papeis=("corpo",)):
    def f(papel, x, y, c, rnd):
        if papel in papeis and (y % cada == 0): return shade(cor, 0.9 + rnd.random() * 0.2)
    return f

def manchas(cor, chance, papeis=("corpo", "cabeca", "perna", "braco")):
    def f(papel, x, y, c, rnd):
        if papel in papeis and ((x // 2) * 7 + (y // 2) * 13) % 11 < chance: return shade(cor, 0.9 + rnd.random() * 0.2)
    return f

def placas(cor):
    def f(papel, x, y, c, rnd):
        if x % 4 == 0 or y % 4 == 0: return shade(cor, 0.8)
        if x % 4 == 1 and y % 4 == 1: return (230, 240, 250, 255)
    return f

def facetas(cor):
    def f(papel, x, y, c, rnd):
        if (x + y) % 5 == 0: return shade(cor, 1.3)
        if (x - y) % 7 == 0: return shade(cor, 0.7)
    return f

def veias(cor):
    def f(papel, x, y, c, rnd):
        if (x * 3 + y * 5) % 9 == 0 or (x + 2 * y) % 13 == 0: return cor + (255,)
    return f

def coroa(cor):
    base = veias((150, 255, 90))
    def f(papel, x, y, c, rnd):
        if papel == "cabeca" and 8 <= y <= 10: return cor + (255,) if (x % 4) else (230, 40, 60, 255)
        if papel == "cabeca" and y < 8 and x >= 8 and x < 16: return shade(cor, 0.9)
        return base(papel, x, y, c, rnd)
    return f

def nada(papel, x, y, c, rnd): return None

SKINS = [
    ("GALINHA",        "galinha",  {"corpo": (190, 150, 230), "cabeca": (200, 165, 235), "bico": (250, 160, 40), "barbela": (60, 140, 230), "asa": (160, 120, 210), "pe": (250, 160, 40)},
                       olhos((20, 20, 30), 0.3), manchas((240, 220, 255), 2), 1),
    ("LEITAO",         "porco",    {"corpo": (240, 205, 160), "cabeca": (240, 205, 160), "perna": (200, 150, 110)},
                       olhos((30, 20, 20), 0.3, boca=(150, 80, 60)), manchas((140, 85, 50), 3), 2),
    ("CARNEIRO",       "carneiro", {"corpo": (170, 210, 240), "cabeca": (60, 60, 80), "perna": (60, 60, 80)},
                       olhos((240, 240, 255), 0.35), manchas((215, 235, 255), 4, ("corpo",)), 3),
    ("ARANHA",         "aranha",   {"corpo": (35, 45, 80), "cabeca": (30, 35, 60), "perna": (25, 30, 50)},
                       olhos((255, 150, 40), 0.3, n=4), manchas((240, 130, 40), 2, ("corpo",)), 4),
    ("ARANHA_GIGANTE", "aranha",   {"corpo": (150, 25, 30), "cabeca": (60, 15, 20), "perna": (40, 10, 15)},
                       olhos((255, 230, 60), 0.3, n=4), listras((20, 10, 10), 3), 5),
    ("ZUMBI_TITAN",    "humano",   {"corpo": (95, 110, 90), "cabeca": (105, 120, 95), "braco": (90, 105, 85), "perna": (70, 80, 70)},
                       olhos((255, 140, 30), 0.4, boca=(40, 30, 25)), manchas((60, 95, 50), 3), 6),
    ("CREEPER_COLOSSO", "creeper", {"corpo": (95, 110, 140), "cabeca": (105, 120, 150), "perna": (80, 90, 115)},
                       olhos((80, 230, 255), 0.35, boca=(30, 40, 60)), manchas((50, 60, 85), 4), 7),
    ("GALINHA_MONSTRO", "galinha", {"corpo": (35, 30, 35), "cabeca": (45, 35, 40), "bico": (90, 90, 95), "barbela": (220, 20, 40), "asa": (60, 20, 25), "pe": (90, 90, 95)},
                       olhos((255, 230, 40), 0.3), manchas((190, 30, 40), 2), 8),
]

# ------------------------------------------------------- dragao manso
# x1,y1,z1, x2,y2,z2 (1/16 de bloco), u,v, anim, pivo x,y,z (1/32 de bloco), cor
# anim: 1/2 = asas batendo, 5/6 = pernas andando, 7 = rabo balancando
DRAGAO_PARTES = [
    ("corpo",   -4, 6, -7,   4, 13, 7,   0, 0,   0, 0, 0, 0,       (40, 140, 110)),
    ("pescoco", -2, 10, -13, 2, 14, -7,  26, 32, 0, 0, 0, 0,       (40, 140, 110)),
    ("cabeca",  -3, 11, -20, 3, 16, -13, 0, 32,  0, 0, 0, 0,       (45, 150, 115)),
    ("focinho", -2, 11, -24, 2, 14, -20, 48, 21, 0, 0, 0, 0,       (45, 150, 115)),
    ("chifre_e", 2, 16, -15, 3, 19, -14, 56, 0,  0, 0, 0, 0,       (240, 225, 190)),
    ("chifre_d",-3, 16, -15,-2, 19, -14, 56, 0,  0, 0, 0, 0,       (240, 225, 190)),
    ("rabo1",   -2, 7, 7,    2, 11, 15,  0, 44,  7, 0, 18, 14,     (40, 140, 110)),
    ("rabo2",   -1, 8, 15,   1, 11, 23,  24, 44, 7, 0, 18, 14,     (35, 125, 100)),
    ("asa_e",    4, 12, -5,  18, 13, 5,  0, 21,  1, 8, 25, 0,      (230, 110, 50)),
    ("asa_d",  -18, 12, -5, -4, 13, 5,   0, 21,  2, -8, 25, 0,     (230, 110, 50)),
    ("perna1",   1, 0, -6,   4, 6, -3,   44, 0,  5, 5, 12, -9,     (35, 120, 95)),
    ("perna2",  -4, 0, -6,  -1, 6, -3,   44, 0,  6, -5, 12, -9,    (35, 120, 95)),
    ("perna3",   1, 0, 3,    4, 6, 6,    44, 0,  6, 5, 12, 9,      (35, 120, 95)),
    ("perna4",  -4, 0, 3,   -1, 6, 6,    44, 0,  5, -5, 12, 9,     (35, 120, 95)),
]

def dragao_png():
    W, H = 64, 64
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load()
    for parte in DRAGAO_PARTES:
        n, x1, y1, z1, x2, y2, z2, u, v, anim = parte[:10]; cor = parte[13]
        sx, sy, sz = x2 - x1, y2 - y1, z2 - z1
        w, h = 2 * (sx + sz), sz + sy
        rnd = random.Random(sum(map(ord, n)))
        for yy in range(v, min(H, v + h)):
            for xx in range(u, min(W, u + w)):
                c = shade(cor, 0.85 + rnd.random() * 0.3)
                if n.startswith("asa"):
                    # membrana com ossos
                    c = shade(cor, 0.95 + rnd.random() * 0.1) if (xx - u) % 5 else (120, 60, 30, 255)
                elif (xx + (yy // 2) * 2) % 4 == 0:
                    c = shade(cor, 0.7)  # escamas
                px[xx, yy] = c
        if n == "corpo":
            # barriga creme (face de baixo: u+sz+sx .. u+sz+2sx, v .. v+sz)
            for yy in range(v, v + sz):
                for xx in range(u + sz + sx, u + sz + 2 * sx):
                    px[xx, yy] = shade((235, 215, 160), 0.9 + rnd.random() * 0.15)
            # espinhos dourados no topo
            for yy in range(v, v + sz, 2):
                px[u + sz + sx // 2, yy] = (250, 200, 60, 255)
        if n == "cabeca":
            # olhos amarelos dos dois lados e na frente
            fx, fy = u + sz, v + sz
            px[fx + 1, fy + 1] = px[fx + sx - 2, fy + 1] = (255, 220, 40, 255)
            px[u + 2, v + sz + 1] = px[u + sz + sx + 4, v + sz + 1] = (255, 220, 40, 255)
        if n == "focinho":
            fx, fy = u + sz, v + sz
            px[fx, fy] = px[fx + sx - 1, fy] = (20, 20, 20, 255)
    return png_bytes(im), im

# ------------------------------------------ modelos dos chefoes e dragoes
# (nome, x1,y1,z1, x2,y2,z2 em 1/16 de bloco, anim, pivo x,y,z em 1/32, cor, padrao, grupo)
# A frente do modelo e o lado -Z. As caixas de mesmo grupo dividem o mesmo pedaco da textura.
# anim: 1/2 asas, 3/4 pernas abertas, 5/6 pernas andando, 7/14 rabo (fases opostas),
#       8/9 bracos balancando, 10/11 pernas de aranha
OURO, ROXO, PRETO = (250, 200, 50), (120, 50, 170), (30, 20, 35)

def perna_lado(lado, z, grupo="perna"):
    x1, x2 = (5, 19) if lado > 0 else (-19, -5)
    return ("perna", x1, 8, z, x2, 10, z + 2, 10 if lado > 0 else 11, 10 * lado, 18, 2 * z + 2, (190, 130, 240), "cristal", grupo)

MODELOS_PROPRIOS = {
    "rainha": [
        ("abdomen",  -4, 8, 2,    4, 15, 16,   0, 0, 0, 0,     (120, 50, 170), "listras", "abdomen"),
        ("torax",    -3, 8, -4,   3, 14, 2,    0, 0, 0, 0,     (70, 30, 90),   "liso", "torax"),
        ("cabeca",   -2, 9, -8,   2, 13, -4,   0, 0, 0, 0,     (50, 20, 60),   "olhos_vermelhos", "cabeca"),
        ("coroa",    -2, 13, -7,  2, 15, -5,   0, 0, 0, 0,     OURO,           "coroa", "coroa"),
        ("ferrao",   -1, 10, -16, 1, 11, -8,   0, 0, 0, 0,     (230, 180, 255), "liso", "ferrao"),
        ("asa1_e",    3, 14, -3, 14, 15, 3,    1, 6, 28, 0,    (215, 190, 245), "asa", "asa1"),
        ("asa1_d",  -14, 14, -3, -3, 15, 3,    2, -6, 28, 0,   (215, 190, 245), "asa", "asa1"),
        ("asa2_e",    3, 13, 2,  11, 14, 7,    1, 6, 26, 8,    (190, 160, 235), "asa", "asa2"),
        ("asa2_d",  -11, 13, 2,  -3, 14, 7,    2, -6, 26, 8,   (190, 160, 235), "asa", "asa2"),
    ] + [("perna", x, 1, z, x + 1, 8, z + 1, 3 if x > 0 else 4, 2 * x + 1, 16, 2 * z + 1, PRETO, "liso", "perna")
         for x in (2, -3) for z in (-3, -1, 1)],
    "golem": [
        ("tronco",   -7, 14, -4,  7, 26, 4,    0, 0, 0, 0,     (150, 165, 185), "placas", "tronco"),
        ("cabeca",   -4, 24, -6,  4, 31, 1,    0, 0, 0, 0,     (130, 145, 165), "visor", "cabeca"),
        ("nucleo",   -2, 18, -5,  2, 22, -4,   0, 0, 0, 0,     (90, 240, 255),  "brilho", "nucleo"),
        ("ombro_e",   7, 25, -2, 11, 28, 2,    0, 0, 0, 0,     (110, 120, 140), "liso", "ombro"),
        ("ombro_d", -11, 25, -2, -7, 28, 2,    0, 0, 0, 0,     (110, 120, 140), "liso", "ombro"),
        ("braco_e",   7, 5, -2,  12, 25, 3,    8, 19, 50, 1,   (140, 155, 175), "placas", "braco"),
        ("braco_d", -12, 5, -2,  -7, 25, 3,    9, -19, 50, 1,  (140, 155, 175), "placas", "braco"),
        ("perna_e",   1, 0, -2,   6, 14, 3,    5, 7, 28, 1,    (120, 130, 150), "placas", "perna"),
        ("perna_d",  -6, 0, -2,  -1, 14, 3,    6, -7, 28, 1,   (120, 130, 150), "placas", "perna"),
    ],
    "aranha_cristal": [
        ("cabeca",   -5, 6, -12,  5, 12, -2,   0, 0, 0, 0,     (110, 50, 170),  "olhos_brancos", "cabeca"),
        ("abdomen",  -6, 5, -2,   6, 14, 12,   0, 0, 0, 0,     (150, 80, 210),  "cristal", "abdomen"),
        ("presa_e",   2, 3, -13,  3, 7, -12,   0, 0, 0, 0,     (240, 230, 255), "liso", "presa"),
        ("presa_d",  -3, 3, -13, -2, 7, -12,   0, 0, 0, 0,     (240, 230, 255), "liso", "presa"),
        ("cristal1", -1, 14, 1,   1, 22, 3,    0, 0, 0, 0,     (200, 250, 255), "cristal", "cristal1"),
        ("cristal2",  2, 14, 4,   4, 19, 6,    0, 0, 0, 0,     (230, 170, 255), "cristal", "cristal2"),
        ("cristal3", -4, 14, 5,  -2, 20, 7,    0, 0, 0, 0,     (170, 230, 255), "cristal", "cristal3"),
        ("cristal4",  0, 14, 8,   2, 18, 10,   0, 0, 0, 0,     (240, 200, 255), "cristal", "cristal2"),
    ] + [perna_lado(l, z) for l in (1, -1) for z in (-11, -8, -5, -2)],
    "rei_creeper": [
        ("corpo",    -5, 6, -4,   5, 22, 4,    0, 0, 0, 0,     (40, 75, 35),   "veias", "corpo"),
        ("cabeca",   -6, 22, -6,  6, 33, 6,    0, 0, 0, 0,     (45, 85, 40),   "rosto_creeper", "cabeca"),
        ("nucleo",   -2, 12, -5,  2, 16, -4,   0, 0, 0, 0,     (160, 255, 90), "brilho", "nucleo"),
        ("ombreira_e", 5, 17, -2, 8, 21, 2,    0, 0, 0, 0,     (120, 240, 70), "cristal", "ombreira"),
        ("ombreira_d",-8, 17, -2,-5, 21, 2,    0, 0, 0, 0,     (120, 240, 70), "cristal", "ombreira"),
    ] + [("coroa", x, 33, z, x + 2, 37 + (1 if (x + z) % 4 else 0), z + 2, 0, 0, 0, 0, OURO, "coroa", "coroa")
         for x in (-6, 4) for z in (-6, 4)] + [
        ("coroa_meio", -1, 33, -6, 1, 39, -4,  0, 0, 0, 0,     (230, 40, 60),  "brilho", "joia"),
    ] + [("perna", x, 0, z, x + 4, 6, z + 4, 5 if (x > 0) == (z > 0) else 6, 2 * x + 4, 12, 2 * z + 4, (30, 55, 28), "veias", "perna")
         for x in (-5, 1) for z in (-6, 2)],
    "dragao_fogo": [
        ("corpo",    -3, 8, -6,   3, 14, 6,    0, 0, 0, 0,     (190, 50, 30),   "escamas", "corpo"),
        ("pescoco",  -2, 11, -12, 2, 14, -6,   0, 0, 0, 0,     (190, 50, 30),   "escamas", "pescoco"),
        ("cabeca",   -3, 11, -18, 3, 15, -12,  0, 0, 0, 0,     (200, 60, 35),   "olhos_amarelos", "cabeca"),
        ("mandibula",-2, 9, -17,  2, 11, -13,  0, 0, 0, 0,     (240, 160, 60),  "liso", "mandibula"),
        ("chifre_e",  2, 15, -14, 3, 19, -13,  0, 0, 0, 0,     (40, 30, 30),    "liso", "chifre"),
        ("chifre_d", -3, 15, -14,-2, 19, -13,  0, 0, 0, 0,     (40, 30, 30),    "liso", "chifre"),
        ("asa_e",     3, 12, -6, 21, 13, 6,    1, 6, 25, 0,    (250, 140, 40),  "asa", "asa"),
        ("asa_d",   -21, 12, -6, -3, 13, 6,    2, -6, 25, 0,   (250, 140, 40),  "asa", "asa"),
        ("rabo1",    -2, 9, 6,    2, 12, 14,   7, 0, 21, 12,   (190, 50, 30),   "escamas", "rabo1"),
        ("rabo2",    -1, 9, 14,   1, 11, 22,   14, 0, 20, 28,  (170, 40, 25),   "escamas", "rabo2"),
        ("ponta",    -2, 8, 22,   2, 12, 25,   14, 0, 20, 28,  (40, 30, 30),    "liso", "ponta"),
        ("perna_e",   1, 0, -1,   3, 8, 2,     5, 4, 16, 1,    (150, 40, 25),   "escamas", "perna"),
        ("perna_d",  -3, 0, -1,  -1, 8, 2,     6, -4, 16, 1,   (150, 40, 25),   "escamas", "perna"),
    ],
    "dragao_gelo": [
        ("cabeca",   -3, 10, -14, 3, 15, -7,   0, 0, 0, 0,     (150, 210, 245), "olhos_azuis", "cabeca"),
        ("chifre_e",  2, 15, -10, 3, 16, -3,   0, 0, 0, 0,     (240, 250, 255), "liso", "chifre"),
        ("chifre_d", -3, 15, -10,-2, 16, -3,   0, 0, 0, 0,     (240, 250, 255), "liso", "chifre"),
        ("seg1",     -3, 9, -7,   3, 14, 1,    0, 0, 0, 0,     (110, 180, 230), "escamas", "seg1"),
        ("seg2",     -3, 9, 1,    3, 14, 9,    7, 0, 23, 2,    (110, 180, 230), "escamas", "seg1"),
        ("seg3",     -2, 9, 9,    2, 13, 17,   14, 0, 22, 18,  (100, 165, 220), "escamas", "seg3"),
        ("seg4",     -2, 9, 17,   2, 13, 25,   7, 0, 22, 34,   (100, 165, 220), "escamas", "seg3"),
        ("seg5",     -1, 10, 25,  1, 12, 33,   14, 0, 22, 50,  (90, 150, 210),  "escamas", "seg5"),
        ("barbatana",-1, 14, -4,  1, 17, 4,    0, 0, 0, 0,     (230, 245, 255), "cristal", "barbatana"),
        ("asa_e",     3, 13, -5, 12, 14, 2,    1, 6, 27, 0,    (210, 240, 255), "asa", "asa"),
        ("asa_d",   -12, 13, -5, -3, 14, 2,    2, -6, 27, 0,   (210, 240, 255), "asa", "asa"),
    ],
}

def empacotar(partes, W=64, H=64):
    """Distribui os pedacos de textura (skyline): devolve {grupo: (u, v)}"""
    grupos = {}
    for pt in partes:
        g = pt[13]
        if g in grupos: continue
        sx, sy, sz = pt[4] - pt[1], pt[5] - pt[2], pt[6] - pt[3]
        grupos[g] = (2 * (sx + sz), sz + sy)
    ordem = sorted(grupos, key=lambda g: (-grupos[g][1], -grupos[g][0]))
    sky = [0] * W; uv = {}
    for g in ordem:
        w, h = grupos[g]
        melhor = None
        for x in range(0, W - w + 1):
            y = max(sky[x:x + w])
            if y + h <= H and (melhor is None or y < melhor[1]): melhor = (x, y)
        if melhor is None: raise SystemExit("textura cheia: " + g)
        x, y = melhor; uv[g] = melhor
        for i in range(x, x + w): sky[i] = y + h
    return uv

def pintar_modelo(nome, partes):
    W = H = 64
    uv = empacotar(partes)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load()
    feitos = set()
    for pt in partes:
        n, x1, y1, z1, x2, y2, z2, anim, pvx, pvy, pvz, cor, padrao, g = pt
        if g in feitos: continue
        feitos.add(g)
        sx, sy, sz = x2 - x1, y2 - y1, z2 - z1
        u, v = uv[g]; w, h = 2 * (sx + sz), sz + sy
        rnd = random.Random(sum(map(ord, nome + g)))
        for yy in range(v, v + h):
            for xx in range(u, u + w):
                lx, ly = xx - u, yy - v
                c = shade(cor, 0.88 + rnd.random() * 0.2)
                if padrao == "listras" and (lx // 2) % 3 == 0: c = shade(PRETO, 1.0)
                if padrao == "placas" and (lx % 5 == 0 or ly % 5 == 0): c = shade(cor, 0.72)
                if padrao == "placas" and lx % 5 == 2 and ly % 5 == 2: c = (230, 240, 250, 255)
                if padrao == "escamas" and (lx + (ly // 2) * 2) % 4 == 0: c = shade(cor, 0.7)
                if padrao == "cristal" and (lx + ly) % 4 == 0: c = shade(cor, 1.3)
                if padrao == "cristal" and (lx - ly) % 5 == 0: c = shade(cor, 0.75)
                if padrao == "veias" and ((lx * 3 + ly * 5) % 9 == 0 or (lx + 2 * ly) % 13 == 0): c = (150, 255, 90, 255)
                if padrao == "asa": c = shade(cor, 0.95 + rnd.random() * 0.1) if lx % 4 else shade(cor, 0.55)
                if padrao == "brilho": c = shade(cor, 1.1 if (lx + ly) % 2 else 0.95)
                if padrao == "coroa" and ly % 3 == 0: c = (230, 40, 60, 255)
                px[xx, yy] = c
        # rostos (face da frente: u+sz .. u+sz+sx, v+sz .. v+sz+sy)
        fx, fy = u + sz, v + sz
        def olho(x, y, c):
            if 0 <= x < W and 0 <= y < H: px[x, y] = c + (255,)
        if padrao.startswith("olhos"):
            c = {"olhos_vermelhos": (255, 40, 40), "olhos_brancos": (250, 250, 255),
                 "olhos_amarelos": (255, 220, 40), "olhos_azuis": (40, 120, 255)}[padrao]
            ey = fy + max(1, sy // 3)
            olho(fx, ey, c); olho(fx + sx - 1, ey, c)
            if padrao == "olhos_brancos":  # aranha: 6 olhos
                for i in range(1, sx - 1, 2): olho(fx + i, ey - 1, c)
                for i in (2, sx - 3): olho(fx + i, ey + 1, c)
            if sx >= 6: olho(fx + 1, ey, c); olho(fx + sx - 2, ey, c)
        if padrao == "visor":
            for i in range(1, sx - 1): olho(fx + i, fy + 3, (60, 240, 255))
            for i in range(2, sx - 2): olho(fx + i, fy + 5, (40, 50, 70))
        if padrao == "rosto_creeper":
            for (ox, oy) in ((2, 3), (3, 3), (8, 3), (9, 3), (2, 4), (3, 4), (8, 4), (9, 4)):
                olho(fx + ox, fy + oy, (180, 255, 90))
            for ox in range(4, 8): olho(fx + ox, fy + 7, (20, 30, 15)); olho(fx + ox, fy + 8, (20, 30, 15))
            olho(fx + 3, fy + 9, (20, 30, 15)); olho(fx + 8, fy + 9, (20, 30, 15))
    saida = []
    for pt in partes:
        n, x1, y1, z1, x2, y2, z2, anim, pvx, pvy, pvz = pt[:11]
        u, v = uv[pt[13]]
        saida.append((n, x1, y1, z1, x2, y2, z2, u, v, anim, pvx, pvy, pvz))
    return saida, png_bytes(im), im

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
    ("TROFEU_FERRAO",  trofeu_ferrao),
    ("TROFEU_NUCLEO",  trofeu_nucleo),
    ("TROFEU_OLHO",    trofeu_olho),
    ("TROFEU_CORACAO", trofeu_coracao),
    ("ALTAR_RAINHA",   altar_rainha),
    ("ALTAR_GOLEM",    altar_golem),
    ("ALTAR_ARANHA",   altar_aranha),
    ("ALTAR_CREEPER",  altar_creeper),
    ("ESPADA_DEVASTADORA", item_devastadora),
    ("OVO_DRAGAO",     ovo_dragao),
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

        def modelo(nome, partes, png, piv):
            N = nome.upper()
            fp.write("\n#define %s_PARTES %d\n" % (N, len(partes)))
            fp.write("static const cc_int8 %s_partes[%s_PARTES][12] = {\n" % (nome, N))
            for parte in partes:
                n, x1, y1, z1, x2, y2, z2, u, v, anim = parte[:10]
                px_, py_, pz_ = piv(parte)
                fp.write("\t{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }, /* %s */\n"
                         % (x1, y1, z1, x2, y2, z2, u, v, anim, px_, py_, pz_, n))
            fp.write("};\n")
            fp.write("static const cc_uint8 %s_png[%d] = {\n" % (nome, len(png)))
            for i in range(0, len(png), 24):
                fp.write("\t" + ",".join(str(b) for b in png[i:i + 24]) + ",\n")
            fp.write("};\n")

        def piv_mosquito(p):
            n, x1, y1, z1, x2, y2, z2, u, v, anim = p[:10]
            if anim >= 3: return x1 + x2, 2 * y2, z1 + z2   # pernas: onde encostam no corpo
            if anim == 1: return 4, 18, 0                     # asas: onde encostam no torax
            if anim == 2: return -4, 18, 0
            return 0, 0, 0

        fp.write("\n/* Modelos proprios: x1,y1,z1, x2,y2,z2 (1/16 de bloco), u,v na textura, anim, pivo x,y,z (1/32 de bloco) */\n")
        modelo("mosquito", MOSQ_PARTES, mosquito_png()[0], piv_mosquito)
        modelo("dragao", DRAGAO_PARTES, dragao_png()[0], lambda p: p[10:13])
        for nome, partes in MODELOS_PROPRIOS.items():
            saida, png, _ = pintar_modelo(nome, partes)
            modelo(nome, [o + (0,) for o in saida], png, lambda p: p[10:13])
        fp.write("\n/* Caixa em volta de cada modelo (1/16 de bloco): x1,y1,z1, x2,y2,z2 */\n")
        for nome, partes in [("mosquito", MOSQ_PARTES), ("dragao", DRAGAO_PARTES)] + list(MODELOS_PROPRIOS.items()):
            xs1 = min(min(pt[1], pt[4]) for pt in partes); xs2 = max(max(pt[1], pt[4]) for pt in partes)
            ys1 = min(min(pt[2], pt[5]) for pt in partes); ys2 = max(max(pt[2], pt[5]) for pt in partes)
            zs1 = min(min(pt[3], pt[6]) for pt in partes); zs2 = max(max(pt[3], pt[6]) for pt in partes)
            fp.write("static const cc_int8 %s_caixa[6] = { %d, %d, %d, %d, %d, %d };\n" % (nome, xs1, ys1, zs1, xs2, ys2, zs2))

        fp.write("\n/* Skins proprias dos monstros (modelos padrao, desenho original) */\n")
        fp.write("#define MINERIOS_SKINS %d\n" % len(SKINS))
        for i, sk in enumerate(SKINS):
            fp.write("#define SKIN_%s %d\n" % (sk[0], i))
        for sk in SKINS:
            png = png_bytes(pintar_skin(sk[1], sk[2], sk[3], sk[4], sk[5]))
            fp.write("static const cc_uint8 skin_%s_png[%d] = {\n" % (sk[0].lower(), len(png)))
            for i in range(0, len(png), 24):
                fp.write("\t" + ",".join(str(b) for b in png[i:i + 24]) + ",\n")
            fp.write("};\n")
        fp.write("static const struct { const cc_uint8* data; int len; } minerios_skins[MINERIOS_SKINS] = {\n")
        for sk in SKINS:
            fp.write("\t{ skin_%s_png, sizeof(skin_%s_png) },\n" % (sk[0].lower(), sk[0].lower()))
        fp.write("};\n")
    if len(sys.argv) > 2:
        stone = noise((125, 125, 125), 0.12, 1)
        prev = Image.new("RGBA", (S * len(imgs), S))
        for i, (n, im) in enumerate(imgs):
            tile = Image.alpha_composite(stone, im) if n.startswith("ORE") else im
            prev.paste(tile, (i * S, 0))
        prev.resize((S * len(imgs) * 8, S * 8), Image.NEAREST).save(sys.argv[2])
        mosquito_png()[1].resize((64 * 8, 32 * 8), Image.NEAREST).save(sys.argv[2].replace(".png", "_mosquito.png"))
        dragao_png()[1].resize((64 * 6, 64 * 6), Image.NEAREST).save(sys.argv[2].replace(".png", "_dragao.png"))
        for nome, partes in MODELOS_PROPRIOS.items():
            pintar_modelo(nome, partes)[2].resize((64 * 6, 64 * 6), Image.NEAREST).save(sys.argv[2].replace(".png", "_m_%s.png" % nome))
        for sk in SKINS:
            im = pintar_skin(sk[1], sk[2], sk[3], sk[4], sk[5])
            im.resize((im.width * 6, im.height * 6), Image.NEAREST).save(sys.argv[2].replace(".png", "_skin_%s.png" % sk[0].lower()))

if __name__ == "__main__":
    main()
