# Compoe e sintetiza as musicas proprias do ClassiCube 3DS.
# Uso:  python3 misc/sons/gerar_musica.py [nome]
# Saida: misc/sons/saida/musica/<nome>.ogg (precisa do ffmpeg com libvorbis)
#
# As musicas sao compostas aqui por regras simples (acordes + melodia que
# anda pela escala e repousa nas notas do acorde) com sementes fixas, entao
# o resultado e sempre o mesmo. Nada e copiado de outro jogo.
import os, sys, subprocess
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sintese import *

AQUI  = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, 'saida', 'musica')

ESCALAS = {
    'maior':    [0, 2, 4, 5, 7, 9, 11],
    'menor':    [0, 2, 3, 5, 7, 8, 10],
    'dorico':   [0, 2, 3, 5, 7, 9, 10],
    'frigio':   [0, 1, 3, 5, 7, 8, 10],
    'harmonica': [0, 2, 3, 5, 7, 8, 11],
    'lidio':    [0, 2, 4, 6, 7, 9, 11],
}


# ===================================================================== INSTRUMENTOS
def i_piano(f, d, v):
    n = int((d + 1.2) * SR)
    x = fm(f, n, 1.0, 1.6, env_exp(n, 0.35)) * env_exp(n, 0.9)
    x += fm(f, n, 14.0, 0.3, env_exp(n, 0.05)) * env_exp(n, 0.1) * 0.15
    rel = int(d * SR)
    if rel < n:
        x[rel:] *= np.exp(-np.arange(n - rel) / (0.15 * SR))
    return x * v


def i_kalimba(f, d, v):
    n = int(1.2 * SR)
    x = fm(f, n, 3.01, 1.1, env_exp(n, 0.06)) * env_exp(n, 0.45)
    return x * v


def i_caixinha(f, d, v):  # caixinha de musica
    n = int(1.6 * SR)
    x = (fm(f, n, 3.5, 1.4, env_exp(n, 0.1)) * env_exp(n, 0.6) + seno(f * 2, n) * env_exp(n, 0.25) * 0.2)
    return x * v


def i_sino(f, d, v):
    n = int(2.5 * SR)
    x = fm(f, n, 1.4, 2.2, env_exp(n, 0.4)) * env_exp(n, 1.0)
    return x * v * 0.8


def i_dedilhado(f, d, v):
    x = karplus(f, min(2.0, d + 0.8), seed=int(f * 10) % 997, brilho=0.6, decay=0.997)
    return fade(x, 0.001, 0.05) * v


def i_flauta(f, d, v):
    n = int((d + 0.15) * SR)
    tt = np.arange(n) / SR
    vib = 1 + 0.006 * np.sin(2 * np.pi * 5.0 * tt) * np.clip(tt / 0.4, 0, 1)
    x = seno(f * vib, n) + 0.25 * seno(2 * f * vib, n) + 0.08 * seno(3 * f * vib, n)
    x += filtro(ruido(n, int(f)), lo=f, hi=f * 4) * 0.05
    return x * env_adsr(n, 0.06, 0.1, 0.8, 0.15) * v


def i_pad(f, d, v):
    n = int((d + 0.8) * SR)
    x = sum(serra(f * k, n, harm=10) for k in (1.0, 1.004, 0.996)) / 3
    x += seno(f / 2, n) * 0.3
    x = filtro(x, hi=1400)
    return x * env_adsr(n, 0.5, 0.3, 0.8, 0.7) * v


def i_pad_escuro(f, d, v):
    n = int((d + 1.0) * SR)
    tt = np.arange(n) / SR
    x = sum(serra(f * k, n, harm=8) for k in (1.0, 1.006, 0.994)) / 3
    x = filtro(x, hi=700) * (0.85 + 0.15 * np.sin(2 * np.pi * 0.3 * tt))
    return x * env_adsr(n, 0.9, 0.3, 0.85, 0.9) * v


def i_baixo(f, d, v):
    n = int((d + 0.1) * SR)
    x = triangulo(f, n) + seno(f, n) * 0.6
    return x * env_adsr(n, 0.01, 0.1, 0.7, 0.08) * v


