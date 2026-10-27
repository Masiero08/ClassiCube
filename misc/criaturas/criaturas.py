#!/usr/bin/env python3
"""Desenha as criaturas do mod (modelos + texturas) e gera o codigo C.

Tudo aqui e original: formas de corpo, nomes e pinturas. Cada "plano de
corpo" e uma lista de caixas (como os modelos do ClassiCube). As especies
reaproveitam um plano e mudam a pintura e o tamanho.

Uso (na raiz do repositorio):
    python3 misc/criaturas/criaturas.py

Gera:
    misc/criaturas/texturas/<especie>.png   (64x64)
    src/CriaturasDados.h                    (caixas, especies e PNGs embutidos)
"""
import io
import os
import random

from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
TEX_DIR = os.path.join(AQUI, "texturas")
HEADER = os.path.join(RAIZ, "src", "CriaturasDados.h")
TEX_W = TEX_H = 64

# animacoes (iguais as do Criaturas.c)
FIXO, CABECA, PERNA_A, PERNA_B, CAUDA, ASA_E, ASA_D = range(7)

# ---------------------------------------------------------------- planos
# (nome, x1,y1,z1, x2,y2,z2, pivo, animacao). Unidade = 1/16 de bloco,
# y para cima, a criatura olha para -z.
PLANOS = {
    "raposa": [
        ("cabeca", -4, 6, -10, 4, 13, -4, (0, 8, -5), CABECA),
        ("focinho", -2, 6, -12, 2, 9, -10, (0, 8, -5), CABECA),
        ("orelha_e", -4, 13, -7, -1, 16, -6, (0, 8, -5), CABECA),
        ("orelha_d", 1, 13, -7, 4, 16, -6, (0, 8, -5), CABECA),
        ("corpo", -3, 3, -5, 3, 8, 4, (0, 0, 0), FIXO),
        ("perna", -3, 0, -5, -1, 3, -3, (-2, 3, -4), PERNA_A),
        ("perna", 1, 0, -5, 3, 3, -3, (2, 3, -4), PERNA_B),
        ("perna", -3, 0, 2, -1, 3, 4, (-2, 3, 3), PERNA_B),
        ("perna", 1, 0, 2, 3, 3, 4, (2, 3, 3), PERNA_A),
        ("cauda", -2, 4, 4, 2, 8, 11, (0, 6, 4), CAUDA),
    ],
    "porquinho": [
        ("cabeca", -4, 4, -11, 4, 12, -4, (0, 7, -5), CABECA),
        ("focinho", -2, 5, -12, 2, 8, -11, (0, 7, -5), CABECA),
        ("orelha_e", -4, 12, -7, -2, 14, -6, (0, 7, -5), CABECA),
        ("orelha_d", 2, 12, -7, 4, 14, -6, (0, 7, -5), CABECA),
        ("broto", -1, 12, -10, 1, 15, -8, (0, 7, -5), CABECA),
        ("corpo", -4, 2, -5, 4, 9, 6, (0, 0, 0), FIXO),
        ("perna", -4, 0, -5, -1, 2, -2, (-2, 2, -4), PERNA_A),
        ("perna", 1, 0, -5, 4, 2, -2, (2, 2, -4), PERNA_B),
        ("perna", -4, 0, 3, -1, 2, 6, (-2, 2, 4), PERNA_B),
        ("perna", 1, 0, 3, 4, 2, 6, (2, 2, 4), PERNA_A),
    ],
    "esquilo": [
        ("cabeca", -4, 8, -6, 4, 14, 0, (0, 9, -2), CABECA),
        ("orelha_e", -4, 14, -3, -2, 17, -2, (0, 9, -2), CABECA),
        ("orelha_d", 2, 14, -3, 4, 17, -2, (0, 9, -2), CABECA),
        ("corpo", -3, 2, -4, 3, 9, 3, (0, 0, 0), FIXO),
        ("braco", -4, 4, -5, -2, 7, -3, (-3, 7, -4), PERNA_A),
        ("braco", 2, 4, -5, 4, 7, -3, (3, 7, -4), PERNA_B),
        ("perna", -3, 0, -2, -1, 2, 2, (-2, 2, 0), PERNA_B),
        ("perna", 1, 0, -2, 3, 2, 2, (2, 2, 0), PERNA_A),
        ("cauda", -2, 3, 3, 2, 15, 6, (0, 4, 4), CAUDA),
    ],
    "ave": [
        ("cabeca", -3, 7, -5, 3, 13, 1, (0, 8, -1), CABECA),
        ("bico", -1, 8, -7, 1, 10, -5, (0, 8, -1), CABECA),
        ("crista", -1, 13, -3, 1, 16, 0, (0, 8, -1), CABECA),
        ("corpo", -3, 2, -3, 3, 8, 4, (0, 0, 0), FIXO),
        ("asa_e", -4, 3, -2, -3, 7, 3, (-3, 7, 0), ASA_E),
        ("asa_d", 3, 3, -2, 4, 7, 3, (3, 7, 0), ASA_D),
        ("perna", -2, 0, 0, -1, 2, 1, (-1, 2, 0), PERNA_A),
        ("perna", 1, 0, 0, 2, 2, 1, (1, 2, 0), PERNA_B),
        ("cauda", -2, 4, 4, 2, 5, 7, (0, 4, 4), CAUDA),
    ],
    "bolha": [
        ("corpo", -4, 0, -4, 4, 8, 4, (0, 0, 0), FIXO),
        ("topo", -1, 8, -1, 1, 10, 1, (0, 0, 0), FIXO),
        ("braco", -6, 2, -1, -4, 5, 1, (-5, 5, 0), PERNA_A),
        ("braco", 4, 2, -1, 6, 5, 1, (5, 5, 0), PERNA_B),
    ],
}
ORDEM_PLANOS = ["raposa", "porquinho", "esquilo", "ave", "bolha"]

