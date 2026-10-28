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

# --- formas evoluidas: corpos diferentes, nao so maiores
def _troca(plano, nome, nova):
    return [nova if c[0] == nome else c for c in plano]

PLANOS["raposa2"] = _troca(_troca(PLANOS["raposa"],
    "orelha_e", ("orelha_e", -4, 13, -7, -1, 18, -6, (0, 8, -5), CABECA)),
    "orelha_d", ("orelha_d", 1, 13, -7, 4, 18, -6, (0, 8, -5), CABECA)) + [
    ("juba", -4, 4, -7, 4, 10, -3, (0, 0, 0), FIXO),
]
PLANOS["raposa3"] = [
    ("cabeca", -4, 9, -12, 4, 16, -6, (0, 11, -7), CABECA),
    ("focinho", -2, 9, -14, 2, 12, -12, (0, 11, -7), CABECA),
    ("orelha_e", -4, 16, -9, -1, 20, -8, (0, 11, -7), CABECA),
    ("orelha_d", 1, 16, -9, 4, 20, -8, (0, 11, -7), CABECA),
    ("juba", -5, 6, -8, 5, 14, -4, (0, 0, 0), FIXO),
    ("corpo", -3, 5, -6, 3, 11, 5, (0, 0, 0), FIXO),
    ("perna", -3, 0, -6, -1, 5, -4, (-2, 5, -5), PERNA_A),
    ("perna", 1, 0, -6, 3, 5, -4, (2, 5, -5), PERNA_B),
    ("perna", -3, 0, 2, -1, 5, 4, (-2, 5, 3), PERNA_B),
    ("perna", 1, 0, 2, 3, 5, 4, (2, 5, 3), PERNA_A),
    ("cauda", -2, 7, 5, 2, 11, 12, (0, 9, 5), CAUDA),
    ("chama", -1, 10, 10, 1, 15, 13, (0, 9, 5), CAUDA),
]
PLANOS["porquinho2"] = [c for c in PLANOS["porquinho"] if c[0] != "broto"] + [
    ("musgo", -3, 9, -4, 3, 11, 4, (0, 0, 0), FIXO),
    ("presa", -3, 4, -12, -2, 6, -11, (0, 7, -5), CABECA),
    ("presa", 2, 4, -12, 3, 6, -11, (0, 7, -5), CABECA),
]
PLANOS["porquinho3"] = [
    ("cabeca", -5, 5, -13, 5, 13, -6, (0, 9, -6), CABECA),
    ("focinho", -3, 6, -14, 3, 9, -13, (0, 9, -6), CABECA),
    ("presa", -4, 4, -14, -3, 7, -13, (0, 9, -6), CABECA),
    ("presa", 3, 4, -14, 4, 7, -13, (0, 9, -6), CABECA),
    ("orelha_e", -5, 13, -9, -3, 15, -8, (0, 9, -6), CABECA),
    ("orelha_d", 3, 13, -9, 5, 15, -8, (0, 9, -6), CABECA),
    ("corpo", -5, 3, -7, 5, 11, 6, (0, 0, 0), FIXO),
    ("perna", -5, 0, -7, -2, 3, -4, (-3, 3, -5), PERNA_A),
    ("perna", 2, 0, -7, 5, 3, -4, (3, 3, -5), PERNA_B),
    ("perna", -5, 0, 3, -2, 3, 6, (-3, 3, 4), PERNA_B),
    ("perna", 2, 0, 3, 5, 3, 6, (3, 3, 4), PERNA_A),
    ("tronco", -1, 11, -1, 1, 15, 1, (0, 0, 0), FIXO),
    ("copa", -3, 15, -3, 3, 19, 3, (0, 0, 0), FIXO),
]
PLANOS["esquilo2"] = _troca(_troca(_troca(PLANOS["esquilo"],
    "orelha_e", ("orelha_e", -4, 14, -3, -2, 20, -2, (0, 9, -2), CABECA)),
    "orelha_d", ("orelha_d", 2, 14, -3, 4, 20, -2, (0, 9, -2), CABECA)),
    "cauda", ("cauda", -3, 3, 3, 3, 17, 6, (0, 4, 4), CAUDA))
PLANOS["ave2"] = _troca(_troca(_troca(_troca(PLANOS["ave"],
    "crista", ("crista", -1, 13, -4, 1, 18, 2, (0, 8, -1), CABECA)),
    "asa_e", ("asa_e", -4, 2, -3, -3, 8, 4, (-3, 8, 0), ASA_E)),
    "asa_d", ("asa_d", 3, 2, -3, 4, 8, 4, (3, 8, 0), ASA_D)),
    "cauda", ("cauda", -2, 4, 4, 2, 5, 10, (0, 4, 4), CAUDA))
PLANOS["bolha2"] = _troca(_troca(_troca(PLANOS["bolha"],
    "topo", ("barbatana", -1, 8, -3, 1, 12, 4, (0, 0, 0), FIXO)),
    "braco", ("braco", -7, 2, -1, -4, 6, 1, (-5, 6, 0), PERNA_A)),
    "braco", ("braco", 4, 2, -1, 7, 6, 1, (5, 6, 0), PERNA_B))