def i_chip(f, d, v):
    n = int((d * 0.92 + 0.02) * SR)
    tt = np.arange(n) / SR
    vib = 1 + 0.01 * np.sin(2 * np.pi * 6 * tt) * np.clip((tt - 0.15) / 0.2, 0, 1)
    x = quadrada(f * vib, n, harm=9) * 0.55
    return x * env_adsr(n, 0.004, 0.06, 0.75, 0.04) * v


def i_chip_baixo(f, d, v):
    n = int((d * 0.85 + 0.01) * SR)
    x = triangulo(f, n) * 0.9 + quadrada(f, n, harm=3) * 0.2
    return x * env_adsr(n, 0.003, 0.03, 0.8, 0.02) * v


def i_baixo_sujo(f, d, v):
    n = int((d * 0.9 + 0.01) * SR)
    x = serra(f, n, harm=20) + serra(f * 1.01, n, harm=20)
    x = distorcer(filtro(x, hi=1800), 2.5)
    return x * env_adsr(n, 0.003, 0.05, 0.8, 0.03) * v * 0.5


def i_lead_forte(f, d, v):
    n = int((d * 0.95 + 0.02) * SR)
    tt = np.arange(n) / SR
    vib = 1 + 0.012 * np.sin(2 * np.pi * 5.5 * tt) * np.clip((tt - 0.2) / 0.2, 0, 1)
    x = serra(f * vib, n, harm=14) * 0.5 + quadrada(f * vib * 1.003, n, harm=9) * 0.3
    x = filtro(x, hi=3500)
    return x * env_adsr(n, 0.006, 0.08, 0.8, 0.06) * v


def i_drone(f, d, v):
    n = int((d + 1.5) * SR)
    tt = np.arange(n) / SR
    x = serra(f, n, harm=12) + serra(f * 1.498, n, harm=8) * 0.4
    lfo = 0.6 + 0.4 * np.sin(2 * np.pi * 0.15 * tt)
    x = filtro(x, hi=500) * lfo
    return x * env_adsr(n, 1.2, 0.5, 0.9, 1.4) * v


INSTR = {
    'piano': i_piano, 'kalimba': i_kalimba, 'caixinha': i_caixinha, 'sino': i_sino,
    'dedilhado': i_dedilhado, 'flauta': i_flauta, 'pad': i_pad, 'pad_escuro': i_pad_escuro,
    'baixo': i_baixo, 'chip': i_chip, 'chip_baixo': i_chip_baixo, 'baixo_sujo': i_baixo_sujo,
    'lead_forte': i_lead_forte, 'drone': i_drone,
}


# ----------------------------------------------- bateria
def d_bumbo(v, seed=0):
    n = int(0.35 * SR)
    x = seno(np.geomspace(130, 42, n), n) * env_exp(n, 0.09)
    x += filtro(ruido(n, seed), hi=900) * env_exp(n, 0.006) * 0.4
    return x * v


def d_caixa(v, seed=0):
    n = int(0.25 * SR)
    x = filtro(ruido(n, seed), lo=900, hi=7000) * env_exp(n, 0.05)
    x += seno(np.geomspace(240, 170, n), n) * env_exp(n, 0.03) * 0.6
    return x * v


def d_chimbal(v, seed=0):
    n = int(0.06 * SR)
    return filtro(ruido(n, seed), lo=7000) * env_exp(n, 0.012) * v


def d_chocalho(v, seed=0):
    n = int(0.09 * SR)
    return filtro(ruido(n, seed), lo=4000, hi=10000) * env_adsr(n, 0.02, 0.02, 0.5, 0.05) * v


def d_tambor(v, seed=0):  # tambor grave (Nether)
    n = int(0.8 * SR)
    x = seno(np.geomspace(95, 55, n), n) * env_exp(n, 0.25)
    x += filtro(ruido(n, seed), lo=80, hi=600) * env_exp(n, 0.05) * 0.5
    return x * v


BATERIA = {'K': d_bumbo, 'S': d_caixa, 'H': d_chimbal, 'C': d_chocalho, 'T': d_tambor}


