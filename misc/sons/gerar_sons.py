# Gera os sons proprios do ClassiCube 3DS (blocos, monstros, criaturas, interface).
# Uso:  python3 misc/sons/gerar_sons.py
# Saida: misc/sons/saida/classicube.zip (passos e quebrar blocos)
#        misc/sons/saida/efeitos.zip    (fx_*.wav: criaturas, monstros, jogo)
# Todo som e sintetizado aqui, do zero.
import os, sys, zipfile, io
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sintese import *

AQUI  = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, 'saida')


# ===================================================================== BLOCOS
def bloco_grama(seed, passo):
    r = rng(seed)
    dur = 0.16 if passo else 0.24
    n = int(dur * SR)
    x = np.zeros(n)
    # varios "crecs" de folhinhas
    for k in range(10 if passo else 16):
        p = int(r.uniform(0, 0.6) * n)
        m = int(r.uniform(0.01, 0.03) * SR)
        g = ruido(m, seed * 31 + k) * env_exp(m, 0.006) * r.uniform(0.3, 1)
        x[p:p + m] += g[:max(0, min(m, n - p))]
    x = filtro(x, lo=900, hi=5200)
    return fade(x * env_exp(n, dur * 0.5))


def bloco_cascalho(seed, passo):
    r = rng(seed)
    dur = 0.18 if passo else 0.26
    n = int(dur * SR)
    x = np.zeros(n)
    for k in range(14 if passo else 22):
        p = int((r.uniform(0, 0.7) ** 1.5) * n)
        m = int(r.uniform(0.004, 0.012) * SR)
        g = ruido(m, seed * 17 + k) * env_exp(m, 0.003)
        g = filtro(g, lo=r.uniform(400, 900), hi=r.uniform(2500, 4500))
        x[p:p + m] += g[:max(0, min(m, n - p))] * r.uniform(0.4, 1)
    x += filtro(ruido(n, seed), lo=200, hi=1200) * env_exp(n, 0.04) * 0.4
    return fade(x)


def bloco_pedra(seed, passo):
    r = rng(seed)
    dur = 0.14 if passo else 0.22
    n = int(dur * SR)
    clique = filtro(ruido(n, seed), lo=500, hi=r.uniform(2500, 3500)) * env_exp(n, 0.012)
    corpo = seno(r.uniform(150, 210), n) * env_exp(n, 0.03) * 0.7
    corpo += seno(r.uniform(380, 520), n) * env_exp(n, 0.018) * 0.4
    grao = filtro(ruido(n, seed + 9), lo=300, hi=1400) * env_exp(n, 0.05) * 0.35
    return fade(clique + corpo + grao)


def bloco_madeira(seed, passo):
    r = rng(seed)
    dur = 0.16 if passo else 0.26
    n = int(dur * SR)
    f = r.uniform(170, 240)
    corpo = (seno(f, n) * env_exp(n, 0.05) + seno(f * 2.31, n) * env_exp(n, 0.03) * 0.5
             + seno(f * 3.9, n) * env_exp(n, 0.015) * 0.3)
    clique = filtro(ruido(n, seed), lo=800, hi=3000) * env_exp(n, 0.006) * 0.8
    return fade(corpo + clique)


def bloco_areia(seed, passo):
    r = rng(seed)
    dur = 0.18 if passo else 0.24
    n = int(dur * SR)
    x = filtro(ruido(n, seed), lo=600, hi=r.uniform(2800, 3800))
    e = env_adsr(n, a=0.02, d=0.04, s=0.5, r=dur * 0.6)
    gr = 1 + 0.5 * filtro(ruido(n, seed + 1), hi=60)
    return fade(x * e * gr)


def bloco_neve(seed, passo):
    r = rng(seed)
    dur = 0.2 if passo else 0.26
    n = int(dur * SR)
    x = np.zeros(n)
    for k in range(18):
        p = int(r.uniform(0, 0.75) * n)
        m = int(r.uniform(0.006, 0.016) * SR)
        g = ruido(m, seed * 7 + k) * env_exp(m, 0.004)
        x[p:p + m] += g[:max(0, min(m, n - p))] * r.uniform(0.3, 1)
    x = filtro(x, lo=1500, hi=6500) + filtro(ruido(n, seed), lo=400, hi=1500) * env_exp(n, 0.06) * 0.4
    return fade(x * env_adsr(n, 0.01, 0.05, 0.7, dur * 0.5))


def bloco_tecido(seed, passo):
    r = rng(seed)
    dur = 0.16 if passo else 0.22
    n = int(dur * SR)
    x = filtro(ruido(n, seed), lo=150, hi=r.uniform(700, 1100)) * env_adsr(n, 0.01, 0.04, 0.4, dur * 0.6)
    return fade(x)


def bloco_vidro(seed):
    r = rng(seed)
    dur = 0.6
    n = int(dur * SR)
    x = np.zeros(n)
    for k in range(14):
        p = int(r.uniform(0, 0.25) * n)
        f = r.uniform(1800, 6500)
        m = n - p
        x[p:] += seno(f, m) * env_exp(m, r.uniform(0.03, 0.12)) * r.uniform(0.3, 1)
    x += filtro(ruido(n, seed), lo=2500) * env_exp(n, 0.03) * 0.8
    return fade(x)