PLANOS["bolha2"][2] = ("braco", -7, 2, -1, -4, 6, 1, (-5, 6, 0), PERNA_A)
PLANOS["bolha2"][3] = ("braco", 4, 2, -1, 7, 6, 1, (5, 6, 0), PERNA_B)
PLANOS["pedra2"] = [
    ("corpo", -4, 0, -4, 4, 8, 4, (0, 0, 0), FIXO),
    ("rocha", -6, 6, -2, -3, 9, 2, (0, 0, 0), FIXO),
    ("rocha", 3, 6, -2, 6, 9, 2, (0, 0, 0), FIXO),
    ("topo", -2, 8, -2, 2, 9, 2, (0, 0, 0), FIXO),
    ("braco", -7, 0, -1, -4, 6, 2, (-5, 6, 0), PERNA_A),
    ("braco", 4, 0, -1, 7, 6, 2, (5, 6, 0), PERNA_B),
]
PLANOS["lagarta"] = [
    ("cabeca", -3, 0, -8, 3, 6, -3, (0, 3, -4), CABECA),
    ("antena_e", -3, 6, -6, -2, 9, -5, (0, 3, -4), CABECA),
    ("antena_d", 2, 6, -6, 3, 9, -5, (0, 3, -4), CABECA),
    ("corpo", -3, 0, -3, 3, 5, 2, (0, 0, 0), FIXO),
    ("segmento", -3, 0, 2, 3, 4, 6, (0, 2, 2), PERNA_A),
    ("ponta", -2, 0, 6, 2, 3, 9, (0, 2, 6), CAUDA),
]
PLANOS["casulo"] = [
    ("corpo", -3, 0, -3, 3, 11, 3, (0, 0, 0), FIXO),
    ("topo", -2, 11, -2, 2, 13, 2, (0, 0, 0), FIXO),
    ("casca", -4, 2, -2, -3, 9, 2, (0, 0, 0), FIXO),
    ("casca", 3, 2, -2, 4, 9, 2, (0, 0, 0), FIXO),
    ("fio", 0, 13, 0, 1, 16, 1, (0, 0, 0), FIXO),
]
PLANOS["borboleta"] = [
    ("cabeca", -2, 7, -5, 2, 11, -1, (0, 9, -1), CABECA),
    ("antena_e", -2, 11, -4, -1, 15, -3, (0, 9, -1), CABECA),
    ("antena_d", 1, 11, -4, 2, 15, -3, (0, 9, -1), CABECA),
    ("corpo", -1, 5, -1, 1, 9, 6, (0, 0, 0), FIXO),
    ("asa_e", -12, 4, 1, -1, 16, 2, (-1, 9, 1), ASA_E),
    ("asa_d", 1, 4, 1, 12, 16, 2, (1, 9, 1), ASA_D),
    ("asinha_e", -8, 1, 2, -1, 7, 3, (-1, 6, 2), ASA_E),
    ("asinha_d", 1, 1, 2, 8, 7, 3, (1, 6, 2), ASA_D),
]
PLANOS["caranguejo"] = [
    ("corpo", -5, 2, -4, 5, 7, 3, (0, 0, 0), FIXO),
    ("olho_e", -4, 7, -4, -1, 10, -2, (0, 7, -3), CABECA),
    ("olho_d", 1, 7, -4, 4, 10, -2, (0, 7, -3), CABECA),
    ("garra", -9, 2, -8, -5, 6, -4, (-5, 4, -4), PERNA_A),
    ("garra", 5, 2, -8, 9, 6, -4, (5, 4, -4), PERNA_B),
    ("perna", -7, 0, -2, -5, 3, -1, (-5, 3, -1), PERNA_B),
    ("perna", -7, 0, 1, -5, 3, 2, (-5, 3, 1), PERNA_A),
    ("perna", 5, 0, -2, 7, 3, -1, (5, 3, -1), PERNA_A),
    ("perna", 5, 0, 1, 7, 3, 2, (5, 3, 1), PERNA_B),
]
PLANOS["caranguejo2"] = [
    ("corpo", -6, 3, -5, 6, 9, 4, (0, 0, 0), FIXO),
    ("espinho", -4, 9, -2, -2, 11, 0, (0, 0, 0), FIXO),
    ("espinho", 2, 9, -2, 4, 11, 0, (0, 0, 0), FIXO),
    ("espinho", -1, 9, 1, 1, 12, 3, (0, 0, 0), FIXO),
    ("olho_e", -5, 9, -5, -2, 12, -3, (0, 9, -4), CABECA),
    ("olho_d", 2, 9, -5, 5, 12, -3, (0, 9, -4), CABECA),
    ("garra", -12, 2, -11, -6, 8, -5, (-6, 5, -5), PERNA_A),
    ("garra", 6, 2, -11, 12, 8, -5, (6, 5, -5), PERNA_B),
    ("perna", -8, 0, -3, -6, 4, -2, (-6, 4, -2), PERNA_B),
    ("perna", -8, 0, 1, -6, 4, 2, (-6, 4, 1), PERNA_A),
    ("perna", 6, 0, -3, 8, 4, -2, (6, 4, -2), PERNA_A),
    ("perna", 6, 0, 1, 8, 4, 2, (6, 4, 1), PERNA_B),
]
PLANOS["toupeira"] = [
    ("cabeca", -3, 2, -9, 3, 8, -4, (0, 5, -4), CABECA),
    ("focinho", -1, 3, -10, 1, 5, -9, (0, 5, -4), CABECA),
    ("corpo", -4, 1, -4, 4, 8, 4, (0, 0, 0), FIXO),
    ("garra", -6, 0, -6, -3, 3, -3, (-4, 3, -4), PERNA_A),
    ("garra", 3, 0, -6, 6, 3, -3, (4, 3, -4), PERNA_B),
    ("perna", -3, 0, 2, -1, 1, 4, (-2, 1, 3), PERNA_B),
    ("perna", 1, 0, 2, 3, 1, 4, (2, 1, 3), PERNA_A),
    ("cauda", -1, 2, 4, 1, 3, 7, (0, 2, 4), CAUDA),
]
PLANOS["toupeira2"] = [
    ("cabeca", -4, 4, -11, 4, 11, -5, (0, 7, -5), CABECA),
    ("broca", -2, 4, -13, 2, 7, -11, (0, 7, -5), CABECA),
    ("broca", -1, 5, -15, 1, 6, -13, (0, 7, -5), CABECA),
    ("corpo", -5, 2, -5, 5, 11, 5, (0, 0, 0), FIXO),
    ("rocha", -4, 11, -3, 4, 13, 4, (0, 0, 0), FIXO),
    ("garra", -8, 0, -8, -4, 5, -4, (-5, 5, -5), PERNA_A),
    ("garra", 4, 0, -8, 8, 5, -4, (5, 5, -5), PERNA_B),
    ("perna", -4, 0, 2, -1, 2, 5, (-2, 2, 3), PERNA_B),
    ("perna", 1, 0, 2, 4, 2, 5, (2, 2, 3), PERNA_A),
    ("cauda", -1, 3, 5, 1, 4, 8, (0, 3, 5), CAUDA),
]
# fantasminhas: flutuam (corpo comeca no alto) e tem um rabinho de fumaca
PLANOS["fantasma"] = [
    ("corpo", -4, 5, -4, 4, 13, 4, (0, 0, 0), FIXO),
    ("veu", -3, 3, -3, 3, 5, 3, (0, 4, 0), CAUDA),
    ("fumaca", -2, 1, -2, 2, 3, 2, (0, 2, 0), CAUDA),
    ("braco", -6, 7, -1, -4, 10, 1, (-5, 10, 0), ASA_E),
    ("braco", 4, 7, -1, 6, 10, 1, (5, 10, 0), ASA_D),
]
PLANOS["fantasma2"] = [
    ("corpo", -5, 6, -5, 5, 16, 5, (0, 0, 0), FIXO),
    ("chifre", -5, 16, -2, -3, 19, 0, (0, 0, 0), FIXO),
    ("chifre", 3, 16, -2, 5, 19, 0, (0, 0, 0), FIXO),
    ("veu", -4, 3, -4, 4, 6, 4, (0, 5, 0), CAUDA),
    ("fumaca", -3, 0, -3, 3, 3, 3, (0, 3, 0), CAUDA),
    ("braco", -9, 8, -1, -5, 12, 2, (-5, 12, 0), ASA_E),
    ("braco", 5, 8, -1, 9, 12, 2, (5, 12, 0), ASA_D),
]
# focas de gelo: corpo baixo e comprido, nadadeiras e rabo
PLANOS["foca"] = [
    ("cabeca", -3, 3, -9, 3, 9, -3, (0, 5, -4), CABECA),
    ("focinho", -2, 3, -10, 2, 6, -9, (0, 5, -4), CABECA),
    ("corpo", -4, 0, -4, 4, 7, 5, (0, 0, 0), FIXO),
    ("nadadeira", -7, 0, -3, -4, 1, 0, (-4, 1, -2), PERNA_A),
    ("nadadeira", 4, 0, -3, 7, 1, 0, (4, 1, -2), PERNA_B),
    ("cauda", -3, 0, 5, 3, 2, 9, (0, 1, 5), CAUDA),
]
PLANOS["foca2"] = [
    ("cabeca", -4, 5, -12, 4, 12, -5, (0, 7, -5), CABECA),
    ("focinho", -3, 5, -13, 3, 8, -12, (0, 7, -5), CABECA),
    ("presa", -3, 3, -13, -2, 5, -12, (0, 7, -5), CABECA),
    ("presa", 2, 3, -13, 3, 5, -12, (0, 7, -5), CABECA),
    ("corpo", -6, 0, -6, 6, 10, 7, (0, 0, 0), FIXO),
    ("cristal", -3, 10, -3, -1, 14, -1, (0, 0, 0), FIXO),
    ("cristal", 1, 10, 0, 3, 13, 2, (0, 0, 0), FIXO),
    ("cristal", -2, 10, 3, 0, 12, 5, (0, 0, 0), FIXO),
    ("nadadeira", -10, 0, -4, -6, 2, 0, (-6, 2, -2), PERNA_A),
    ("nadadeira", 6, 0, -4, 10, 2, 0, (6, 2, -2), PERNA_B),
    ("cauda", -4, 0, 7, 4, 3, 12, (0, 1, 7), CAUDA),
]
# bau das criaturas (kit do comeco): a tampa gira pela dobradica de tras (animacao CABECA)
PLANOS["bau"] = [
    ("caixa", -6, 0, -5, 6, 8, 5, (0, 0, 0), FIXO),
    ("tampa", -6, 8, -5, 6, 11, 5, (0, 8, 5), CABECA),
    ("fecho", -1, 6, -6, 1, 9, -5, (0, 8, 5), CABECA),
]
# casulo de captura (parece uma bolota: corpo, tampa e cabinho; nao e uma bola)
PLANOS["casulo_item"] = [
    ("corpo", -2, 0, -2, 2, 5, 2, (0, 0, 0), FIXO),
    ("ponta", -1, -1, -1, 1, 0, 1, (0, 0, 0), FIXO),
    ("tampa", -3, 5, -3, 3, 7, 3, (0, 0, 0), FIXO),
    ("cabo", 0, 7, 0, 1, 9, 1, (0, 0, 0), FIXO),
]
ORDEM_PLANOS = ["raposa", "porquinho", "esquilo", "ave", "bolha", "raposa2", "raposa3", "porquinho2", "porquinho3", "esquilo2", "ave2", "bolha2", "pedra2",
                "lagarta", "casulo", "borboleta", "caranguejo", "caranguejo2", "toupeira", "toupeira2", "casulo_item",
                "fantasma", "fantasma2", "foca", "foca2", "bau"]