# ===================================================================== COMPOSICAO
class Musica:
    def __init__(self, bpm, tonica, escala, compasso=4):
        self.bpm = bpm
        self.tonica = midi(tonica)
        self.escala = ESCALAS[escala]
        self.compasso = compasso
        self.eventos = {}   # trilha -> lista de (inicio_batida, dur_batidas, midi, vel)
        self.batidas = []   # (inicio, letra, vel)
        self.volumes = {}
        self.reverb_trilha = {}

    def seg(self, batidas):
        return batidas * 60.0 / self.bpm

    # grau da escala (0 = tonica, pode ser negativo/maior que 7) -> MIDI
    def grau(self, g, oitava=0):
        o, i = divmod(g, 7)
        return self.tonica + 12 * (o + oitava) + self.escala[i]

    def acorde(self, g, notas=3):
        return [g + 2 * k for k in range(notas)]

    def nota(self, trilha, ini, dur, m, vel=0.8):
        self.eventos.setdefault(trilha, []).append((ini, dur, m, vel))

    # ---------------------------------------------------------- melodia
    def melodia(self, acordes, ini, semente, ritmos, faixa=(7, 16), cadencia=True,
                comeca=None, pausa=0.0):
        """gera uma frase: uma lista de (inicio, dur, grau)"""
        r = rng(semente)
        frase = []
        atual = comeca if comeca is not None else (faixa[0] + faixa[1]) // 2
        b = ini
        for c_i, g in enumerate(acordes):
            ritmo = ritmos[r.integers(len(ritmos))]
            tons = [g + 2 * k for k in range(3)]
            ultimo_compasso = (c_i == len(acordes) - 1)
            if ultimo_compasso and cadencia:
                ritmo = [self.compasso] if r.random() < 0.5 else [self.compasso - 2, 2] if self.compasso == 4 else [1, 2]
            pos = 0
            for j, d in enumerate(ritmo):
                forte = (pos == 0) or (self.compasso == 4 and pos == 2)
                if ultimo_compasso and cadencia and j == len(ritmo) - 1:
                    # termina numa nota do acorde (de preferencia a fundamental)
                    alvo = [t + 7 * k for t in (tons[0], tons[0], tons[1]) for k in range(-2, 4)]
                elif forte:
                    alvo = [t + 7 * k for t in tons for k in range(-2, 4)]
                else:
                    alvo = [atual + s for s in (-2, -1, -1, 1, 1, 2, 0, 3, -3)]
                alvo = [a for a in alvo if faixa[0] <= a <= faixa[1]] or [atual]
                if forte or (ultimo_compasso and cadencia):
                    alvo.sort(key=lambda a: abs(a - atual) + r.random() * 1.5)
                    prox = alvo[0] if r.random() < 0.75 else alvo[min(1, len(alvo) - 1)]
                else:
                    prox = alvo[r.integers(len(alvo))]
                if pausa and not forte and r.random() < pausa:
                    pos += d; continue
                frase.append((b + pos, d, prox))
                atual = prox
                pos += d
            b += self.compasso
        return frase

    def tocar_frase(self, trilha, frase, desloc=0, oitava=0, vel=0.8, transp=0):
        for ini, d, g in frase:
            self.nota(trilha, ini + desloc, d, self.grau(g + transp, oitava), vel)

    # ---------------------------------------------------------- acompanhamento
    def arpejo(self, trilha, acordes, ini, padrao=(0, 2, 4, 7, 4, 2, 0, 2), passo=0.5, oitava=0, vel=0.5):
        b = ini
        for g in acordes:
            k = 0
            while k * passo < self.compasso - 1e-6:
                p = padrao[k % len(padrao)]
                self.nota(trilha, b + k * passo, passo * 2, self.grau(g + p, oitava), vel)
                k += 1
            b += self.compasso

    def blocos(self, trilha, acordes, ini, oitava=0, vel=0.4, notas=3):
        b = ini
        for g in acordes:
            for x in self.acorde(g, notas):
                self.nota(trilha, b, self.compasso, self.grau(x, oitava), vel)
            b += self.compasso

    def valsa(self, trilha_baixo, trilha_acorde, acordes, ini, vel=0.5):
        b = ini
        for g in acordes:
            self.nota(trilha_baixo, b, 1, self.grau(g, -2), vel)
            for k in (1, 2):
                for x in self.acorde(g):
                    self.nota(trilha_acorde, b + k, 0.9, self.grau(x, 0), vel * 0.45)
            b += self.compasso

    def baixo(self, trilha, acordes, ini, padrao=((0, 2), (2, 2)), oitava=-2, vel=0.7):
        """padrao: lista de (batida, dur) com a fundamental; 'q' = quinta"""
        b = ini
        for g in acordes:
            for item in padrao:
                bat, d = item[0], item[1]
                inter = item[2] if len(item) > 2 else 0
                self.nota(trilha, b + bat, d, self.grau(g + inter, oitava), vel)
            b += self.compasso

    def bateria(self, padrao, ini, compassos, vel=1.0):
        """padrao: dict letra -> string de passos ('x' forte, 'o' fraco, '.' nada), 16 passos/compasso"""
        for c in range(compassos):
            for letra, s in padrao.items():
                passo = self.compasso / len(s)
                for i, ch in enumerate(s):
                    if ch in 'xo':
                        self.batidas.append((ini + c * self.compasso + i * passo, letra,
                                             vel * (1.0 if ch == 'x' else 0.5)))

    # ---------------------------------------------------------- render
    def render(self, total_batidas, rev=(1.6, 0.25), cauda=2.0, laco=False):
        n = int((self.seg(total_batidas) + cauda) * SR)
        out = np.zeros(n)
        for trilha, evs in self.eventos.items():
            instr = INSTR[trilha.split(':')[0]]
            buf = np.zeros(n)
            cache = {}
            for ini, d, m, vel in evs:
                chave = (m, round(d, 3))
                if chave not in cache:
                    cache[chave] = instr(nota(m), self.seg(d), 1.0)
                x = cache[chave] * vel
                i = int(self.seg(ini) * SR)
                if i >= n: continue
                buf[i:i + len(x)] += x[:n - i]
            out += buf * self.volumes.get(trilha, 1.0)
        rbuf = np.zeros(n)
        r = rng(3)
        for ini, letra, vel in self.batidas:
            x = BATERIA[letra](vel, int(r.integers(1 << 30)))
            i = int(self.seg(ini) * SR)
            if i >= n: continue
            rbuf[i:i + len(x)] += x[:n - i]
        out += rbuf * self.volumes.get('bateria', 0.8)
        out = reverb(out, rev[0], rev[1])[:n]
        if laco:
            # para tocar em laco (batalha): o rabo do reverb volta para o comeco
            fim = int(self.seg(total_batidas) * SR)
            rabo = out[fim:]
            out = out[:fim].copy()
            out[:len(rabo)] += rabo
            out = fade(out, 0.002, 0.01)
        else:
            out = fade(out, 0.01, 1.5)
        return np.tanh(normalizar(out, 0.95) * 1.1) * 0.85