def gerar_blocos(z):
    tipos = {
        'grass': bloco_grama, 'gravel': bloco_cascalho, 'stone': bloco_pedra, 'wood': bloco_madeira,
        'sand': bloco_areia, 'snow': bloco_neve, 'cloth': bloco_tecido,
    }
    for nome, fn in tipos.items():
        for i in range(1, 5):
            z.add('dig_%s%d.wav' % (nome, i), fn(100 + i * 13 + len(nome), False), 0.8)
            z.add('step_%s%d.wav' % (nome, i), fn(500 + i * 7 + len(nome), True), 0.55)
    for i in range(1, 4):
        z.add('dig_glass%d.wav' % i, bloco_vidro(900 + i), 0.75)


# ===================================================================== VOZES
def voz(f0, dur, forms, vib=5.0, vib_prof=0.02, seed=0, ruido_mix=0.08, glide=None, q=5.0):
    """voz sintetica: pulso rico em harmonicos passando por formantes"""
    n = int(dur * SR)
    tt = np.arange(n) / SR
    if glide:
        f = np.interp(tt, np.linspace(0, dur, len(glide)), np.array(glide) * f0)
    else:
        f = np.full(n, f0)
    f = f * (1 + vib_prof * np.sin(2 * np.pi * vib * tt))
    src = serra(f, n) + ruido(n, seed) * ruido_mix
    return formantes(src, forms, [q] * len(forms), [1, 0.7, 0.4][:len(forms)])


# ----------------------------------------------- monstros da noite
def fx_zumbi(seed):
    r = rng(seed)
    dur = r.uniform(0.9, 1.2)
    n = int(dur * SR)
    x = voz(r.uniform(75, 95), dur, [380, 900, 2300], vib=r.uniform(3, 6), vib_prof=0.05,
            seed=seed, ruido_mix=0.3, glide=[1.0, 1.15, 0.85, 0.7], q=4)
    x *= env_adsr(n, 0.12, 0.2, 0.8, 0.35)
    return distorcer(normalizar(x), 2.0)


def fx_esqueleto(seed):
    r = rng(seed)
    dur = 0.5
    n = int(dur * SR)
    x = np.zeros(n)
    tpos = 0.0
    while tpos < dur - 0.04:
        m = int(0.04 * SR)
        p = int(tpos * SR)
        f = r.uniform(900, 1600)
        k = (seno(f, m) * 0.6 + seno(f * 2.7, m) * 0.3 + filtro(ruido(m, int(tpos * 1000)), lo=1500) * 0.4) * env_exp(m, 0.008)
        x[p:p + m] += k[:max(0, min(m, n - p))]
        tpos += r.uniform(0.03, 0.08)
    return fade(x)


def fx_aranha(seed):
    r = rng(seed)
    dur = 0.6
    n = int(dur * SR)
    tt = t_(dur)
    sib = filtro(ruido(n, seed), lo=3000, hi=7500) * env_adsr(n, 0.05, 0.1, 0.6, 0.25)
    chit = filtro(ruido(n, seed + 1), lo=1500, hi=4000) * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * r.uniform(28, 38) * tt)))
    chit *= env_exp(n, 0.25)
    return fade(sib * 0.6 + chit * 0.7)


def fx_chiado(seed):
    """pavio do creeper"""
    dur = 1.5
    n = int(dur * SR)
    tt = t_(dur)
    x = ruido(n, seed)
    x = lowpass_var(x, 900, 7000) - lowpass_var(x, 300, 2500) * 0.8
    x *= np.linspace(0.3, 1, n) * (1 + 0.3 * np.sin(2 * np.pi * 14 * tt))
    return fade(x, 0.05, 0.03)


def fx_explosao(seed):
    dur = 1.6
    n = int(dur * SR)
    tt = t_(dur)
    x = ruido(n, seed)
    grave = lowpass_var(x, 2500, 120) * env_exp(n, 0.35)
    estalo = filtro(x, lo=1000) * env_exp(n, 0.04) * 0.6
    sub = seno(np.geomspace(90, 35, n), n) * env_exp(n, 0.3) * 0.9
    return distorcer(normalizar(grave + estalo + sub), 2.5)


def fx_golpe(seed):
    """pancada generica (bater em algo)"""
    r = rng(seed)
    dur = 0.18
    n = int(dur * SR)
    th = seno(np.geomspace(r.uniform(160, 200), 60, n), n) * env_exp(n, 0.04)
    x = filtro(ruido(n, seed), lo=200, hi=2500) * env_exp(n, 0.02)
    return fade(th + x * 0.8)