# ---------------------------------------------------------------- tipos
TIPOS = ["FOGO", "AGUA", "PLANTA", "ELETRICO", "VENTO", "PEDRA", "INSETO", "FANTASMA", "GELO"]
# habitat: onde nasce no mundo
# NEVE = chao de neve ou gelo; NOITE = qualquer lugar a noite, ou cavernas fundas
HABITATS = ["CAMPO", "AREIA", "AGUA", "ALTO", "PEDRA", "NEVE", "NOITE"]

# ---------------------------------------------------------------- especies
# nome, cor do nome (codigo &), tipo, plano, escala, vida, dano, velocidade,
# evolui_para, nivel_evolucao, habitat, inicial, paleta
ESPECIES = [
    # linha de fogo: raposinhas
    dict(id="faiscol", nome="Faiscol", cor="&6", tipo="FOGO", plano="raposa", escala=0.85,
         vida=10, dano=3, vel=4.0, evolui="labareda", nivel=5, habitat="AREIA", inicial=1,
         pal=dict(base=(240, 120, 40), barriga=(255, 225, 170), detalhe=(255, 215, 60), olho=(40, 25, 20))),
    dict(id="labareda", nome="Labareda", cor="&6", tipo="FOGO", plano="raposa2", escala=1.1,
         vida=16, dano=5, vel=4.5, evolui="fornalobo", nivel=10, habitat="AREIA", inicial=0,
         pal=dict(base=(220, 70, 35), barriga=(255, 210, 140), detalhe=(255, 190, 40), olho=(40, 20, 20), marcas=True)),
    dict(id="fornalobo", nome="Fornalobo", cor="&c", tipo="FOGO", plano="raposa3", escala=1.25,
         vida=24, dano=8, vel=4.8, evolui=None, nivel=0, habitat="AREIA", inicial=0,
         pal=dict(base=(120, 35, 30), barriga=(70, 30, 30), detalhe=(255, 150, 30), olho=(255, 220, 80), lava=True)),
    # linha de agua: bolhas
    dict(id="gotinha", nome="Gotinha", cor="&b", tipo="AGUA", plano="bolha", escala=0.8,
         vida=11, dano=2, vel=3.0, evolui="marolha", nivel=5, habitat="AGUA", inicial=1,
         pal=dict(base=(110, 190, 250), barriga=(200, 235, 255), detalhe=(60, 130, 230), olho=(20, 30, 70))),
    dict(id="marolha", nome="Marolha", cor="&b", tipo="AGUA", plano="bolha", escala=1.1,
         vida=17, dano=4, vel=3.2, evolui="tsunamor", nivel=10, habitat="AGUA", inicial=0,
         pal=dict(base=(60, 140, 235), barriga=(180, 220, 255), detalhe=(240, 250, 255), olho=(20, 25, 60), ondas=True)),
    dict(id="tsunamor", nome="Tsunamor", cor="&9", tipo="AGUA", plano="bolha2", escala=1.5,
         vida=26, dano=7, vel=3.2, evolui=None, nivel=0, habitat="AGUA", inicial=0,
         pal=dict(base=(35, 80, 190), barriga=(120, 180, 240), detalhe=(250, 250, 255), olho=(255, 255, 255), ondas=True, sobrancelha=True)),
    # linha de planta: porquinhos com broto
    dict(id="brotinho", nome="Brotinho", cor="&a", tipo="PLANTA", plano="porquinho", escala=0.8,
         vida=13, dano=2, vel=2.8, evolui="musgao", nivel=5, habitat="CAMPO", inicial=1,
         pal=dict(base=(150, 210, 110), barriga=(225, 245, 190), detalhe=(60, 160, 60), olho=(30, 40, 20))),
    dict(id="musgao", nome="Musgao", cor="&a", tipo="PLANTA", plano="porquinho2", escala=1.1,
         vida=20, dano=4, vel=2.6, evolui="florestauro", nivel=10, habitat="CAMPO", inicial=0,
         pal=dict(base=(110, 85, 55), barriga=(180, 150, 110), detalhe=(80, 160, 60), olho=(25, 25, 20), musgo=True)),
    dict(id="florestauro", nome="Florestauro", cor="&2", tipo="PLANTA", plano="porquinho3", escala=1.25,
         vida=30, dano=6, vel=2.4, evolui=None, nivel=0, habitat="CAMPO", inicial=0,
         pal=dict(base=(80, 60, 40), barriga=(140, 110, 80), detalhe=(50, 140, 50), olho=(250, 230, 120), musgo=True, flores=True)),
    # eletrico: esquilos
    dict(id="choquilo", nome="Choquilo", cor="&e", tipo="ELETRICO", plano="esquilo", escala=0.85,
         vida=9, dano=3, vel=5.0, evolui="trovesquilo", nivel=8, habitat="CAMPO", inicial=0,
         pal=dict(base=(250, 215, 70), barriga=(255, 245, 200), detalhe=(60, 60, 70), olho=(30, 30, 40), raio=True)),
    dict(id="trovesquilo", nome="Trovesquilo", cor="&e", tipo="ELETRICO", plano="esquilo2", escala=1.2,
         vida=16, dano=6, vel=5.5, evolui=None, nivel=0, habitat="CAMPO", inicial=0,
         pal=dict(base=(60, 60, 75), barriga=(250, 220, 80), detalhe=(255, 230, 60), olho=(255, 240, 120), raio=True)),
    # vento: passarinhos
    dict(id="penape", nome="Penape", cor="&3", tipo="VENTO", plano="ave", escala=0.85,
         vida=8, dano=2, vel=5.0, evolui="ventania", nivel=7, habitat="ALTO", inicial=0,
         pal=dict(base=(120, 180, 240), barriga=(245, 245, 255), detalhe=(250, 180, 50), olho=(20, 20, 40))),
    dict(id="ventania", nome="Ventania", cor="&3", tipo="VENTO", plano="ave2", escala=1.3,
         vida=15, dano=5, vel=5.5, evolui=None, nivel=0, habitat="ALTO", inicial=0,
         pal=dict(base=(50, 110, 200), barriga=(240, 240, 250), detalhe=(250, 170, 40), olho=(20, 20, 40), crista_longa=True)),
    # pedra: bolhas de pedra
    dict(id="seixo", nome="Seixo", cor="&7", tipo="PEDRA", plano="bolha", escala=0.85,
         vida=14, dano=3, vel=2.2, evolui="pedregulho", nivel=8, habitat="PEDRA", inicial=0,
         pal=dict(base=(150, 145, 140), barriga=(185, 180, 175), detalhe=(110, 105, 100), olho=(30, 30, 30), rocha=True)),
    dict(id="pedregulho", nome="Pedregulho", cor="&8", tipo="PEDRA", plano="pedra2", escala=1.35,
         vida=26, dano=6, vel=2.0, evolui=None, nivel=0, habitat="PEDRA", inicial=0,
         pal=dict(base=(105, 100, 95), barriga=(140, 135, 130), detalhe=(90, 150, 70), olho=(255, 200, 60), rocha=True, sobrancelha=True)),
    # inseto: lagarta -> casulo -> borboleta
    dict(id="larvito", nome="Larvito", cor="&a", tipo="INSETO", plano="lagarta", escala=1.0,
         vida=8, dano=2, vel=2.2, evolui="casulim", nivel=4, habitat="CAMPO", inicial=0,
         pal=dict(base=(140, 215, 90), barriga=(250, 235, 160), detalhe=(250, 150, 60), olho=(30, 35, 20), bolinhas=True)),
    dict(id="casulim", nome="Casulim", cor="&2", tipo="INSETO", plano="casulo", escala=0.95,
         vida=12, dano=1, vel=0.6, evolui="asalume", nivel=7, habitat="CAMPO", inicial=0,
         pal=dict(base=(170, 190, 90), barriga=(210, 220, 140), detalhe=(120, 140, 60), olho=(30, 35, 20), sonolento=True)),
    dict(id="asalume", nome="Asalume", cor="&d", tipo="INSETO", plano="borboleta", escala=1.1,
         vida=15, dano=5, vel=4.5, evolui=None, nivel=0, habitat="CAMPO", inicial=0,
         pal=dict(base=(90, 70, 120), barriga=(250, 245, 255), detalhe=(250, 120, 200), olho=(240, 60, 90), asa2=(120, 220, 250))),
    # agua: caranguejos da praia
    dict(id="pincudo", nome="Pincudo", cor="&c", tipo="AGUA", plano="caranguejo", escala=0.85,
         vida=11, dano=3, vel=3.0, evolui="tenazor", nivel=9, habitat="AREIA", inicial=0,
         pal=dict(base=(240, 110, 80), barriga=(255, 215, 180), detalhe=(255, 240, 230), olho=(25, 20, 30))),
    dict(id="tenazor", nome="Tenazor", cor="&4", tipo="AGUA", plano="caranguejo2", escala=1.2,
         vida=22, dano=7, vel=3.2, evolui=None, nivel=0, habitat="AREIA", inicial=0,
         pal=dict(base=(170, 50, 50), barriga=(240, 180, 140), detalhe=(250, 230, 200), olho=(255, 230, 90), sobrancelha=True)),
    # pedra: toupeiras cavadoras
    dict(id="toupito", nome="Toupito", cor="&6", tipo="PEDRA", plano="toupeira", escala=0.85,
         vida=12, dano=3, vel=2.6, evolui="cavucao", nivel=9, habitat="PEDRA", inicial=0,
         pal=dict(base=(120, 90, 70), barriga=(200, 170, 140), detalhe=(250, 160, 170), olho=(25, 20, 20))),
    dict(id="cavucao", nome="Cavucao", cor="&6", tipo="PEDRA", plano="toupeira2", escala=1.25,
         vida=24, dano=7, vel=2.6, evolui=None, nivel=0, habitat="PEDRA", inicial=0,
         pal=dict(base=(95, 70, 55), barriga=(185, 150, 120), detalhe=(150, 150, 160), olho=(255, 210, 80), rocha=True)),
    # fantasma: lencoizinhos que aparecem a noite
    dict(id="vultinho", nome="Vultinho", cor="&5", tipo="FANTASMA", plano="fantasma", escala=0.85,
         vida=10, dano=3, vel=3.4, evolui="assombrao", nivel=9, habitat="NOITE", inicial=0,
         pal=dict(base=(185, 165, 225), barriga=(235, 225, 250), detalhe=(120, 90, 180), olho=(40, 20, 70))),
    dict(id="assombrao", nome="Assombrao", cor="&5", tipo="FANTASMA", plano="fantasma2", escala=1.2,
         vida=20, dano=7, vel=3.8, evolui=None, nivel=0, habitat="NOITE", inicial=0,
         pal=dict(base=(95, 70, 150), barriga=(170, 140, 220), detalhe=(60, 40, 110), olho=(120, 255, 200), sobrancelha=True, brilho=True)),
    # gelo: focas da neve
    dict(id="friozinho", nome="Friozinho", cor="&f", tipo="GELO", plano="foca", escala=0.85,
         vida=12, dano=3, vel=2.8, evolui="geleirao", nivel=9, habitat="NEVE", inicial=0,
         pal=dict(base=(220, 235, 245), barriga=(250, 252, 255), detalhe=(140, 200, 240), olho=(25, 35, 60))),
    dict(id="geleirao", nome="Geleirao", cor="&b", tipo="GELO", plano="foca2", escala=1.25,
         vida=26, dano=7, vel=2.6, evolui=None, nivel=0, habitat="NEVE", inicial=0,
         pal=dict(base=(150, 190, 225), barriga=(230, 242, 252), detalhe=(170, 235, 255), olho=(20, 30, 70), sobrancelha=True)),
]