# ===================================================================== AS MUSICAS
CALMO_4 = [[2, 1, 1], [1, 1, 2], [1.5, 0.5, 2], [1, 0.5, 0.5, 2], [3, 1], [2, 2], [1, 1, 1, 1]]
CALMO_3 = [[2, 1], [1, 1, 1], [1.5, 0.5, 1], [3], [1, 2]]
ANIMADO = [[1, 0.5, 0.5, 1, 1], [0.5, 0.5, 1, 0.5, 0.5, 1], [1.5, 0.5, 1, 1], [1, 1, 0.5, 0.5, 1],
           [0.5, 0.5, 0.5, 0.5, 1, 1], [2, 1, 1]]


def m_dia_1():
    """Manha no Campo - Do maior, piano e violao"""
    m = Musica(84, 'C4', 'maior')
    A = [0, 4, 5, 3]          # I V vi IV
    A2 = [0, 4, 3, 0]
    B = [5, 3, 0, 4]          # vi IV I V
    fA = m.melodia(A, 0, 11, CALMO_4, (7, 14))
    fA2 = m.melodia(A2, 0, 12, CALMO_4, (7, 14), comeca=fA[-1][2])
    fB = m.melodia(B, 0, 13, CALMO_4, (9, 16))
    sec = [(A, fA), (A2, fA2), (B, fB), (A2, fA2)]
    b = 0
    for volta in range(2):
        for acs, fr in sec:
            m.tocar_frase('piano' if volta == 0 else 'flauta', fr, b, 0, 0.75 if volta == 0 else 0.55)
            m.arpejo('dedilhado', acs, b, oitava=-1, vel=0.35)
            m.baixo('baixo', acs, b, ((0, 3), (3, 1, 4)), vel=0.5)
            if volta == 1:
                m.blocos('pad', acs, b, -1, 0.18)
            b += 16
    m.volumes = {'piano': 1.0, 'flauta': 0.9, 'dedilhado': 0.8, 'baixo': 0.7, 'pad': 0.6}
    return m.render(b, (1.8, 0.28))