# ----------------------------------------------- animais
def fx_porco(seed):
    r = rng(seed)
    partes = []
    for k in range(r.integers(1, 3) + 1):
        d = r.uniform(0.11, 0.17)
        n = int(d * SR)
        v = voz(r.uniform(160, 210), d, [550, 1300], vib=0, vib_prof=0, seed=seed + k, ruido_mix=0.5,
                glide=[1.1, 1.0, 0.8], q=5) * env_adsr(n, 0.01, 0.03, 0.8, 0.05)
        partes.append(em(v, k * 0.17))
    return distorcer(normalizar(mix(*partes)), 1.8)


def fx_ovelha(seed):
    r = rng(seed)
    dur = r.uniform(0.7, 0.9)
    n = int(dur * SR)
    tt = t_(dur)
    x = voz(r.uniform(300, 360), dur, [600, 1900, 2600], vib=0, vib_prof=0, seed=seed, ruido_mix=0.1,
            glide=[1.0, 1.05, 1.0, 0.95], q=6)
    x *= (1 + 0.6 * np.sin(2 * np.pi * r.uniform(17, 22) * tt))  # tremido "beee"
    return x * env_adsr(n, 0.04, 0.1, 0.8, 0.25)


def fx_galinha(seed):
    r = rng(seed)
    partes = []
    p = 0.0
    for k in range(r.integers(2, 5)):
        d = r.uniform(0.05, 0.09)
        n = int(d * SR)
        v = voz(r.uniform(500, 750), d, [900, 2200], vib=0, vib_prof=0, seed=seed + k, ruido_mix=0.15,
                glide=[0.9, 1.25, 1.0], q=5) * env_adsr(n, 0.005, 0.02, 0.7, 0.03)
        partes.append(em(v, p))
        p += d + r.uniform(0.04, 0.09)
    return mix(*partes)


# ----------------------------------------------- jogador e mundo
def fx_dano(seed):
    """o jogador levou dano: baque + 'uh' curto"""
    r = rng(seed)
    dur = 0.28
    n = int(dur * SR)
    baque = seno(np.geomspace(180, 70, n), n) * env_exp(n, 0.05)
    v = voz(r.uniform(140, 170), dur, [650, 1100], vib=0, vib_prof=0, seed=seed, ruido_mix=0.25,
            glide=[1.1, 0.8], q=4) * env_adsr(n, 0.01, 0.05, 0.5, 0.15)
    return fade(baque + v * 0.5)


def fx_comer(seed):
    r = rng(seed)
    partes = []
    for k in range(3):
        d = 0.09
        n = int(d * SR)
        c = filtro(ruido(n, seed * 3 + k), lo=900, hi=r.uniform(3500, 5000)) * env_exp(n, 0.02)
        c += filtro(ruido(n, seed * 5 + k), lo=150, hi=600) * env_exp(n, 0.03) * 0.6
        partes.append(em(c, k * 0.16 + r.uniform(0, 0.02)))
    return mix(*partes)


def fx_morte(seed):
    dur = 1.4
    n = int(dur * SR)
    f = np.geomspace(330, 110, n)
    x = (triangulo(f, n) + triangulo(f * 1.5, n) * 0.4) * env_adsr(n, 0.02, 0.2, 0.7, 0.6)
    return reverb(x, 1.0, 0.3)


def arpejo(notas, passo, dur_nota, timbre='sino', brilho=1.0, rev=0.25):
    partes = []
    for i, nm in enumerate(notas):
        f = nota(nm) if isinstance(nm, str) else nm
        n = int(dur_nota * SR)
        if timbre == 'sino':
            s = fm(f, n, 3.5, 2.0 * brilho, env_exp(n, 0.15)) * env_exp(n, dur_nota * 0.4)
        elif timbre == 'chip':
            s = quadrada(f, n, harm=9) * env_adsr(n, 0.003, 0.05, 0.6, dur_nota * 0.3) * 0.6
        else:
            s = triangulo(f, n) * env_exp(n, dur_nota * 0.5)
        partes.append(em(s, i * passo))
    return reverb(mix(*partes), 0.9, rev)


def fx_criar(seed):
    """craft: toc-toc de madeira e um brilhinho"""
    a = bloco_madeira(seed, True)
    b = bloco_madeira(seed + 1, True)
    s = arpejo(['E6', 'B6'], 0.06, 0.4, 'sino', 0.6, 0.2)
    return mix(a, em(b, 0.1), em(s * 0.5, 0.2))


def fx_bau_abrir(seed):
    dur = 0.55
    n = int(dur * SR)
    tt = t_(dur)
    # rangido: atrito periodico com frequencia subindo
    f = np.linspace(55, 95, n) * (1 + 0.1 * np.sin(2 * np.pi * 3 * tt))
    pulsos = np.maximum(0, np.sin(fase(f, n))) ** 12
    x = formantes(pulsos, [700, 1500], [5, 7]) * env_adsr(n, 0.03, 0.1, 0.8, 0.15)
    tampa = bloco_madeira(seed, False)
    return mix(x, em(tampa * 0.8, 0.42))


def fx_bau_fechar(seed):
    a = bloco_madeira(seed, False)
    b = filtro(ruido(int(0.12 * SR), seed), hi=500) * env_exp(int(0.12 * SR), 0.03)
    return mix(a, b * 0.8)