# ---------------------------------------------------------------- tipos
TIPOS = ["FOGO", "AGUA", "PLANTA", "ELETRICO", "VENTO", "PEDRA"]
# habitat: onde nasce no mundo
HABITATS = ["CAMPO", "AREIA", "AGUA", "ALTO", "PEDRA"]

# ---------------------------------------------------------------- especies
# nome, cor do nome (codigo &), tipo, plano, escala, vida, dano, velocidade,
# evolui_para, nivel_evolucao, habitat, inicial, paleta
ESPECIES = [
    # linha de fogo: raposinhas
    dict(id="faiscol", nome="Faiscol", cor="&6", tipo="FOGO", plano="raposa", escala=0.85,
         vida=10, dano=3, vel=4.0, evolui="labareda", nivel=5, habitat="AREIA", inicial=1,
         pal=dict(base=(240, 120, 40), barriga=(255, 225, 170), detalhe=(255, 215, 60), olho=(40, 25, 20))),
    dict(id="labareda", nome="Labareda", cor="&6", tipo="FOGO", plano="raposa", escala=1.1,
         vida=16, dano=5, vel=4.5, evolui="fornalobo", nivel=10, habitat="AREIA", inicial=0,
         pal=dict(base=(220, 70, 35), barriga=(255, 210, 140), detalhe=(255, 190, 40), olho=(40, 20, 20), marcas=True)),
    dict(id="fornalobo", nome="Fornalobo", cor="&c", tipo="FOGO", plano="raposa", escala=1.45,
         vida=24, dano=8, vel=4.8, evolui=None, nivel=0, habitat="AREIA", inicial=0,
         pal=dict(base=(120, 35, 30), barriga=(70, 30, 30), detalhe=(255, 150, 30), olho=(255, 220, 80), lava=True)),
    # linha de agua: bolhas
    dict(id="gotinha", nome="Gotinha", cor="&b", tipo="AGUA", plano="bolha", escala=0.8,
         vida=11, dano=2, vel=3.0, evolui="marolha", nivel=5, habitat="AGUA", inicial=1,
         pal=dict(base=(110, 190, 250), barriga=(200, 235, 255), detalhe=(60, 130, 230), olho=(20, 30, 70))),
    dict(id="marolha", nome="Marolha", cor="&b", tipo="AGUA", plano="bolha", escala=1.1,
         vida=17, dano=4, vel=3.2, evolui="tsunamor", nivel=10, habitat="AGUA", inicial=0,
         pal=dict(base=(60, 140, 235), barriga=(180, 220, 255), detalhe=(240, 250, 255), olho=(20, 25, 60), ondas=True)),
    dict(id="tsunamor", nome="Tsunamor", cor="&9", tipo="AGUA", plano="bolha", escala=1.5,
         vida=26, dano=7, vel=3.2, evolui=None, nivel=0, habitat="AGUA", inicial=0,
         pal=dict(base=(35, 80, 190), barriga=(120, 180, 240), detalhe=(250, 250, 255), olho=(255, 255, 255), ondas=True, sobrancelha=True)),
    # linha de planta: porquinhos com broto
    dict(id="brotinho", nome="Brotinho", cor="&a", tipo="PLANTA", plano="porquinho", escala=0.8,
         vida=13, dano=2, vel=2.8, evolui="musgao", nivel=5, habitat="CAMPO", inicial=1,
         pal=dict(base=(150, 210, 110), barriga=(225, 245, 190), detalhe=(60, 160, 60), olho=(30, 40, 20))),
    dict(id="musgao", nome="Musgao", cor="&a", tipo="PLANTA", plano="porquinho", escala=1.1,
         vida=20, dano=4, vel=2.6, evolui="florestauro", nivel=10, habitat="CAMPO", inicial=0,
         pal=dict(base=(110, 85, 55), barriga=(180, 150, 110), detalhe=(80, 160, 60), olho=(25, 25, 20), musgo=True)),
    dict(id="florestauro", nome="Florestauro", cor="&2", tipo="PLANTA", plano="porquinho", escala=1.5,
         vida=30, dano=6, vel=2.4, evolui=None, nivel=0, habitat="CAMPO", inicial=0,
         pal=dict(base=(80, 60, 40), barriga=(140, 110, 80), detalhe=(50, 140, 50), olho=(250, 230, 120), musgo=True, flores=True)),
    # eletrico: esquilos
    dict(id="choquilo", nome="Choquilo", cor="&e", tipo="ELETRICO", plano="esquilo", escala=0.85,
         vida=9, dano=3, vel=5.0, evolui="trovesquilo", nivel=8, habitat="CAMPO", inicial=0,
         pal=dict(base=(250, 215, 70), barriga=(255, 245, 200), detalhe=(60, 60, 70), olho=(30, 30, 40), raio=True)),
    dict(id="trovesquilo", nome="Trovesquilo", cor="&e", tipo="ELETRICO", plano="esquilo", escala=1.2,
         vida=16, dano=6, vel=5.5, evolui=None, nivel=0, habitat="CAMPO", inicial=0,
         pal=dict(base=(60, 60, 75), barriga=(250, 220, 80), detalhe=(255, 230, 60), olho=(255, 240, 120), raio=True)),
    # vento: passarinhos
    dict(id="penape", nome="Penape", cor="&3", tipo="VENTO", plano="ave", escala=0.85,
         vida=8, dano=2, vel=5.0, evolui="ventania", nivel=7, habitat="ALTO", inicial=0,
         pal=dict(base=(120, 180, 240), barriga=(245, 245, 255), detalhe=(250, 180, 50), olho=(20, 20, 40))),
    dict(id="ventania", nome="Ventania", cor="&3", tipo="VENTO", plano="ave", escala=1.3,
         vida=15, dano=5, vel=5.5, evolui=None, nivel=0, habitat="ALTO", inicial=0,
         pal=dict(base=(50, 110, 200), barriga=(240, 240, 250), detalhe=(250, 170, 40), olho=(20, 20, 40), crista_longa=True)),
    # pedra: bolhas de pedra
    dict(id="seixo", nome="Seixo", cor="&7", tipo="PEDRA", plano="bolha", escala=0.85,
         vida=14, dano=3, vel=2.2, evolui="pedregulho", nivel=8, habitat="PEDRA", inicial=0,
         pal=dict(base=(150, 145, 140), barriga=(185, 180, 175), detalhe=(110, 105, 100), olho=(30, 30, 30), rocha=True)),
    dict(id="pedregulho", nome="Pedregulho", cor="&8", tipo="PEDRA", plano="bolha", escala=1.35,
         vida=26, dano=6, vel=2.0, evolui=None, nivel=0, habitat="PEDRA", inicial=0,
         pal=dict(base=(105, 100, 95), barriga=(140, 135, 130), detalhe=(90, 150, 70), olho=(255, 200, 60), rocha=True, sobrancelha=True)),
]