# ---------------------------------------------------------------- mestres de bioma
# Treinadores originais (um por tipo). Cada um tem uma equipe e da uma insignia.
MESTRES = [
    dict(id="tronco", nome="Mestre Tronco", titulo="Mestre das Plantas", insignia="Folha", cor="&2", tipo="PLANTA",
         equipe=[("larvito", 6), ("brotinho", 7), ("musgao", 10)],
         pal=dict(pele=(225, 180, 140), cabelo=(90, 60, 35), camisa=(70, 140, 60), calca=(110, 80, 50), detalhe=(200, 230, 120), sapato=(60, 45, 30))),
    dict(id="mare", nome="Mestra Mare", titulo="Mestra das Aguas", insignia="Onda", cor="&9", tipo="AGUA",
         equipe=[("gotinha", 9), ("pincudo", 10), ("marolha", 12)],
         pal=dict(pele=(240, 200, 170), cabelo=(40, 90, 170), camisa=(60, 150, 230), calca=(30, 60, 130), detalhe=(240, 250, 255), sapato=(250, 250, 250), longo=True)),
    dict(id="cascalho", nome="Mestre Cascalho", titulo="Mestre das Pedras", insignia="Rocha", cor="&7", tipo="PEDRA",
         equipe=[("seixo", 12), ("toupito", 12), ("pedregulho", 15)],
         pal=dict(pele=(160, 115, 80), cabelo=(40, 40, 40), camisa=(130, 120, 110), calca=(80, 70, 60), detalhe=(250, 200, 60), sapato=(50, 40, 30), capacete=True)),
    dict(id="faisca", nome="Mestra Faisca", titulo="Mestra dos Raios", insignia="Raio", cor="&e", tipo="ELETRICO",
         equipe=[("choquilo", 14), ("choquilo", 15), ("trovesquilo", 17)],
         pal=dict(pele=(235, 195, 160), cabelo=(250, 220, 60), camisa=(50, 50, 60), calca=(40, 40, 50), detalhe=(255, 230, 40), sapato=(250, 220, 60), longo=True)),
    dict(id="vendaval", nome="Mestre Vendaval", titulo="Mestre dos Ventos", insignia="Pena", cor="&3", tipo="VENTO",
         equipe=[("penape", 16), ("asalume", 18), ("ventania", 20)],
         pal=dict(pele=(200, 160, 120), cabelo=(240, 240, 245), camisa=(120, 190, 240), calca=(240, 240, 250), detalhe=(250, 170, 40), sapato=(90, 90, 110))),
    dict(id="brasa", nome="Mestra Brasa", titulo="Mestra do Fogo", insignia="Chama", cor="&c", tipo="FOGO",
         equipe=[("labareda", 20), ("labareda", 22), ("fornalobo", 25)],
         pal=dict(pele=(230, 185, 150), cabelo=(200, 50, 30), camisa=(220, 80, 30), calca=(60, 30, 30), detalhe=(255, 210, 60), sapato=(40, 20, 20), longo=True)),
]