def fx_fogo(seed):
    """fornalha acesa / item pronto: estalinhos de lenha"""
    r = rng(seed)
    dur = 0.9
    n = int(dur * SR)
    x = filtro(ruido(n, seed), lo=200, hi=1500) * 0.25 * env_adsr(n, 0.1, 0.1, 0.8, 0.3)
    for k in range(10):
        p = int(r.uniform(0, 0.85) * n)
        m = int(0.01 * SR)
        c = filtro(ruido(m, seed + k), lo=2000) * env_exp(m, 0.002) * r.uniform(0.4, 1)
        x[p:p + m] += c[:max(0, min(m, n - p))]
    return fade(x)


def fx_quebrou(seed):
    """ferramenta quebrou"""
    dur = 0.45
    n = int(dur * SR)
    estalo = filtro(ruido(n, seed), lo=1500) * env_exp(n, 0.015)
    metal = (fm(1200, n, 1.41, 3, env_exp(n, 0.05)) * env_exp(n, 0.12))
    cai = arpejo([nota('A5'), nota('E5')], 0.12, 0.25, 'tri', 1, 0.1)
    return mix(estalo, metal * 0.5, em(cai * 0.4, 0.08))


def fx_dormir(seed):
    return arpejo(['G5', 'E5', 'C5', 'G4'], 0.28, 1.0, 'sino', 0.4, 0.4)


def fx_portal(seed):
    dur = 2.0
    n = int(dur * SR)
    tt = t_(dur)
    x = ruido(n, seed)
    sweep = np.abs(np.sin(2 * np.pi * 0.7 * tt)) * 2500 + 300
    out = np.zeros(n)
    # varredura de banda (efeito "vuuum")
    bloco = 1024
    for i in range(0, n, bloco):
        seg = x[i:i + bloco]
        out[i:i + bloco] = ressonante(seg, sweep[i], 3)[:len(seg)]
    hum = seno(np.linspace(70, 110, n), n) * 0.4 + seno(np.linspace(105, 165, n), n) * 0.2
    return reverb((out + hum) * env_adsr(n, 0.3, 0.3, 0.8, 0.6), 1.2, 0.35)


def fx_sorte(seed):
    return arpejo(['C6', 'E6', 'G6', 'C7', 'E7'], 0.06, 0.6, 'sino', 0.8, 0.35)


