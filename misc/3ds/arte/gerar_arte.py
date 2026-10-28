#!/usr/bin/env python3
"""Gera a arte original do ClassiCube 3DS (sem nada da Mojang/Minecraft nem de Pokemon):

  misc/3ds/icon.png              icone 48x48 do menu HOME (e do .3dsx)
  misc/3ds/banner.png            banner 256x128 que aparece na tela de cima do menu HOME
  misc/3ds/arte/topo_cena.png    paisagem da tela de cima do launcher (meia resolucao, 200x120)
  misc/3ds/arte/topo_logo.png    logo "ClassiCube" com legenda (meia resolucao, transparente)
  src/LauncherArt.h              os dois PNGs acima embutidos no executavel

Depois rode atualizar_bins.py para gravar o icone/banner em icon.bin e banner.bin (usados no .cia).
Uso: python3 misc/3ds/arte/gerar_arte.py
"""
import io, os, random
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.normpath(os.path.join(AQUI, "..", "..", ".."))

# ---------------------------------------------------------------- paleta
SKY_TOP, SKY_LOW = (78, 146, 226), (176, 220, 250)
GRASS  = [(98, 172, 54), (84, 154, 44), (118, 190, 66), (72, 138, 38)]
DIRT   = [(134, 96, 66), (118, 82, 55), (150, 110, 76), (104, 72, 48)]
STONE  = [(124, 124, 128), (108, 108, 112), (140, 140, 144), (96, 96, 100)]
LEAF   = [(58, 138, 52), (46, 116, 42), (78, 158, 64), (38, 100, 36)]
TRUNK  = [(112, 80, 50), (94, 66, 40), (128, 94, 60), (84, 58, 36)]
ORE    = {"ouro": (236, 196, 64), "cristal": (90, 214, 220), "rubi": (214, 64, 72)}
OUTLINE = (40, 26, 20)

def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))

def shade(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c)

# ---------------------------------------------------------------- fonte 5x7 (desenhada a mao)
FONTE = {
 'A': ["01110","10001","10001","11111","10001","10001","10001"],
 'B': ["11110","10001","10001","11110","10001","10001","11110"],
 'C': ["01110","10001","10000","10000","10000","10001","01110"],
 'D': ["11110","10001","10001","10001","10001","10001","11110"],
 'E': ["11111","10000","10000","11110","10000","10000","11111"],
 'I': ["111","010","010","010","010","010","111"],
 'M': ["10001","11011","10101","10101","10001","10001","10001"],
 'N': ["10001","11001","10101","10011","10001","10001","10001"],
 'O': ["01110","10001","10001","10001","10001","10001","01110"],
 'R': ["11110","10001","10001","11110","10100","10010","10001"],
 'S': ["01111","10000","10000","01110","00001","00001","11110"],
 'T': ["11111","00100","00100","00100","00100","00100","00100"],
 'U': ["10001","10001","10001","10001","10001","10001","01110"],
 'V': ["10001","10001","10001","10001","10001","01010","00100"],
 'Z': ["11111","00001","00010","00100","01000","10000","11111"],
 '3': ["11110","00001","00001","01110","00001","00001","11110"],
 'a': ["0000","0000","0110","0001","0111","1001","0111"],
 'b': ["1000","1000","1110","1001","1001","1001","1110"],
 'e': ["0000","0000","0110","1001","1111","1000","0111"],
 'i': ["1","0","1","1","1","1","1"],
 'l': ["11","01","01","01","01","01","01"],
 's': ["0000","0000","0111","1000","0110","0001","1110"],
 'u': ["0000","0000","1001","1001","1001","1001","0111"],
 ' ': ["00","00","00","00","00","00","00"],
 '.': ["0","0","0","0","0","0","1"],
 '+': ["000","000","010","111","010","000","000"],
 '-': ["000","000","000","111","000","000","000"],
}
# letras pequenas 3x5 para legendas
MINI = {
 'A': ["010","101","111","101","101"], 'B': ["110","101","110","101","110"],
 'C': ["011","100","100","100","011"], 'D': ["110","101","101","101","110"],
 'E': ["111","100","110","100","111"], 'G': ["011","100","101","101","011"],
 'I': ["111","010","010","010","111"], 'M': ["10001","11011","10101","10001","10001"],
 'N': ["1001","1101","1011","1001","1001"], 'O': ["010","101","101","101","010"],
 'R': ["110","101","110","101","101"], 'S': ["011","100","010","001","110"],
 'T': ["111","010","010","010","010"], 'U': ["101","101","101","101","111"],
 'V': ["101","101","101","101","010"], 'Z': ["111","001","010","100","111"],
 'P': ["110","101","110","100","100"], 'L': ["100","100","100","100","111"],
 'X': ["101","101","010","101","101"], 'F': ["111","100","110","100","100"],
 'H': ["101","101","111","101","101"], 'J': ["001","001","001","101","010"],
 'K': ["101","110","100","110","101"], 'Q': ["010","101","101","110","011"],
 'W': ["10001","10001","10101","11011","10001"], 'Y': ["101","101","010","010","010"],
 '3': ["110","001","010","001","110"], ' ': ["0","0","0","0","0"],
 '*': ["000","010","111","010","000"], '&': ["010","101","010","101","011"],
 'Ã': ["0101","1010","0110","1001","1111","1001","1001"],
 'Ç': ["011","100","100","100","011","010","110"],
 'É': ["001","010","111","100","110","100","111"],
}
ACIMA = {'Ã': 2, 'É': 2, 'Ç': 0}