# caixas da pele de humano 64x64: (nome, u, v, sx, sy, sz)
PELE_CAIXAS = [
    ("cabeca", 0, 0, 8, 8, 8), ("chapeu", 32, 0, 8, 8, 8),
    ("corpo", 16, 16, 8, 12, 4), ("braco", 40, 16, 4, 12, 4), ("braco", 32, 48, 4, 12, 4),
    ("perna", 0, 16, 4, 12, 4), ("perna", 16, 48, 4, 12, 4),
]


def pintar_mestre(m):
    pal = m["pal"]
    rnd = random.Random(m["id"])
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    pele, cab, cam, calca, det, sap = pal["pele"], pal["cabelo"], pal["camisa"], pal["calca"], pal["detalhe"], pal["sapato"]

    def ruido(c, amt=6):
        k = rnd.randint(-amt, amt)
        return tuple(max(0, min(255, v + k)) for v in c) + (255,)

    for nome, u, v, sx, sy, sz in PELE_CAIXAS:
        for face, (fx, fy, fw, fh) in regioes(u, v, sx, sy, sz).items():
            for j in range(fh):
                for i in range(fw):
                    c = None
                    if nome == "cabeca":
                        c = pele
                        if face == "topo":
                            c = cab
                        elif face == "tras":
                            c = cab if j < (7 if pal.get("longo") else 6) else pele
                        elif face in ("esq", "dir"):
                            c = cab if j < 2 or (pal.get("longo") and j < 7 and (i < 3 if face == "dir" else i >= fw - 3)) else pele
                        elif face == "frente":
                            c = pele
                            if j < 2:
                                c = cab
                            elif j in (3, 4) and i in (1, 2, 5, 6):
                                # olhos grandes: branco por fora, pupila por dentro
                                c = (255, 255, 255) if i in (1, 6) else (40, 30, 40)
                            elif j == 6 and 3 <= i <= 4:
                                c = clar(pele, 0.6)
                            elif j == 5 and i in (0, 7):
                                c = (240, 150, 150)
                            elif pal.get("longo") and i in (0, 7) and j < 6:
                                c = cab
                        elif face == "baixo":
                            c = pele
                    elif nome == "chapeu":
                        # faixa na testa (ou capacete de minerador)
                        if pal.get("capacete"):
                            if face == "topo" or (face != "baixo" and j < 3):
                                c = det if (face == "frente" and j == 1 and 3 <= i <= 4) else (230, 190, 40)
                        elif face in ("frente", "tras", "esq", "dir") and j == 2:
                            c = det
                    elif nome == "corpo":
                        c = cam
                        if j >= fh - 2 and face in ("frente", "tras", "esq", "dir"):
                            c = clar(calca, 0.8)  # cinto
                            if face == "frente" and j == fh - 2 and 3 <= i <= 4:
                                c = det
                        elif face == "frente" and 2 <= j <= 4 and 3 <= i <= 4:
                            c = det  # emblema do tipo
                        elif face in ("frente", "tras") and (i == 0 or i == fw - 1):
                            c = clar(cam, 0.8)
                    elif nome == "braco":
                        c = cam if (j < 4 or face == "topo") else pele
                        if j == 3 and face != "topo":
                            c = det
                    elif nome == "perna":
                        c = sap if j >= fh - 2 or face == "baixo" else calca
                    if c is not None:
                        img.putpixel((fx + i, fy + j), ruido(c))
    return img