def fx_azar(seed):
    dur = 0.9
    n = int(dur * SR)
    tt = t_(dur)
    f = np.repeat([nota('G3'), nota('F#3'), nota('F3'), nota('E3')], n // 4 + 1)[:n]
    x = serra(f, n) * (0.6 + 0.4 * np.sin(2 * np.pi * 6 * tt))
    x = filtro(x, hi=1800) * env_adsr(n, 0.01, 0.05, 0.8, 0.2)
    return x


# ----------------------------------------------- criaturas
def chirp(f0, dur, forma, seed=0, vib=8.0, prof=0.04, harm=0.4):
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f = np.interp(tt, np.linspace(0, dur, len(forma)), np.array(forma) * f0)
    f = f * (1 + prof * np.sin(2 * np.pi * vib * tt))
    x = seno(f, n) + harm * seno(f * 2, n) * 0.5 + harm * seno(f * 3, n) * 0.25
    return x * env_adsr(n, 0.01, 0.04, 0.8, dur * 0.35)


def fx_cri_fogo(seed):
    r = rng(seed)
    a = chirp(r.uniform(600, 700), 0.22, [0.9, 1.3, 1.1], seed)
    b = chirp(r.uniform(700, 800), 0.16, [1.0, 1.4], seed + 1)
    est = filtro(ruido(int(0.4 * SR), seed), lo=2500) * 0
    for k in range(6):
        p = int(r.uniform(0, 0.35) * SR)
        m = int(0.008 * SR)
        est[p:p + m] += filtro(ruido(m, seed + k), lo=2000) * env_exp(m, 0.002) * 0.5
    return mix(a, em(b, 0.2), est)


def fx_cri_agua(seed):
    r = rng(seed)
    partes = []
    for k in range(3):
        d = 0.09
        n = int(d * SR)
        f = np.geomspace(r.uniform(350, 450), r.uniform(900, 1300), n)  # bolha subindo
        partes.append(em(seno(f, n) * env_adsr(n, 0.005, 0.02, 0.8, 0.04), k * 0.08 + r.uniform(0, 0.02)))
    c = chirp(r.uniform(550, 650), 0.2, [1.0, 1.2, 0.9], seed, vib=10, prof=0.06)
    return mix(*partes, em(c * 0.8, 0.26))


def fx_cri_planta(seed):
    r = rng(seed)
    c = chirp(r.uniform(420, 500), 0.32, [1.0, 1.1, 1.25, 1.0], seed, vib=5, prof=0.02, harm=0.6)
    fol = filtro(ruido(int(0.3 * SR), seed), lo=2000, hi=6000) * env_exp(int(0.3 * SR), 0.08) * 0.25
    return mix(c, fol)


def fx_cri_eletrico(seed):
    r = rng(seed)
    dur = 0.3
    n = int(dur * SR)
    f = np.interp(np.arange(n), [0, n * 0.3, n], [r.uniform(700, 800), r.uniform(1000, 1200), 800])
    x = quadrada(f, n, harm=7) * env_adsr(n, 0.005, 0.05, 0.7, 0.1)
    zap = filtro(ruido(n, seed), lo=3000) * (np.sin(2 * np.pi * 60 * t_(dur)) > 0.6) * 0.4
    return x * 0.6 + zap * env_exp(n, 0.1)


def fx_cri_vento(seed):
    r = rng(seed)
    a = chirp(r.uniform(1300, 1500), 0.14, [1.0, 1.3], seed, vib=0, prof=0, harm=0.1)
    b = chirp(r.uniform(1500, 1700), 0.2, [1.2, 0.9, 1.1], seed + 1, vib=25, prof=0.05, harm=0.1)
    return mix(a, em(b, 0.16))


def fx_cri_pedra(seed):
    r = rng(seed)
    dur = 0.35
    n = int(dur * SR)
    v = voz(r.uniform(120, 150), dur, [500, 1100], vib=0, vib_prof=0, seed=seed, ruido_mix=0.2,
            glide=[1.0, 1.3, 1.1], q=4) * env_adsr(n, 0.02, 0.05, 0.8, 0.12)
    clack = bloco_pedra(seed, True)
    return mix(v * 0.7, em(clack * 0.6, 0.25))


def fx_cri_inseto(seed):
    r = rng(seed)
    dur = 0.4
    n = int(dur * SR)
    tt = t_(dur)
    f = r.uniform(900, 1100) * (1 + 0.08 * np.sin(2 * np.pi * 40 * tt))
    x = serra(f, n, harm=8) * (0.5 + 0.5 * np.sin(2 * np.pi * 30 * tt) ** 2)
    return filtro(x, lo=500, hi=4000) * env_adsr(n, 0.02, 0.05, 0.8, 0.12)


def fx_cri_fantasma(seed):
    r = rng(seed)
    dur = 0.9
    n = int(dur * SR)
    tt = t_(dur)
    f = np.interp(tt, [0, dur * 0.4, dur], [380, 520, 300]) * (1 + 0.03 * np.sin(2 * np.pi * 5 * tt))
    x = seno(f, n) + 0.3 * seno(f * 2.01, n)
    x *= env_adsr(n, 0.15, 0.1, 0.8, 0.35)
    return reverb(x, 1.3, 0.45)


def fx_cri_gelo(seed):
    r = rng(seed)
    c = chirp(r.uniform(900, 1050), 0.2, [1.0, 1.25, 1.1], seed, vib=12, prof=0.03, harm=0.2)
    tin = arpejo([r.uniform(2500, 3000), r.uniform(3200, 3800)], 0.05, 0.3, 'sino', 0.5, 0.2)
    return mix(c, em(tin * 0.4, 0.12))


# ----------------------------------------------- batalha de criaturas
def fx_esp_fogo(seed):
    dur = 0.8
    n = int(dur * SR)
    x = ruido(n, seed)
    x = lowpass_var(x, 600, 3500) * env_adsr(n, 0.05, 0.1, 0.8, 0.35)
    ronco = filtro(ruido(n, seed + 1), hi=250) * 1.5 * env_adsr(n, 0.05, 0.1, 0.8, 0.3)
    return distorcer(normalizar(x + ronco), 1.5)


def fx_esp_agua(seed):
    dur = 0.7
    n = int(dur * SR)
    jato = filtro(ruido(n, seed), lo=800, hi=5000) * env_adsr(n, 0.02, 0.1, 0.7, 0.3)
    r = rng(seed)
    bolhas = []
    for k in range(8):
        d = 0.05; m = int(d * SR)
        f = np.geomspace(r.uniform(300, 600), r.uniform(800, 1600), m)
        bolhas.append(em(seno(f, m) * env_exp(m, 0.02) * 0.6, r.uniform(0, 0.6)))
    return mix(jato * 0.7, *bolhas)


def fx_esp_planta(seed):
    dur = 0.35
    n = int(dur * SR)
    chic = lowpass_var(ruido(n, seed), 800, 7000) * np.linspace(0.2, 1, n) ** 3
    estalo = em(filtro(ruido(int(0.04 * SR), seed), lo=2000) * env_exp(int(0.04 * SR), 0.006), 0.3)
    return mix(chic * 0.8, estalo)


def fx_esp_eletrico(seed):
    r = rng(seed)
    dur = 0.6
    n = int(dur * SR)
    x = np.zeros(n)
    p = 0
    while p < n:
        m = int(r.uniform(0.01, 0.05) * SR)
        seg = filtro(ruido(m, p), lo=r.uniform(800, 3000)) * r.uniform(0.3, 1)
        x[p:p + m] += seg[:max(0, min(m, n - p))]
        p += m + int(r.uniform(0, 0.02) * SR)
    zumb = quadrada(np.full(n, 110.0), n, harm=15) * 0.3
    return distorcer(normalizar((x + zumb) * env_adsr(n, 0.005, 0.1, 0.8, 0.2)), 3)


def fx_esp_vento(seed):
    dur = 0.9
    n = int(dur * SR)
    tt = t_(dur)
    x = ruido(n, seed)
    out = np.zeros(n)
    bloco = 512
    cf = 600 + 1800 * np.sin(np.pi * tt / dur)
    for i in range(0, n, bloco):
        seg = x[i:i + bloco]
        out[i:i + bloco] = ressonante(seg, cf[i], 4)[:len(seg)]
    return out * env_adsr(n, 0.15, 0.1, 0.8, 0.3)


def fx_esp_pedra(seed):
    dur = 0.9
    n = int(dur * SR)
    r = rng(seed)
    ronco = filtro(ruido(n, seed), hi=300) * env_adsr(n, 0.05, 0.2, 0.7, 0.4) * 2
    x = ronco
    for k in range(5):
        x = mix(x, em(bloco_pedra(seed + k, False) * 0.6, r.uniform(0, 0.7)))
    return x


def fx_esp_inseto(seed):
    dur = 0.6
    n = int(dur * SR)
    tt = t_(dur)
    x = mix(*[serra(np.full(n, f), n, harm=10) * 0.3 for f in (220, 233, 247)])
    x *= (0.6 + 0.4 * np.sin(2 * np.pi * 45 * tt))
    return filtro(x, lo=200, hi=3500) * env_adsr(n, 0.05, 0.1, 0.8, 0.2)


def fx_esp_fantasma(seed):
    dur = 1.0
    n = int(dur * SR)
    tt = t_(dur)
    f = 200 * (1 + 0.15 * np.sin(2 * np.pi * 3 * tt))
    x = seno(f, n) + seno(f * 1.5, n) * 0.5 + seno(f * 2.13, n) * 0.3
    x += filtro(ruido(n, seed), lo=1500, hi=4000) * 0.2
    return reverb(x * env_adsr(n, 0.2, 0.2, 0.8, 0.4), 1.5, 0.5)


def fx_esp_gelo(seed):
    r = rng(seed)
    sopro = filtro(ruido(int(0.6 * SR), seed), lo=3000, hi=8000) * env_adsr(int(0.6 * SR), 0.05, 0.1, 0.7, 0.3) * 0.5
    cristais = []
    for k in range(9):
        f = r.uniform(2000, 5000)
        m = int(0.4 * SR)
        cristais.append(em(fm(f, m, 2.76, 1.5, env_exp(m, 0.05)) * env_exp(m, 0.08) * 0.4, r.uniform(0.1, 0.6)))
    return reverb(mix(sopro, *cristais), 0.8, 0.3)


def fx_super(seed):
    """super golpe (nivel 15): sobe e explode"""
    dur = 0.6
    n = int(dur * SR)
    sobe = serra(np.geomspace(150, 1200, n), n) * np.linspace(0.2, 1, n)
    sobe = filtro(sobe, hi=4000)
    boom = fx_explosao(seed) * 0.8
    return mix(sobe * 0.6, em(boom, 0.55))


def fx_casulo_lancar(seed):
    dur = 0.35
    n = int(dur * SR)
    x = lowpass_var(ruido(n, seed), 500, 5000) * env_adsr(n, 0.05, 0.05, 0.8, 0.15)
    return x


def fx_casulo_balanca(seed):
    a = bloco_madeira(seed, True)
    b = bloco_madeira(seed + 1, True)
    return mix(a, em(b * 0.7, 0.09))


def fx_captura(seed):
    clique = fx_casulo_balanca(seed)
    j = arpejo(['C5', 'E5', 'G5', 'C6'], 0.09, 0.5, 'chip', 1, 0.25)
    return mix(clique * 0.6, em(j, 0.15))


def fx_escapou(seed):
    pop = filtro(ruido(int(0.05 * SR), seed), lo=500) * env_exp(int(0.05 * SR), 0.01)
    dur = 0.35
    n = int(dur * SR)
    bloop = seno(np.geomspace(900, 250, n), n) * env_exp(n, 0.12)
    return mix(pop, em(bloop, 0.03))


def fx_nivel(seed):
    return arpejo(['C5', 'E5', 'G5', 'C6', 'G5', 'C6'], 0.07, 0.3, 'chip', 1, 0.2)


def fx_evolucao(seed):
    dur = 2.4
    n = int(dur * SR)
    tt = t_(dur)
    f = np.geomspace(220, 880, n)
    x = (seno(f, n) + seno(f * 1.5, n) * 0.5 + seno(f * 2, n) * 0.3) * (0.7 + 0.3 * np.sin(2 * np.pi * 9 * tt))
    x *= env_adsr(n, 0.3, 0.2, 0.8, 0.3) * 0.5
    acorde = arpejo(['C6', 'E6', 'G6', 'C7'], 0.0, 1.5, 'sino', 0.7, 0.4)
    brilhos = arpejo(['G6', 'C7', 'E7', 'G7', 'C7', 'E7'], 0.12, 0.4, 'sino', 0.4, 0.2)
    return reverb(mix(x, em(brilhos * 0.5, 1.0), em(acorde, 2.0)), 1.2, 0.3)


def fx_desmaio(seed):
    return arpejo(['E5', 'C5', 'A4', 'E4'], 0.13, 0.4, 'tri', 1, 0.2)


def fx_insignia(seed):
    """fanfarra curta (ganhou insignia / venceu o Mestre)"""
    notas = [('C5', 0, 0.15), ('C5', 0.15, 0.15), ('C5', 0.3, 0.15), ('G5', 0.45, 0.6),
             ('F5', 1.05, 0.15), ('A5', 1.2, 0.15), ('C6', 1.35, 0.9)]
    partes = []
    for nm, ini, d in notas:
        n = int(d * SR)
        f = nota(nm)
        s = (quadrada(f, n, harm=11) * 0.5 + triangulo(f / 2, n) * 0.5) * env_adsr(n, 0.005, 0.04, 0.7, min(0.15, d * 0.4))
        partes.append(em(s, ini))
    return reverb(mix(*partes), 1.0, 0.2)


def fx_desafio(seed):
    """um Mestre desafiou: acorde de alerta"""
    notas = [('A4', 0, 0.12), ('C5', 0.12, 0.12), ('E5', 0.24, 0.12), ('A5', 0.36, 0.5)]
    partes = []
    for nm, ini, d in notas:
        n = int(d * SR)
        f = nota(nm)
        partes.append(em(quadrada(f, n, harm=9) * env_adsr(n, 0.004, 0.03, 0.7, 0.08), ini))
    n = int(0.9 * SR)
    partes.append(filtro(ruido(n, seed), lo=4000) * env_exp(n, 0.2) * 0.3)
    return reverb(mix(*partes), 0.8, 0.2)


def fx_lendaria(seed):
    dur = 2.2
    n = int(dur * SR)
    ronco = voz(70, dur, [300, 750, 2000], vib=4, vib_prof=0.04, seed=seed, ruido_mix=0.5,
                glide=[0.8, 1.2, 1.0, 0.7], q=3) * env_adsr(n, 0.2, 0.3, 0.8, 0.6)
    acorde = mix(*[triangulo(nota(x), n) * 0.3 for x in ('D3', 'A3', 'F4')]) * env_adsr(n, 0.5, 0.3, 0.7, 0.8)
    return reverb(distorcer(normalizar(ronco), 2) + acorde, 1.5, 0.35)


# ----------------------------------------------- monstros grandes e chefoes
def fx_mosquito(seed):
    dur = 0.8
    n = int(dur * SR)
    tt = t_(dur)
    f = 520 * (1 + 0.06 * np.sin(2 * np.pi * 7 * tt) + 0.02 * np.sin(2 * np.pi * 31 * tt))
    x = serra(f, n, harm=12)
    return filtro(x, lo=400, hi=5000) * env_adsr(n, 0.15, 0.1, 0.8, 0.25)


def fx_golem(seed):
    """passo pesado de metal"""
    r = rng(seed)
    dur = 0.8
    n = int(dur * SR)
    baque = seno(np.geomspace(110, 40, n), n) * env_exp(n, 0.12) * 1.2
    metal = fm(r.uniform(180, 230), n, 1.41, 4, env_exp(n, 0.2)) * env_exp(n, 0.25) * 0.5
    pedra = bloco_pedra(seed, False) * 0.6
    return distorcer(normalizar(mix(baque, metal, pedra)), 1.5)


def rugido(f0, dur, seed, forms=(400, 900, 2400), dist=3.0):
    n = int(dur * SR)
    tt = t_(dur)
    v = voz(f0, dur, list(forms), vib=6, vib_prof=0.05, seed=seed, ruido_mix=0.7,
            glide=[0.8, 1.3, 1.15, 0.8], q=3)
    rosnado = 1 + 0.4 * np.sin(2 * np.pi * 23 * tt)
    return distorcer(normalizar(v * rosnado * env_adsr(n, 0.08, 0.2, 0.8, 0.4)), dist)


def fx_chefao(seed):
    return reverb(rugido(60, 1.8, seed), 1.4, 0.3)


def fx_dragao(seed):
    return reverb(rugido(rng(seed).uniform(95, 120), 1.3, seed, (500, 1200, 2800), 2.5), 1.0, 0.25)


def fx_sopro_fogo(seed):
    dur = 1.3
    n = int(dur * SR)
    x = lowpass_var(ruido(n, seed), 400, 4000)
    x += filtro(ruido(n, seed + 1), hi=200) * 1.5
    return distorcer(normalizar(x * env_adsr(n, 0.08, 0.2, 0.8, 0.5)), 1.8)


def fx_sopro_gelo(seed):
    dur = 1.3
    n = int(dur * SR)
    x = filtro(ruido(n, seed), lo=2500, hi=9000) * env_adsr(n, 0.1, 0.2, 0.7, 0.5)
    return mix(x * 0.7, fx_esp_gelo(seed + 3) * 0.6)


# ===================================================================== LISTA
# nome no jogo -> (funcao, variacoes, volume relativo)
EFEITOS = {
    'zumbi': (fx_zumbi, 3, 0.85), 'esqueleto': (fx_esqueleto, 2, 0.7), 'aranha': (fx_aranha, 2, 0.7),
    'chiado': (fx_chiado, 1, 0.8), 'explosao': (fx_explosao, 2, 1.0), 'golpe': (fx_golpe, 3, 0.8),
    'porco': (fx_porco, 3, 0.75), 'ovelha': (fx_ovelha, 2, 0.7), 'galinha': (fx_galinha, 3, 0.65),
    'dano': (fx_dano, 2, 0.85), 'comer': (fx_comer, 2, 0.7), 'morte': (fx_morte, 1, 0.8),
    'criar': (fx_criar, 1, 0.7), 'bau_abrir': (fx_bau_abrir, 1, 0.75), 'bau_fechar': (fx_bau_fechar, 1, 0.7),
    'fogo': (fx_fogo, 1, 0.6), 'quebrou': (fx_quebrou, 1, 0.8), 'dormir': (fx_dormir, 1, 0.6),
    'portal': (fx_portal, 1, 0.8), 'sorte': (fx_sorte, 1, 0.7), 'azar': (fx_azar, 1, 0.7),
    'cri_fogo': (fx_cri_fogo, 2, 0.7), 'cri_agua': (fx_cri_agua, 2, 0.7), 'cri_planta': (fx_cri_planta, 2, 0.7),
    'cri_eletrico': (fx_cri_eletrico, 2, 0.6), 'cri_vento': (fx_cri_vento, 2, 0.6), 'cri_pedra': (fx_cri_pedra, 2, 0.7),
    'cri_inseto': (fx_cri_inseto, 2, 0.6), 'cri_fantasma': (fx_cri_fantasma, 2, 0.7), 'cri_gelo': (fx_cri_gelo, 2, 0.6),
    'esp_fogo': (fx_esp_fogo, 1, 0.8), 'esp_agua': (fx_esp_agua, 1, 0.8), 'esp_planta': (fx_esp_planta, 1, 0.8),
    'esp_eletrico': (fx_esp_eletrico, 1, 0.7), 'esp_vento': (fx_esp_vento, 1, 0.8), 'esp_pedra': (fx_esp_pedra, 1, 0.8),
    'esp_inseto': (fx_esp_inseto, 1, 0.7), 'esp_fantasma': (fx_esp_fantasma, 1, 0.8), 'esp_gelo': (fx_esp_gelo, 1, 0.7),
    'super': (fx_super, 1, 0.9), 'casulo': (fx_casulo_lancar, 1, 0.7), 'balanca': (fx_casulo_balanca, 1, 0.7),
    'captura': (fx_captura, 1, 0.8), 'escapou': (fx_escapou, 1, 0.7), 'nivel': (fx_nivel, 1, 0.7),
    'evolucao': (fx_evolucao, 1, 0.8), 'desmaio': (fx_desmaio, 1, 0.7), 'insignia': (fx_insignia, 1, 0.8),
    'desafio': (fx_desafio, 1, 0.8), 'lendaria': (fx_lendaria, 1, 0.9),
    'mosquito': (fx_mosquito, 1, 0.5), 'golem': (fx_golem, 2, 0.9), 'chefao': (fx_chefao, 2, 1.0),
    'dragao': (fx_dragao, 2, 0.9), 'sopro_fogo': (fx_sopro_fogo, 1, 0.8), 'sopro_gelo': (fx_sopro_gelo, 1, 0.8),
}


class Zip:
    def __init__(self, caminho):
        self.z = zipfile.ZipFile(caminho, 'w', zipfile.ZIP_DEFLATED)
        self.total = 0

    def add(self, nome, x, pico):
        buf = io.BytesIO()
        salvar_wav(buf, silencio_fim(x / (np.max(np.abs(x)) + 1e-9)), pico)
        data = buf.getvalue()
        self.total += len(data)
        self.z.writestr(nome, data)

    def close(self):
        self.z.close()


def main(soPrevia=False):
    os.makedirs(SAIDA, exist_ok=True)
    z = Zip(os.path.join(SAIDA, 'classicube.zip'))
    gerar_blocos(z)
    z.close()
    print('classicube.zip: %d KB de audio' % (z.total // 1024))

    z = Zip(os.path.join(SAIDA, 'efeitos.zip'))
    for nome, (fn, var, vol) in EFEITOS.items():
        for i in range(1, var + 1):
            z.add('fx_%s%d.wav' % (nome, i), fn(1000 + i * 101 + len(nome) * 7), vol)
    z.close()
    print('efeitos.zip: %d KB de audio, %d grupos' % (z.total // 1024, len(EFEITOS)))


if __name__ == '__main__':
    main()