def m_dia_2():
    """Riacho - Fa maior, kalimba e piano"""
    m = Musica(96, 'F4', 'maior')
    A = [0, 2, 3, 4]          # I iii IV V
    B = [5, 3, 0, 4]
    C = [1, 4, 0, 0]          # ii V I I
    fA = m.melodia(A, 0, 21, ANIMADO[:4] + CALMO_4[:3], (7, 14))
    fB = m.melodia(B, 0, 22, ANIMADO[:4] + CALMO_4[:3], (8, 15))
    fC = m.melodia(C, 0, 23, CALMO_4, (7, 14))
    b = 0
    for acs, fr, ins in [(A, fA, 'kalimba'), (B, fB, 'kalimba'), (A, fA, 'piano'), (C, fC, 'kalimba'),
                         (B, fB, 'piano'), (A, fA, 'kalimba'), (C, fC, 'piano')]:
        m.tocar_frase(ins, fr, b, 0 if ins == 'piano' else 1, 0.7)
        m.arpejo('piano:acomp', acs, b, padrao=(0, 4, 7, 4), passo=1, oitava=-1, vel=0.3)
        m.baixo('baixo', acs, b, ((0, 2), (2, 1.5, 4)), vel=0.45)
        m.bateria({'C': 'o.o.x.o.o.o.x.o.'}, b, 4, 0.25)
        b += 16
    m.volumes = {'kalimba': 1.0, 'piano': 0.9, 'piano:acomp': 0.7, 'baixo': 0.7, 'bateria': 0.5}
    return m.render(b, (1.5, 0.25))


def m_dia_3():
    """Colinas - Sol maior, valsa com caixinha de musica"""
    m = Musica(108, 'G4', 'maior', compasso=3)
    A = [0, 3, 4, 0, 5, 1, 4, 4]
    B = [3, 0, 3, 4, 2, 5, 1, 4]
    A2 = [0, 3, 4, 0, 5, 3, 4, 0]
    fA = m.melodia(A, 0, 31, CALMO_3, (7, 15), cadencia=False)
    fB = m.melodia(B, 0, 32, CALMO_3, (9, 16), cadencia=False)
    fA2 = m.melodia(A2, 0, 33, CALMO_3, (7, 15))
    b = 0
    for volta in range(2):
        for acs, fr in [(A, fA), (A2, fA2), (B, fB), (A2, fA2)]:
            m.tocar_frase('caixinha' if volta == 0 else 'flauta', fr, b, 1 if volta == 0 else 0, 0.7)
            m.valsa('baixo', 'piano', acs, b, 0.6)
            b += 3 * len(acs)
    m.volumes = {'caixinha': 0.9, 'flauta': 0.8, 'piano': 0.6, 'baixo': 0.7}
    return m.render(b, (1.8, 0.3))


def m_noite_1():
    """Ceu Estrelado - La menor, pad e sinos espacados"""
    m = Musica(66, 'A3', 'menor')
    A = [0, 5, 2, 6]           # i VI III VII
    B = [3, 0, 5, 4]           # iv i VI v
    fA = m.melodia(A, 0, 41, [[3, 1], [2, 2], [4], [1, 3]], (9, 16), pausa=0.3)
    fB = m.melodia(B, 0, 42, [[3, 1], [2, 2], [4], [2, 1, 1]], (10, 17), pausa=0.3)
    b = 0
    for acs, fr in [(A, fA), (A, fA), (B, fB), (A, fA), (B, fB), (A, fA)]:
        m.tocar_frase('sino', fr, b, 0, 0.55)
        m.blocos('pad', acs, b, 0, 0.3)
        m.baixo('baixo', acs, b, ((0, 4),), oitava=-1, vel=0.4)
        b += 16
    m.volumes = {'sino': 0.9, 'pad': 0.9, 'baixo': 0.8}
    return m.render(b, (2.8, 0.4), cauda=3.0)