def text_width(txt, font, scale, gap=1):
    return sum((len(font[ch][0]) + gap) * scale for ch in txt) - gap * scale

# ---------------------------------------------------------------- cena da tela de cima
def ruido(seed):
    rnd = random.Random(seed)
    return lambda: rnd.random()

def textura(rnd, pal, w=8, h=8):
    """bloco 8x8 com ruido de pixel"""
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            r = rnd()
            row.append(pal[0] if r < .45 else pal[1] if r < .7 else pal[2] if r < .9 else pal[3])
        out.append(row)
    return out

def cena(w, h, seed=7, bloco=8, arvores=True):
    """paisagem de blocos: ceu, sol, nuvens, morros ao fundo, terreno com arvores e minerios"""
    img = Image.new("RGB", (w, h))
    px = img.load()
    rnd = ruido(seed)
    # ceu em faixas (dither simples)
    for y in range(h):
        t = min(1.0, y / (h * 0.62))
        for x in range(w):
            tt = t + ((x + y) % 2) * 0.012
            px[x, y] = lerp(SKY_TOP, SKY_LOW, min(1, tt))
    # sol quadrado com brilho
    sx, sy, ss = int(w * 0.84), int(h * 0.10), max(6, w // 18)
    for y in range(sy - ss, sy + 2 * ss):
        for x in range(sx - ss, sx + 2 * ss):
            if 0 <= x < w and 0 <= y < h:
                d = max(abs(x - (sx + ss / 2)), abs(y - (sy + ss / 2)))
                if d < ss / 2:
                    px[x, y] = (255, 246, 196) if d < ss / 2 - 1 else (255, 226, 130)
                elif d < ss:
                    px[x, y] = lerp(px[x, y], (255, 244, 200), 0.35 * (1 - (d - ss / 2) / (ss / 2)))
    # nuvens achatadas
    for (cx, cy, cw) in [(0.10, 0.10, 0.18), (0.42, 0.05, 0.14), (0.62, 0.22, 0.20), (0.20, 0.30, 0.12)]:
        x0, y0, ww = int(cx * w), int(cy * h), int(cw * w)
        for (dx, dy, dw, dh) in [(0, 2, ww, 3), (ww // 5, 0, ww // 2, 3), (ww // 3, 4, ww // 2, 1)]:
            for y in range(y0 + dy, y0 + dy + dh):
                for x in range(x0 + dx, x0 + dx + dw):
                    if 0 <= x < w and 0 <= y < h:
                        px[x, y] = (250, 252, 255) if y < y0 + dy + dh - 1 else (220, 232, 246)
    # morros ao fundo (azulados) em degraus
    base_far = int(h * 0.58)
    alt = 0
    for x in range(w):
        if x % 6 == 0:
            alt = int(8 * (1 + __import__("math").sin(x / 23.0 + 1.3)) + 5 * (1 + __import__("math").sin(x / 9.0)))
        for y in range(base_far - alt, h):
            px[x, y] = (106, 160, 150) if y > base_far - alt else (128, 182, 166)
    # terreno em colunas de blocos
    cols = (w + bloco - 1) // bloco
    import math
    alturas = []
    for c in range(cols):
        v = 3.2 + 1.3 * math.sin(c * 0.55 + 0.4) + 0.8 * math.sin(c * 1.37)
        alturas.append(max(2, int(round(v))))
    rows = (h + bloco - 1) // bloco
    texr = ruido(seed + 1)
    for c in range(cols):
        top = rows - alturas[c]
        for r in range(top, rows):
            if r == top:
                tex = textura(texr, GRASS)
            elif r <= top + 1:
                tex = textura(texr, DIRT)
                # franja de grama
                for xx in range(8):
                    if texr() < 0.5: tex[0][xx] = GRASS[1]
            else:
                tex = textura(texr, STONE if r > top + 2 else DIRT)
                if r > top + 2 and texr() < 0.35:
                    cor = list(ORE.values())[int(texr() * 3) % 3]
                    for _ in range(4):
                        ox, oy = 1 + int(texr() * 5), 1 + int(texr() * 5)
                        tex[oy][ox] = cor; tex[oy][ox + 1] = shade(cor, 0.8)
            for yy in range(bloco):
                for xx in range(bloco):
                    X, Y = c * bloco + xx, r * bloco + yy
                    if X < w and Y < h:
                        cor = tex[yy * 8 // bloco][xx * 8 // bloco]
                        if r == top and yy == 0: cor = shade(cor, 1.12)
                        px[X, Y] = cor
    # arvores
    if arvores:
        for c in [2, cols - 6, cols // 2 + 3]:
            if c < 0 or c >= cols: continue
            top = rows - alturas[c]
            tx = c * bloco
            tronco_h = 2
            for k in range(1, tronco_h + 1):
                tex = textura(texr, TRUNK)
                for yy in range(bloco):
                    for xx in range(bloco):
                        X, Y = tx + xx, (top - k) * bloco + yy
                        if 0 <= X < w and 0 <= Y < h:
                            cor = tex[yy * 8 // bloco][xx * 8 // bloco]
                            if xx * 8 // bloco in (2, 5): cor = shade(cor, 0.85)
                            px[X, Y] = cor
            for (dc, dr) in [(-1, 3), (0, 3), (1, 3), (-1, 4), (0, 4), (1, 4), (0, 5)]:
                tex = textura(texr, LEAF)
                for yy in range(bloco):
                    for xx in range(bloco):
                        X, Y = tx + dc * bloco + xx, (top - dr) * bloco + yy
                        if 0 <= X < w and 0 <= Y < h:
                            px[X, Y] = tex[yy * 8 // bloco][xx * 8 // bloco]
    return img, alturas

# ---------------------------------------------------------------- logo em blocos
def desenha_logo(txt, escala, legenda=None):
    """letras feitas de cubinhos com contorno escuro; devolve RGBA"""
    gap = 1
    lw = text_width(txt, FONTE, escala, gap)
    lh = 7 * escala
    pad = 3
    W = lw + pad * 2 + 2
    leg_h = 0
    if legenda:
        leg_w = text_width(legenda, MINI, 1)
        W = max(W, leg_w + 12)
        leg_h = 5 + 11
    H = lh + pad * 2 + 2 + leg_h
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    ox = (W - lw) // 2
    oy = pad
    cheio = set()
    x = ox
    for ch in txt:
        g = FONTE[ch]
        for gy, row in enumerate(g):
            for gx, b in enumerate(row):
                if b == "1":
                    for yy in range(escala):
                        for xx in range(escala):
                            cheio.add((x + gx * escala + xx, oy + gy * escala + yy, gx, gy, xx, yy))
        x += (len(g[0]) + gap) * escala
    pts = {(p[0], p[1]) for p in cheio}
    # sombra projetada
    for (X, Y) in pts:
        for d in (1, 2):
            if (X + d, Y + d) not in pts and 0 <= X + d < W and 0 <= Y + d < H:
                px[X + d, Y + d] = (20, 12, 10, 150)
    # contorno
    for (X, Y) in pts:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                q = (X + dx, Y + dy)
                if q not in pts and 0 <= q[0] < W and 0 <= q[1] < H:
                    px[q] = OUTLINE + (255,)
    # face das letras: degrade dourado de cima pra baixo + bisel de cubinho
    topo, baixo = (255, 250, 214), (246, 170, 52)
    for (X, Y, gx, gy, xx, yy) in cheio:
        t = (Y - oy) / max(1, lh - 1)
        c = lerp(topo, baixo, t)
        if xx == 0 or yy == 0: c = lerp(c, (255, 255, 255), 0.45)
        elif xx == escala - 1 or yy == escala - 1: c = shade(c, 0.78)
        px[X, Y] = c + (255,)
    if legenda:
        lw2 = text_width(legenda, MINI, 1)
        bx0, by0 = (W - lw2) // 2 - 4, lh + pad * 2 + 3
        for y in range(by0 - 1, by0 + 11):
            for x in range(bx0, bx0 + lw2 + 8):
                borda = y in (by0 - 1, by0 + 10) or x in (bx0, bx0 + lw2 + 7)
                px[x, y] = OUTLINE + (255,) if borda else (54, 98, 40, 235)
        x = bx0 + 4
        for ch in legenda:
            g = MINI[ch]
            for gy, row in enumerate(g):
                for gx, b in enumerate(row):
                    if b == "1":
                        px[x + gx, by0 + 3 + gy - ACIMA.get(ch, 0)] = (255, 255, 255, 255)
            x += len(g[0]) + 1
    return img

def cola(base, logo, x, y):
    base.paste(logo, (x, y), logo)

# ---------------------------------------------------------------- icone 48x48
def icone():
    img = Image.new("RGB", (48, 48))
    px = img.load()
    for y in range(48):
        for x in range(48):
            px[x, y] = lerp(SKY_TOP, SKY_LOW, y / 47)
    # sol
    for y in range(3, 11):
        for x in range(36, 44):
            px[x, y] = (255, 246, 196) if 3 < y < 10 and 36 < x < 43 else (255, 226, 130)
    # cubo isometrico (grama em cima, terra nas laterais)
    rnd = ruido(11)
    cx, top_y, s = 24, 8, 16   # meia largura 16 -> cubo de 32px
    def face_top(x, y):  # dentro do losango?
        return abs(x - cx + 0.5) / s + abs(y - (top_y + s / 2) + 0.5) / (s / 2) <= 1.0
    grama = {}
    for y in range(top_y, top_y + s):
        for x in range(cx - s, cx + s):
            if face_top(x, y):
                u = ((x - cx) // 2 + (y - top_y)) // 2
                v = ((y - top_y) - (x - cx) // 2) // 2
                k = (u * 7 + v * 13) % 10
                c = GRASS[0] if k < 5 else GRASS[2] if k < 8 else GRASS[1]
                px[x, y] = shade(c, 1.08)
                grama[(x, y)] = True
    altura = 18
    for x in range(cx - s, cx + s):
        # borda inferior do losango nessa coluna
        ys = [y for y in range(top_y, top_y + s) if (x, y) in grama]
        if not ys: continue
        y0 = max(ys) + 1
        esquerda = x < cx
        for k in range(altura):
            y = y0 + k
            if y >= 48: break
            tex_v = k // 3
            tex_u = (x - cx) // 3
            r = ((tex_u * 31 + tex_v * 17) % 11) / 11.0
            if k < 3 + ((x * 5) % 3):
                c = GRASS[1] if r < .6 else GRASS[3]
            else:
                c = DIRT[0] if r < .45 else DIRT[1] if r < .75 else DIRT[2] if r < .92 else DIRT[3]
            c = shade(c, 0.95 if esquerda else 0.72)
            px[x, y] = c
    # contorno escuro do cubo
    alvo = {(x, y) for x in range(48) for y in range(48) if px[x, y] not in [] and (x, y) in grama}
    out = img.copy(); po = out.load()
    cubo = set()
    for x in range(48):
        for y in range(48):
            c = px[x, y]
            sky = lerp(SKY_TOP, SKY_LOW, y / 47)
            if c != sky and not (36 <= x < 44 and 3 <= y < 11):
                cubo.add((x, y))
    for (x, y) in cubo:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            q = (x + dx, y + dy)
            if 0 <= q[0] < 48 and 0 <= q[1] < 48 and q not in cubo and not (36 <= q[0] < 44 and 3 <= q[1] < 11):
                po[q] = OUTLINE
    # aresta clara entre topo e laterais
    for x in range(cx - s, cx + s):
        ys = [y for y in range(top_y, top_y + s) if (x, y) in grama]
        if ys:
            po[x, max(ys)] = shade(GRASS[2], 1.2)
    # aresta vertical do meio
    for y in range(top_y + s, min(48, top_y + s + altura)):
        po[cx, y] = shade(po[cx, y], 0.8) if isinstance(po[cx, y], tuple) else po[cx, y]
    # carinha do mascote "Cubinho" na face esquerda (olhos grandes e bochechas)
    def y_face(x):
        ys = [y for y in range(top_y, top_y + s) if (x, y) in grama]
        return max(ys) + 1
    def pinta(u, v, cor):
        x = cx - s + u
        po[x, y_face(x) + v] = cor
    for ou in (3, 10):
        for du in range(4):
            for dv in range(5):
                canto = du in (0, 3) and dv in (0, 4)
                if not canto: pinta(ou + du, 5 + dv, (250, 250, 250))
        for du in (1, 2):
            for dv in (2, 3, 4):
                pinta(ou + du, 5 + dv, (24, 20, 28))
        pinta(ou + 1, 6, (255, 255, 255))
        pinta(ou + 1, 7, (24, 20, 28))
        pinta(ou + 2, 6, (24, 20, 28))
    for u in (2, 3, 13, 14):
        pinta(u, 11, (232, 120, 120))
    for u in (7, 8):
        pinta(u, 12, (60, 30, 24))
    pinta(6, 11, (60, 30, 24)); pinta(9, 11, (60, 30, 24))
    return out

# ---------------------------------------------------------------- banner 256x128
def banner():
    base, _ = cena(128, 64, seed=21, bloco=8)
    big = base.resize((256, 128), Image.NEAREST).convert("RGBA")
    logo = desenha_logo("ClassiCube", 3, "3DS * MODS")
    cola(big, logo, (256 - logo.width) // 2, 14)
    return big

# ---------------------------------------------------------------- C header
def png_bytes(img):
    b = io.BytesIO(); img.save(b, "PNG", optimize=True); return b.getvalue()

def c_array(nome, data):
    linhas = []
    for i in range(0, len(data), 20):
        linhas.append("\t" + ", ".join("0x%02X" % v for v in data[i:i + 20]) + ",")
    return "static const cc_uint8 %s[%d] = {\n%s\n};\n" % (nome, len(data), "\n".join(linhas))

def main():
    cena_img, _ = cena(200, 120, seed=7, bloco=8)
    logo_img = desenha_logo("ClassiCube", 3, "EDIÇÃO 3DS COM MODS")
    cena_img.save(os.path.join(AQUI, "topo_cena.png"))
    logo_img.save(os.path.join(AQUI, "topo_logo.png"))

    icone().save(os.path.join(RAIZ, "misc", "3ds", "icon.png"))
    banner().save(os.path.join(RAIZ, "misc", "3ds", "banner.png"))

    h = ["/* Gerado por misc/3ds/arte/gerar_arte.py - nao editar a mao */",
         "/* Arte original da tela de cima do launcher (meia resolucao, ampliada 2x ao desenhar) */",
         "#ifndef CC_LAUNCHERART_H", "#define CC_LAUNCHERART_H",
         c_array("launcherArt_cena", png_bytes(cena_img)),
         c_array("launcherArt_logo", png_bytes(logo_img)),
         "#endif", ""]
    with open(os.path.join(RAIZ, "src", "LauncherArt.h"), "w", newline="\r\n") as f:
        f.write("\n".join(h))
    print("cena", cena_img.size, "logo", logo_img.size)

if __name__ == "__main__":
    main()