# ---------------------------------------------------------------- empacotar UVs
def tamanho(c):
    _, x1, y1, z1, x2, y2, z2, _, _ = c
    return x2 - x1, y2 - y1, z2 - z1


def empacotar(caixas):
    """Arruma a rede de cada caixa (largura 2*(sz+sx), altura sz+sy) na
    textura 64x64. Devolve a lista de (u, v)."""
    ordem = sorted(range(len(caixas)), key=lambda i: -(tamanho(caixas[i])[2] + tamanho(caixas[i])[1]))
    uvs = [None] * len(caixas)
    x = y = altura_linha = 0
    for i in ordem:
        sx, sy, sz = tamanho(caixas[i])
        w, h = 2 * (sz + sx), sz + sy
        if x + w > TEX_W:
            x, y, altura_linha = 0, y + altura_linha, 0
        if y + h > TEX_H:
            raise SystemExit("plano nao cabe em 64x64")
        uvs[i] = (x, y)
        x += w
        altura_linha = max(altura_linha, h)
    return uvs


UVS = {p: empacotar(PLANOS[p]) for p in PLANOS}


def regioes(u, v, sx, sy, sz):
    """Regioes de cada face na textura (mesma rede do BoxDesc_BuildBox)."""
    return {
        "topo": (u + sz, v, sx, sz),
        "baixo": (u + sz + sx, v, sx, sz),
        "esq": (u, v + sz, sz, sy),            # face x2
        "frente": (u + sz, v + sz, sx, sy),    # face z1 (-z)
        "dir": (u + sz + sx, v + sz, sz, sy),  # face x1
        "tras": (u + 2 * sz + sx, v + sz, sx, sy),
    }