def m_noite_2():
    """Vagalumes - Mi menor, violao e flauta"""
    m = Musica(72, 'E4', 'dorico')
    A = [0, 3, 0, 4]           # i IV i v
    B = [5, 3, 6, 4]
    fA = m.melodia(A, 0, 51, CALMO_4, (7, 14), pausa=0.15)
    fB = m.melodia(B, 0, 52, CALMO_4, (8, 15), pausa=0.15)
    b = 0
    for acs, fr, ins in [(A, fA, 'flauta'), (A, fA, 'flauta'), (B, fB, 'piano'), (A, fA, 'flauta'),
                         (B, fB, 'flauta'), (A, fA, 'piano')]:
        m.tocar_frase(ins, fr, b, 0, 0.55)
        m.arpejo('dedilhado', acs, b, padrao=(0, 4, 7, 9, 7, 4), passo=0.5, oitava=-1, vel=0.35)
        b += 16
    m.volumes = {'flauta': 0.9, 'piano': 0.85, 'dedilhado': 0.9}
    return m.render(b, (2.2, 0.35))


def m_nether():
    """Brasas - Re frigio, drone, tambores e sino sombrio"""
    m = Musica(70, 'D3', 'frigio')
    A = [0, 1, 0, 6]           # i bII i bVII
    B = [5, 1, 3, 0]
    fA = m.melodia(A, 0, 61, [[3, 1], [2, 2], [4], [1, 1, 2]], (7, 13), pausa=0.35)
    fB = m.melodia(B, 0, 62, [[3, 1], [2, 2], [4]], (8, 14), pausa=0.35)
    b = 0
    for acs, fr in [(A, fA), (A, fA), (B, fB), (A, fA), (B, fB), (A, fA)]:
        m.nota('drone', b, 16, m.grau(0, -1), 0.6)
        m.blocos('pad_escuro', acs, b, 0, 0.3)
        m.tocar_frase('sino', fr, b, 0, 0.45)
        m.bateria({'T': 'x.......o...o...'}, b, 4, 0.8)
        b += 16
    m.volumes = {'drone': 0.9, 'pad_escuro': 0.8, 'sino': 0.7, 'bateria': 0.8}
    return m.render(b, (2.5, 0.35), cauda=3.0)


def m_fim():
    """Vazio - Si menor/lidio, pads flutuando e sinos altos"""
    m = Musica(60, 'B3', 'lidio')
    A = [0, 1, 0, 4]
    B = [5, 1, 2, 0]
    fA = m.melodia(A, 0, 71, [[4], [3, 1], [2, 2]], (11, 18), pausa=0.4)
    fB = m.melodia(B, 0, 72, [[4], [2, 2], [1, 3]], (12, 19), pausa=0.4)
    b = 0
    for acs, fr in [(A, fA), (B, fB), (A, fA), (B, fB), (A, fA)]:
        m.blocos('pad', acs, b, -1, 0.3, notas=4)
        m.tocar_frase('sino', fr, b, 0, 0.4)
        m.nota('drone', b, 16, m.grau(0, -1), 0.35)
        b += 16
    m.volumes = {'pad': 0.9, 'sino': 0.8, 'drone': 0.6}
    return m.render(b, (3.5, 0.45), cauda=3.5)


def m_batalha_1():
    """Batalha de Criaturas - La menor, chiptune animado (em laco)"""
    m = Musica(150, 'A4', 'menor')
    A = [0, 5, 6, 0]           # i VI VII i
    B = [3, 0, 6, 2]           # iv i VII III
    C = [5, 6, 4, 4]           # VI VII v v
    fA = m.melodia(A, 0, 81, ANIMADO, (5, 12), cadencia=False)
    fB = m.melodia(B, 0, 82, ANIMADO, (7, 14), cadencia=False)
    fC = m.melodia(C, 0, 83, ANIMADO, (7, 14), cadencia=False)
    b = 0
    for acs, fr in [(A, fA), (A, fA), (B, fB), (C, fC)]:
        m.tocar_frase('chip', fr, b, 0, 0.6)
        m.tocar_frase('chip:eco', fr, b + 0.25, 0, 0.18)
        m.baixo('chip_baixo', acs, b, tuple((k * 0.5, 0.5, 7 if k % 2 else 0) for k in range(8)), oitava=-2, vel=0.6)
        m.arpejo('chip:arp', acs, b, padrao=(0, 2, 4, 7), passo=0.25, oitava=-1, vel=0.12)
        m.bateria({'K': 'x...x...x...x...', 'S': '....x.......x...', 'H': 'o.o.o.o.o.o.o.oo'}, b, 4, 0.9)
        b += 16
    m.volumes = {'chip': 1.0, 'chip:eco': 0.8, 'chip_baixo': 0.9, 'chip:arp': 0.6, 'bateria': 0.8}
    return m.render(b, (1.0, 0.15), cauda=1.0, laco=True)