# descricoes do Diario das Criaturas (sem acentos: a fonte do jogo e cp437)
DESCRICOES = {
    "faiscol": "Raposinha que acende a ponta do rabo quando fica feliz.",
    "labareda": "A juba esquenta tanto que derrete a neve em volta.",
    "fornalobo": "Dizem que o coracao dele e uma fornalha que nunca apaga.",
    "gotinha": "Uma gota viva que adora pular em pocas de chuva.",
    "marolha": "Faz pequenas ondas com os bracinhos para nadar rapido.",
    "tsunamor": "Quando fica bravo, levanta ondas do tamanho de casas.",
    "brotinho": "Dorme ao sol para o broto da cabeca crescer.",
    "musgao": "O musgo das costas guarda agua para os dias secos.",
    "florestauro": "Carrega uma arvore inteira nas costas, com flores e tudo.",
    "choquilo": "Guarda eletricidade no rabo felpudo para a noite.",
    "trovesquilo": "Pula de galho em galho soltando faiscas.",
    "penape": "Canta toda manha no alto das montanhas.",
    "ventania": "Bate as asas e levanta um vento que derruba folhas.",
    "seixo": "Parece uma pedra comum ate abrir os olhos.",
    "pedregulho": "Cresce musgo nas costas de tanto ficar parado.",
    "larvito": "Come folhas o dia inteiro para virar casulo.",
    "casulim": "Quase nao se mexe: esta se preparando para voar.",
    "asalume": "As asas brilham com o sol e confundem quem olha.",
    "pincudo": "Anda de lado na praia e belisca quem chega perto.",
    "tenazor": "As pincas gigantes quebram ate pedra.",
    "toupito": "Cava tuneis sem parar e quase nunca ve o sol.",
    "cavucao": "O focinho de broca atravessa rocha como se fosse areia.",
    "vultinho": "Aparece de noite so para dar sustos e sair rindo.",
    "assombrao": "Os olhos brilham no escuro e guiam quem se perdeu.",
    "friozinho": "Escorrega de barriga no gelo mais rapido que corre.",
    "geleirao": "Os cristais das costas nunca derretem, nem no deserto.",
}
for _e in ESPECIES:
    _e["desc"] = DESCRICOES[_e["id"]]


# ---------------------------------------------------------------- casulos de captura
# mesma ordem do enum do Criaturas.c (comum, reforcado, mare, mestre)
CASULOS = [
    dict(id="comum", corpo=(205, 160, 105), tampa=(120, 75, 40), detalhe=(220, 60, 50), marca="faixa"),
    dict(id="reforcado", corpo=(190, 195, 205), tampa=(90, 95, 110), detalhe=(250, 250, 255), marca="rebites"),
    dict(id="mare", corpo=(110, 200, 235), tampa=(30, 120, 170), detalhe=(245, 250, 255), marca="ondas"),
    dict(id="mestre", corpo=(250, 205, 60), tampa=(130, 60, 170), detalhe=(255, 255, 255), marca="estrela"),
]


def pintar_casulo(cs):
    rnd = random.Random("casulo" + cs["id"])
    img = Image.new("RGBA", (TEX_W, TEX_H), (0, 0, 0, 0))
    plano = PLANOS["casulo_item"]
    corpo, tampa, det = cs["corpo"], cs["tampa"], cs["detalhe"]
    for idx, caixa in enumerate(plano):
        nome = caixa[0]
        sx, sy, sz = tamanho(caixa)
        u, v = UVS["casulo_item"][idx]
        for face, (fx, fy, fw, fh) in regioes(u, v, sx, sy, sz).items():
            for j in range(fh):
                for i in range(fw):
                    if nome == "tampa":
                        # tampa com textura de casca (xadrez)
                        c = clar(tampa, 1.15 if (i + j) % 2 else 0.9)
                        if face != "topo" and face != "baixo" and j == fh - 1:
                            c = clar(tampa, 0.7)
                    elif nome == "cabo":
                        c = clar(tampa, 0.75)
                    elif nome == "ponta":
                        c = clar(corpo, 0.8)
                    else:
                        c = clar(corpo, 1.08 - 0.25 * j / max(1, fh - 1)) if face not in ("topo", "baixo") else clar(corpo, 0.9)
                        if face in ("frente", "tras", "esq", "dir"):
                            m = cs["marca"]
                            if m == "faixa" and j == 2:
                                c = det
                            elif m == "rebites" and j in (1, 3) and i in (0, fw - 1):
                                c = det
                            elif m == "rebites" and j == 2:
                                c = clar(tampa, 1.1)
                            elif m == "ondas" and j == 2 + (i % 2):
                                c = det
                            elif m == "estrela" and face in ("frente", "tras") and ((j == 2 and 0 < i < fw - 1) or (i in (1, 2) and j in (1, 3))):
                                c = det
                            elif m == "estrela" and face in ("esq", "dir") and j == 2:
                                c = clar(tampa, 1.2)
                            if face == "frente" and j == 0 and i == 1:
                                c = (255, 255, 255)  # brilho
                    k = rnd.randint(-5, 5)
                    img.putpixel((fx + i, fy + j), tuple(max(0, min(255, x + k)) for x in c) + (255,))
    return img