# ---------------------------------------------------------------- pintura
def mix(a, b, t):
    return tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3))


def clar(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c)


def pintar(esp):
    pal = esp["pal"]
    rnd = random.Random(esp["id"])
    img = Image.new("RGBA", (TEX_W, TEX_H), (0, 0, 0, 0))
    caixas = PLANOS[esp["plano"]]
    base, barriga, det, olho = pal["base"], pal["barriga"], pal["detalhe"], pal["olho"]

    def ruido(c, amt=8):
        k = rnd.randint(-amt, amt)
        return tuple(max(0, min(255, v + k)) for v in c)

    for idx, caixa in enumerate(caixas):
        nome = caixa[0]
        sx, sy, sz = tamanho(caixa)
        u, v = UVS[esp["plano"]][idx]
        for face, (fx, fy, fw, fh) in regioes(u, v, sx, sy, sz).items():
            for j in range(fh):
                for i in range(fw):
                    c = cor_pixel(esp, nome, face, i, j, fw, fh, rnd)
                    if c is None:
                        continue
                    if len(c) == 3:
                        c = ruido(c) + (255,)
                    img.putpixel((fx + i, fy + j), c)
    return img


def rosto(esp, i, j, w, h, base, olho_y=None):
    """Olhos grandes (2x2 com brilho), bochechas e boca na face da frente."""
    pal = esp["pal"]
    olho = pal["olho"]
    oy = olho_y if olho_y is not None else max(1, h // 2 - 2)
    # olhos ficam a 1 pixel das bordas
    ex = [1, w - 3] if w >= 6 else [0, w - 2]
    for x0 in ex:
        if x0 <= i < x0 + 2 and oy <= j < oy + 2:
            if i == x0 and j == oy:
                return (255, 255, 255, 255)  # brilho
            return olho + (255,)
    if pal.get("sobrancelha") and j == oy - 1 and (ex[0] <= i < ex[0] + 2 or ex[1] <= i < ex[1] + 2):
        return clar(base, 0.5) + (255,)
    # bochechas rosadas
    if j == oy + 2 and (i == ex[0] - 1 or i == ex[1] + 2) and 0 <= i < w:
        return (250, 140, 150, 255)
    if j == oy + 2 and (i == ex[0] or i == ex[1] + 1):
        return (250, 150, 160, 255)
    return None


def cor_pixel(esp, nome, face, i, j, w, h, rnd):
    pal = esp["pal"]
    base, barriga, det = pal["base"], pal["barriga"], pal["detalhe"]
    plano = esp["plano"]
    sombra = {"topo": 1.12, "baixo": 0.8}.get(face, 1.0)
    c = clar(base, sombra)

    # --- detalhes comuns
    if face == "baixo" and nome in ("corpo",):
        return barriga
    if nome in ("perna", "braco"):
        if face == "baixo" or j >= h - 1:
            return clar(base, 0.6)
        return clar(base, 0.9)

    if plano == "raposa":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=2)
            if r:
                return r
            if j >= h - 2 and 2 <= i < w - 2:
                return barriga
        if nome == "focinho":
            if face == "frente" and j == 0 and i in (1, 2):
                return (40, 30, 30)  # nariz
            return barriga
        if nome.startswith("orelha"):
            if face == "frente" and j >= 1 and 0 < i < w - 1:
                return (255, 170, 160)
            return clar(base, 0.85) if j > 0 else det
        if nome == "corpo":
            if face in ("frente",) or (face in ("esq", "dir") and j >= h - 2):
                return barriga
            if pal.get("marcas") and face in ("esq", "dir") and (i + j) % 4 == 0 and j < h - 2:
                return det
            if pal.get("lava") and rnd.random() < 0.15:
                return (255, 140 + rnd.randint(0, 80), 30, 255)
        if nome == "cauda":
            # ponta da cauda em chama/amarela
            ponta = (face == "tras") or (face in ("esq", "dir", "topo", "baixo") and
                                         ((face in ("esq", "dir") and (i < 3 if face == "esq" else i >= w - 3)) or
                                          (face in ("topo", "baixo") and (j < 3 if face == "topo" else j >= h - 3))))
            if ponta:
                if pal.get("lava"):
                    return (255, 120 + rnd.randint(0, 100), 20, 255)
                return det
        if pal.get("lava") and nome == "cabeca" and face != "frente" and rnd.random() < 0.12:
            return (255, 150, 30, 255)
        return c

    if plano == "porquinho":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=2)
            if r:
                return r
        if nome == "focinho":
            if face == "frente" and j == 1 and i in (1, 2):
                return clar(barriga, 0.5)
            return barriga
        if nome == "broto":
            if face == "topo" or j < 2:
                return det
            return clar(det, 0.75)
        if nome.startswith("orelha"):
            return clar(base, 0.85)
        if pal.get("musgo") and nome in ("corpo", "cabeca") and face in ("topo", "tras", "esq", "dir"):
            limite = 3 if face in ("esq", "dir") else 99
            if face == "topo" or j < limite:
                if pal.get("flores") and rnd.random() < 0.08:
                    return rnd.choice([(250, 220, 70), (240, 120, 190), (255, 255, 255)])
                return mix(det, (40, 110, 40), rnd.random() * 0.5)
        if nome == "corpo" and face in ("esq", "dir") and j >= h - 2:
            return barriga
        if not pal.get("musgo") and nome == "corpo" and face == "topo" and (i % 3 == 1) and (j % 4 == 1):
            return clar(det, 1.1)  # pintinhas de folha
        return c

    if plano == "esquilo":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=1)
            if r:
                return r
            if j == h - 1 and i in (3, 4):
                return (60, 40, 40)
        if nome.startswith("orelha"):
            return det if j == 0 else c
        if nome == "corpo" and face == "frente":
            return barriga
        if nome == "cauda":
            if pal.get("raio") and face in ("esq", "dir", "tras") and (j + i) % 5 == 0:
                return det
            if face == "tras" or j < 3:
                return clar(base, 1.08)
        if nome == "corpo" and pal.get("raio") and face == "tras" and j in (2, 4) and 1 <= i < w - 1:
            return det
        return c

    if plano == "ave":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=1)
            if r:
                return r
        if nome in ("bico", "perna"):
            return det
        if nome == "crista":
            if pal.get("crista_longa"):
                return clar(det, 1.0 if j % 2 else 0.85)
            return clar(base, 0.75)
        if nome == "corpo" and face == "frente":
            return barriga
        if nome.startswith("asa"):
            return clar(base, 0.8) if j < h - 1 else clar(base, 0.6)
        if nome == "cauda":
            return clar(base, 0.75)
        return c

    if plano == "bolha":
        if nome == "corpo":
            if face == "frente":
                r = rosto(esp, i, j, w, h, base, olho_y=3)
                if r:
                    return r
                if j == 6 and i in (3, 4):
                    return clar(base, 0.45)  # boquinha
            if face == "topo" and (i, j) in ((1, 1), (2, 1), (1, 2)) and not pal.get("rocha"):
                return (245, 252, 255, 255)
            if pal.get("ondas") and face in ("esq", "dir", "tras") and j == 2 + (i % 2):
                return det
            if pal.get("rocha"):
                if rnd.random() < 0.18:
                    return clar(base, 0.75)
                if pal.get("sobrancelha") and face == "topo" and rnd.random() < 0.3:
                    return det  # musgo por cima
            if face in ("esq", "dir", "tras") and j >= h - 2:
                return barriga
        if nome == "topo":
            return det if not pal.get("rocha") else clar(base, 0.85)
        return c
    return c