def m_batalha_2():
    """Duelo do Ginasio - Do menor harmonico (em laco)"""
    m = Musica(140, 'C4', 'harmonica')
    A = [0, 3, 4, 0]           # i iv V i
    B = [5, 1, 4, 4]           # VI ii V V
    fA = m.melodia(A, 0, 91, ANIMADO, (7, 14), cadencia=False)
    fB = m.melodia(B, 0, 92, ANIMADO, (8, 15), cadencia=False)
    b = 0
    for acs, fr in [(A, fA), (B, fB), (A, fA), (B, fB)]:
        m.tocar_frase('lead_forte', fr, b, 0, 0.45)
        m.baixo('chip_baixo', acs, b, tuple((k * 0.5, 0.5, 0) for k in range(8)), oitava=-2, vel=0.6)
        m.blocos('pad', acs, b, -1, 0.15)
        m.bateria({'K': 'x.....x.x.......', 'S': '....x.......x..o', 'H': 'x.x.x.x.x.x.x.x.'}, b, 4, 0.9)
        b += 16
    m.volumes = {'lead_forte': 0.9, 'chip_baixo': 0.9, 'pad': 0.6, 'bateria': 0.85}
    return m.render(b, (1.0, 0.15), cauda=1.0, laco=True)


def m_chefao():
    """Chefao - Mi menor, pesado e rapido (em laco)"""
    m = Musica(160, 'E4', 'harmonica')
    A = [0, 0, 5, 4]           # i i VI V
    B = [3, 5, 1, 4]           # iv VI ii V
    fA = m.melodia(A, 0, 101, ANIMADO, (5, 12), cadencia=False)
    fB = m.melodia(B, 0, 102, ANIMADO, (7, 14), cadencia=False)
    b = 0
    for acs, fr in [(A, fA), (A, fA), (B, fB), (B, fB)]:
        m.tocar_frase('lead_forte', fr, b, 0, 0.5)
        m.baixo('baixo_sujo', acs, b, tuple((k * 0.5, 0.5, 0 if k % 4 != 3 else 7) for k in range(8)), oitava=-2, vel=0.7)
        m.blocos('pad_escuro', acs, b, -1, 0.25)
        m.bateria({'K': 'x.x...x.x.x...x.', 'S': '....x.......x...', 'H': 'xxxxxxxxxxxxxxxx'}, b, 4, 1.0)
        b += 16
    m.volumes = {'lead_forte': 0.9, 'baixo_sujo': 0.8, 'pad_escuro': 0.7, 'bateria': 0.9}
    return m.render(b, (0.9, 0.12), cauda=1.0, laco=True)


MUSICAS = {
    'dia_1': m_dia_1, 'dia_2': m_dia_2, 'dia_3': m_dia_3,
    'noite_1': m_noite_1, 'noite_2': m_noite_2,
    'nether_1': m_nether, 'fim_1': m_fim,
    'batalha_1': m_batalha_1, 'batalha_2': m_batalha_2, 'chefao_1': m_chefao,
}


def salvar_ogg(nome, x):
    os.makedirs(SAIDA, exist_ok=True)
    wav = os.path.join(SAIDA, nome + '.wav')
    ogg = os.path.join(SAIDA, nome + '.ogg')
    salvar_wav(wav, x, pico=None, alinhar=False)
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-c:a', 'libvorbis', '-q:a', '2',
                    '-ar', str(SR), '-ac', '1', ogg], check=True)
    os.remove(wav)
    return os.path.getsize(ogg)


def main(nomes):
    for nome in nomes or MUSICAS:
        x = MUSICAS[nome]()
        kb = salvar_ogg(nome, x) // 1024
        print('%-10s %5.1f s  %4d KB' % (nome, len(x) / SR, kb))


if __name__ == '__main__':
    main(sys.argv[1:])