def pintar_bau():
    rnd = random.Random("bau")
    img = Image.new("RGBA", (TEX_W, TEX_H), (0, 0, 0, 0))
    madeira, escura, metal, ouro = (165, 110, 60), (105, 65, 35), (200, 205, 215), (250, 205, 60)
    folha = (90, 180, 80)
    for idx, caixa in enumerate(PLANOS["bau"]):
        nome = caixa[0]
        sx, sy, sz = tamanho(caixa)
        u, v = UVS["bau"][idx]
        for face, (fx, fy, fw, fh) in regioes(u, v, sx, sy, sz).items():
            for j in range(fh):
                for i in range(fw):
                    borda = i in (0, fw - 1) or j in (0, fh - 1)
                    if nome == "fecho":
                        c = ouro if not (face == "frente" and i == fw // 2 and j == fh // 2) else (60, 40, 20)
                    elif borda:
                        c = clar(metal, 0.85 if (i + j) % 2 else 1.0)  # cantoneiras de metal
                    elif face in ("topo", "baixo"):
                        c = clar(madeira, 0.92 if (j % 3) == 0 else 1.05)  # tabuas
                    else:
                        c = escura if j % 3 == 0 else clar(madeira, 1.0 - 0.15 * j / max(1, fh - 1))
                    # folhinha verde (simbolo das criaturas) no meio da tampa
                    if nome == "tampa" and face == "topo" and abs(i - fw // 2) + abs(j - fh // 2) <= 1:
                        c = folha
                    k = rnd.randint(-6, 6)
                    img.putpixel((fx + i, fy + j), tuple(max(0, min(255, x + k)) for x in c) + (255,))
    return img


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
                    # contorno suave em cada face, como os modelos em blocos
                    borda = fw >= 3 and fh >= 3 and (i in (0, fw - 1) or j in (0, fh - 1))
                    if borda and c[:3] != (255, 255, 255) and nome not in ("chama", "olho_e", "olho_d"):
                        c = clar(c[:3], 0.84) + (c[3],)
                    img.putpixel((fx + i, fy + j), c)
    return img


def rosto(esp, i, j, w, h, base, olho_y=None):
    """Olhos grandes (2x2 com brilho), bochechas e boca na face da frente."""
    pal = esp["pal"]
    olho = pal["olho"]
    oy = olho_y if olho_y is not None else max(1, h // 2 - 2)
    # olhos ficam a 1 pixel das bordas
    ex = [1, w - 3] if w >= 6 else [0, w - 2]
    alto = 3 if h >= 7 else 2  # olhos mais altos em cabecas grandes
    for x0 in ex:
        if x0 <= i < x0 + 2 and oy <= j < oy + alto:
            if i == x0 and j == oy:
                return (255, 255, 255, 255)  # brilho
            if alto == 3 and j == oy + 2:
                return clar(olho, 1.6) + (255,) if max(olho) < 120 else clar(olho, 0.7) + (255,)
            return olho + (255,)
    if pal.get("sobrancelha") and j == oy - 1 and (ex[0] <= i < ex[0] + 2 or ex[1] <= i < ex[1] + 2):
        return clar(base, 0.5) + (255,)
    # bochechas rosadas
    if j == oy + alto and (i == ex[0] - 1 or i == ex[1] + 2) and 0 <= i < w:
        return (250, 140, 150, 255)
    return None


def cor_pixel(esp, nome, face, i, j, w, h, rnd):
    pal = esp["pal"]
    base, barriga, det = pal["base"], pal["barriga"], pal["detalhe"]
    plano = esp["plano"].rstrip("23")
    if plano == "pedra":
        plano = "bolha"
    # sombreado suave: topo claro, laterais escurecem para baixo
    if face == "topo":
        c = clar(base, 1.12)
    elif face == "baixo":
        c = clar(base, 0.78)
    else:
        c = clar(base, 1.06 - 0.2 * j / max(1, h - 1))

    # --- pecas das formas evoluidas
    if nome == "juba":
        if face == "frente" and j >= 2 and 2 <= i < w - 2:
            return barriga
        return clar(det, 1.0 - 0.15 * ((i + j) % 2))
    if nome == "chama":
        return (255, 110 + int(120 * (1 - j / max(1, h - 1))), 20 + rnd.randint(0, 40), 255)
    if nome == "presa":
        return (245, 238, 215, 255)
    if nome == "musgo":
        if rnd.random() < 0.07:
            return rnd.choice([(250, 220, 70), (240, 120, 190)])
        return mix(det, (40, 110, 40), rnd.random() * 0.5)
    if nome == "tronco":
        return clar((110, 80, 50), 0.9 + 0.1 * (i % 2))
    if nome == "copa":
        if pal.get("flores") and rnd.random() < 0.1:
            return rnd.choice([(250, 220, 70), (240, 120, 190), (255, 255, 255)])
        return mix(det, (30, 90, 30), rnd.random() * 0.6)
    if nome == "barbatana":
        return det if j < h - 1 else clar(base, 0.9)
    if nome == "rocha" and plano == "toupeira":
        return clar((135, 135, 140), 0.75 + 0.3 * rnd.random())
    if nome == "rocha":
        if face == "topo" and rnd.random() < 0.5:
            return mix(det, (60, 120, 50), rnd.random() * 0.4)
        return clar(base, 0.8 + 0.15 * rnd.random())

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
    if plano == "lagarta":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=1)
            if r:
                return r
            if j == h - 1 and i in (2, 3):
                return clar(base, 0.45)
        if nome.startswith("antena"):
            return det if j == 0 else clar(base, 0.7)
        if face == "baixo" or (face in ("esq", "dir", "frente") and j >= h - 1):
            return barriga
        if pal.get("bolinhas") and face in ("esq", "dir") and nome != "cabeca" and (i + 1) % 3 == 0 and j == 1:
            return det
        if nome in ("corpo", "segmento") and face == "topo" and i in (0, w - 1):
            return clar(base, 0.8)
        return c

    if plano == "casulo":
        if nome == "corpo" and face == "frente":
            oy = 3
            for x0 in (1, w - 3):
                if x0 <= i < x0 + 2 and oy <= j < oy + 2:
                    if j == oy:
                        return clar(base, 0.55)  # palpebra sonolenta
                    return pal["olho"] if i != x0 else (255, 255, 255, 255)
            if j == oy + 2 and i in (0, w - 1):
                return (250, 150, 150, 255)
        if nome == "corpo" and face in ("esq", "dir", "tras") and j % 3 == 0:
            return det
        if nome == "casca":
            return clar(det, 0.9 + 0.1 * (j % 2))
        if nome == "fio":
            return (240, 240, 230, 255)
        if nome == "topo":
            return clar(base, 0.85)
        return c

    if plano == "borboleta":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base)
            if r:
                return r
        if nome.startswith("antena"):
            return det if j == 0 else clar(base, 0.8)
        if nome.startswith("asa") or nome.startswith("asinha"):
            asa2 = pal["asa2"]
            if face in ("topo", "baixo", "esq", "dir"):
                return clar(base, 0.9)
            borda = i == 0 or j == 0 or i == w - 1 or j == h - 1
            if borda:
                return clar(base, 0.8)
            # lado de fora da asa (longe do corpo)
            fora = (w - 1 - i) if (nome.endswith("_e")) == (face == "frente") else i
            cx, cy = (w - 1) * 0.35, (h - 1) * 0.45
            dx, dy = (w - 1 - fora) - cx, j - cy
            d = (dx * dx + dy * dy) ** 0.5
            if nome.startswith("asa_") and d < 1.0:
                return (255, 255, 255, 255)
            if nome.startswith("asa_") and d < 2.6:
                return det
            if fora < 2:
                return mix(asa2, base, 0.5)
            return mix(asa2, barriga, 0.35 * j / max(1, h - 1))
        if nome == "corpo":
            return clar(base, 1.0 if (j + i) % 2 else 0.85)
        return c

    if plano == "caranguejo":
        if nome.startswith("olho"):
            if face == "topo":
                return pal["olho"]
            if j < 2 and face in ("frente", "esq", "dir", "tras"):
                if face == "frente" and i == 0 and j == 0:
                    return (255, 255, 255, 255)
                if face == "frente" and i == 0:
                    return (235, 235, 240, 255)
                return pal["olho"]
            return c
        if nome == "corpo":
            if face == "frente":
                if pal.get("sobrancelha") and j == 0 and 2 <= i < w - 2:
                    return clar(base, 0.55)
                if j == h - 2 and 3 <= i < w - 3:
                    return clar(base, 0.45)  # sorriso
                if j == h - 3 and i in (2, w - 3):
                    return clar(base, 0.45)
                if j >= h - 3 and i in (1, w - 2):
                    return (250, 150, 150, 255)
            if face == "topo" and (i * 7 + j * 3) % 11 == 0:
                return det
            if face in ("esq", "dir", "tras") and j >= h - 1:
                return barriga
        if nome == "garra":
            if face == "frente" and j == h // 2:
                return clar(base, 0.45)  # abertura da pinca
            if face == "frente" or j == 0:
                return clar(base, 1.15)
        if nome == "espinho":
            return det if j == 0 or face == "topo" else clar(base, 1.1)
        if nome == "perna":
            return clar(base, 0.75)
        return c

    if plano == "fantasma":
        if nome == "corpo":
            if face == "frente":
                r = rosto(esp, i, j, w, h, base, olho_y=2)
                if r:
                    if pal.get("brilho") and r[:3] == pal["olho"]:
                        return clar(pal["olho"], 1.0 if j % 2 else 0.8) + (255,)
                    return r
                # boca aberta de "buu"
                if 3 <= i < w - 3 and h - 3 <= j < h - 1:
                    return clar(det, 0.6)
            if face == "topo" and pal.get("brilho") and (i + j) % 4 == 0:
                return clar(base, 1.25)
            if face in ("esq", "dir", "tras") and j >= h - 2:
                return mix(base, barriga, 0.6)
            return clar(c, 1.0 + 0.04 * ((i * 3 + j) % 3))
        if nome == "veu":
            # barra ondulada do lencol
            if face in ("frente", "tras", "esq", "dir") and j == h - 1 and i % 2 == 1:
                return clar(barriga, 0.9)
            return mix(base, barriga, 0.4)
        if nome == "fumaca":
            return mix(barriga, (255, 255, 255), 0.3 + 0.2 * ((i + j) % 2))
        if nome == "chifre":
            return clar(det, 1.0 if j else 1.3)
        if nome == "braco":
            return mix(base, barriga, 0.3)
        return c

    if plano == "foca":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=1)
            if r:
                return r
        if nome == "focinho":
            if face == "frente" and j == 0 and w // 2 - 1 <= i <= w // 2:
                return (45, 45, 60)  # nariz
            if face == "frente" and j == 1 and i in (0, w - 1):
                return clar(barriga, 0.75)  # bigodes
            return barriga
        if nome == "presa":
            return (250, 248, 235, 255)
        if nome == "cristal":
            if face == "topo" or (i + j) % 3 == 0:
                return (245, 252, 255, 255)
            return mix(det, (255, 255, 255), 0.25 * (j % 2))
        if nome == "nadadeira":
            return clar(det, 0.95)
        if nome == "cauda":
            return clar(det, 0.95) if face in ("tras", "topo") and j >= h - 2 else c
        if nome == "corpo":
            if face in ("frente", "baixo") or (face in ("esq", "dir") and j >= h - 2):
                return barriga
            # pintinhas azuis nas costas
            if face in ("topo", "esq", "dir") and (i * 5 + j * 3) % 9 == 0:
                return det
        return c

    if plano == "toupeira":
        if nome == "cabeca" and face == "frente":
            r = rosto(esp, i, j, w, h, base, olho_y=1)
            if r:
                return r
            if j >= h - 2 and 1 <= i < w - 1:
                return barriga
        if nome == "focinho":
            return det
        if nome == "broca":
            return clar(det, 1.15 if (i + j) % 3 == 0 else 0.85)
        if nome == "garra":
            if face == "frente" and j >= h - 2 and i % 2 == 0:
                return (250, 245, 230, 255)  # unhas
            return barriga
        if nome == "corpo" and (face == "frente" or (face in ("esq", "dir") and j >= h - 2)):
            return barriga
        if nome == "cauda":
            return det
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
    L.append("\tconst cc_uint8* png; int pngLen; const char* desc; };")
    L.append("")
    for p in ORDEM_PLANOS:
        L.append("static const struct PecaDef pecas_%s[] = {" % p)
        for (nome, x1, y1, z1, x2, y2, z2, piv, anim), (u, v) in zip(PLANOS[p], UVS[p]):
            L.append("\t{ %2d,%2d, %3d,%3d,%3d, %3d,%3d,%3d, %3d,%3d,%3d, %d }, /* %s */" % (
                u, v, x1, y1, z1, x2, y2, z2, piv[0], piv[1], piv[2], anim, nome))
        L.append("};")
    L.append("#define PLANOS_COUNT %d" % len(ORDEM_PLANOS))
    for i, p in enumerate(ORDEM_PLANOS):
        L.append("#define PLANO_%s %d" % (p.upper(), i))
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
        L.append('\t{ "%s", "%s", TIPO_%s, %d, %.2ff, %d, %d, %.1ff, %d, %d, HABITAT_%s, %d, png_%s, sizeof(png_%s), "%s" },' % (
            e["nome"], e["cor"], e["tipo"], ORDEM_PLANOS.index(e["plano"]), e["escala"], e["vida"], e["dano"],
            e["vel"], evo, e["nivel"], e["habitat"], e["inicial"], e["id"], e["id"], e["desc"]))
    L.append("};")
    L.append("")
    for cs in CASULOS:
        data = pngs["casulo_" + cs["id"]]
        L.append("static const cc_uint8 png_casulo_%s[%d] = {" % (cs["id"], len(data)))
        for i in range(0, len(data), 20):
            L.append("\t" + ",".join("%d" % b for b in data[i:i + 20]) + ",")
        L.append("};")
    data = pngs["bau"]
    L.append("static const cc_uint8 png_bau[%d] = {" % len(data))
    for i in range(0, len(data), 20):
        L.append("\t" + ",".join("%d" % b for b in data[i:i + 20]) + ",")
    L.append("};")
    L.append("#define CASULOS_COUNT %d" % len(CASULOS))
    L.append("static const struct { const char* nome; const cc_uint8* png; int pngLen; } casulos_def[CASULOS_COUNT] = {")
    for cs in CASULOS:
        L.append('\t{ "casulo_%s", png_casulo_%s, sizeof(png_casulo_%s) },' % (cs["id"], cs["id"], cs["id"]))
    L.append("};")
    L.append("")
    L.append("struct MestreDef { const char* nome; const char* titulo; const char* insignia; const char* cor; cc_uint8 tipo, count;")
    L.append("\tcc_uint8 especies[3], niveis[3]; const cc_uint8* png; int pngLen; };")
    for m in MESTRES:
        data = pngs["mestre_" + m["id"]]
        L.append("static const cc_uint8 png_mestre_%s[%d] = {" % (m["id"], len(data)))
        for i in range(0, len(data), 20):
            L.append("\t" + ",".join("%d" % b for b in data[i:i + 20]) + ",")
        L.append("};")
    L.append("#define MESTRES_COUNT %d" % len(MESTRES))
    L.append("static const struct MestreDef mestres_def[MESTRES_COUNT] = {")
    for m in MESTRES:
        eq = m["equipe"] + [m["equipe"][-1]] * (3 - len(m["equipe"]))
        L.append('\t{ "%s", "%s", "%s", "%s", TIPO_%s, %d, { %s }, { %s }, png_mestre_%s, sizeof(png_mestre_%s) },' % (
            m["nome"], m["titulo"], m["insignia"], m["cor"], m["tipo"], len(m["equipe"]),
            ", ".join(str(ids.index(e)) for e, _ in eq), ", ".join(str(n) for _, n in eq), m["id"], m["id"]))
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
    for cs in CASULOS:
        img = pintar_casulo(cs)
        path = os.path.join(TEX_DIR, "casulo_" + cs["id"] + ".png")
        img.save(path, optimize=True)
        with open(path, "rb") as f:
            pngs["casulo_" + cs["id"]] = f.read()
    img = pintar_bau()
    path = os.path.join(TEX_DIR, "bau.png")
    img.save(path, optimize=True)
    with open(path, "rb") as f:
        pngs["bau"] = f.read()
    for m in MESTRES:
        img = pintar_mestre(m)
        path = os.path.join(TEX_DIR, "mestre_" + m["id"] + ".png")
        img.save(path, optimize=True)
        with open(path, "rb") as f:
            pngs["mestre_" + m["id"]] = f.read()
    with open(HEADER, "w") as f:
        f.write(gerar_c(pngs))
    print("ok: %d especies, %d planos, %d mestres" % (len(ESPECIES), len(PLANOS), len(MESTRES)))


if __name__ == "__main__":
    main()