# ---------------------------------------------------------------- C
def gerar_c(pngs):
    L = ["/* Gerado por misc/criaturas/criaturas.py. Nao editar a mao. */", ""]
    L.append("enum CriaturaAnim { ANIM_FIXO, ANIM_CABECA, ANIM_PERNA_A, ANIM_PERNA_B, ANIM_CAUDA, ANIM_ASA_E, ANIM_ASA_D };")
    L.append("enum CriaturaTipo { %s, TIPO_COUNT };" % ", ".join("TIPO_" + t for t in TIPOS))
    L.append("enum CriaturaHabitat { %s };" % ", ".join("HABITAT_" + h for h in HABITATS))
    L.append("")
    L.append("struct PecaDef { cc_uint8 u, v; cc_int8 x1,y1,z1, x2,y2,z2, px,py,pz; cc_uint8 anim; };")
    L.append("struct PlanoDef { const struct PecaDef* pecas; int count; float nameY, eyeY; cc_int8 bb[6]; };")
    L.append("struct EspecieDef { const char* nome; const char* cor; cc_uint8 tipo, plano; float escala;")
    L.append("\tcc_uint8 vida, dano; float vel; int evolui, nivelEvolui; cc_uint8 habitat, inicial;")
    L.append("\tconst cc_uint8* png; int pngLen; };")
    L.append("")
    for p in ORDEM_PLANOS:
        L.append("static const struct PecaDef pecas_%s[] = {" % p)
        for (nome, x1, y1, z1, x2, y2, z2, piv, anim), (u, v) in zip(PLANOS[p], UVS[p]):
            L.append("\t{ %2d,%2d, %3d,%3d,%3d, %3d,%3d,%3d, %3d,%3d,%3d, %d }, /* %s */" % (
                u, v, x1, y1, z1, x2, y2, z2, piv[0], piv[1], piv[2], anim, nome))
        L.append("};")
    L.append("#define PLANOS_COUNT %d" % len(ORDEM_PLANOS))
    L.append("static const struct PlanoDef planos_def[PLANOS_COUNT] = {")
    for p in ORDEM_PLANOS:
        cx = PLANOS[p]
        xs = [c[1] for c in cx] + [c[4] for c in cx]
        ys = [c[2] for c in cx] + [c[5] for c in cx]
        zs = [c[3] for c in cx] + [c[6] for c in cx]
        cab = next((c for c in cx if c[0] in ("cabeca",)), cx[0])
        eye = (cab[2] + cab[5]) / 2.0
        L.append("\t{ pecas_%s, %d, %.3ff, %.3ff, { %d,%d,%d, %d,%d,%d } }," % (
            p, len(cx), (max(ys) + 2) / 16.0, eye / 16.0, min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)))
    L.append("};")
    L.append("")
    for e in ESPECIES:
        data = pngs[e["id"]]
        L.append("static const cc_uint8 png_%s[%d] = {" % (e["id"], len(data)))
        for i in range(0, len(data), 20):
            L.append("\t" + ",".join("%d" % b for b in data[i:i + 20]) + ",")
        L.append("};")
    ids = [e["id"] for e in ESPECIES]
    L.append("")
    L.append("#define ESPECIES_COUNT %d" % len(ESPECIES))
    L.append("static const struct EspecieDef especies_def[ESPECIES_COUNT] = {")
    for e in ESPECIES:
        evo = ids.index(e["evolui"]) if e["evolui"] else -1
        L.append('\t{ "%s", "%s", TIPO_%s, %d, %.2ff, %d, %d, %.1ff, %d, %d, HABITAT_%s, %d, png_%s, sizeof(png_%s) },' % (
            e["nome"], e["cor"], e["tipo"], ORDEM_PLANOS.index(e["plano"]), e["escala"], e["vida"], e["dano"],
            e["vel"], evo, e["nivel"], e["habitat"], e["inicial"], e["id"], e["id"]))
    L.append("};")
    L.append("")
    return "\n".join(L)


def main():
    os.makedirs(TEX_DIR, exist_ok=True)
    pngs = {}
    for e in ESPECIES:
        img = pintar(e)
        path = os.path.join(TEX_DIR, e["id"] + ".png")
        img.save(path, optimize=True)
        with open(path, "rb") as f:
            pngs[e["id"]] = f.read()
    with open(HEADER, "w") as f:
        f.write(gerar_c(pngs))
    print("ok: %d especies, %d planos" % (len(ESPECIES), len(PLANOS)))


if __name__ == "__main__":
    main()
