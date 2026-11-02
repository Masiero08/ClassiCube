# Pequena biblioteca de sintese de som (so numpy) para os sons e musicas
# proprios do ClassiCube 3DS. Tudo e gerado do zero: nenhum som gravado.
import numpy as np
import wave, struct

SR = 22050  # taxa de amostragem (boa para o 3DS e leve na memoria)


def t_(dur):
    return np.arange(int(dur * SR)) / SR


def rng(seed):
    return np.random.default_rng(seed)


# ---------------------------------------------------------------- envelopes
def env_exp(n, decay):
    """decaimento exponencial; decay = tempo (s) para cair ~63%"""
    return np.exp(-np.arange(n) / (decay * SR))


def env_adsr(n, a=0.01, d=0.05, s=0.7, r=0.1):
    e = np.ones(n) * s
    na, nd, nr = int(a * SR), int(d * SR), int(r * SR)
    na = max(1, min(na, n)); nd = min(nd, max(0, n - na)); nr = min(nr, n)
    e[:na] = np.linspace(0, 1, na)
    e[na:na + nd] = np.linspace(1, s, nd) if nd else e[na:na + nd]
    if nr:
        e[n - nr:] *= np.linspace(1, 0, nr)
    return e


def fade(x, a=0.003, r=0.01):
    x = x.copy()
    na, nr = int(a * SR), int(r * SR)
    if na: x[:na] *= np.linspace(0, 1, na)
    if nr: x[-nr:] *= np.linspace(1, 0, nr)
    return x


# ---------------------------------------------------------------- filtros (FFT)
def _band_gain(freqs, lo, hi, slope):
    g = np.ones_like(freqs)
    if lo:
        g *= 1.0 / (1.0 + (lo / np.maximum(freqs, 1e-3)) ** slope)
    if hi:
        g *= 1.0 / (1.0 + (freqs / hi) ** slope)
    return g


def filtro(x, lo=None, hi=None, slope=4):
    n = len(x)
    N = 1 << int(np.ceil(np.log2(max(n, 2) * 2)))
    X = np.fft.rfft(x, N)
    f = np.fft.rfftfreq(N, 1 / SR)
    X *= _band_gain(f, lo, hi, slope)
    return np.fft.irfft(X, N)[:n]


def ressonante(x, freq, q=8.0):
    """pico estreito em freq (formante)"""
    n = len(x)
    N = 1 << int(np.ceil(np.log2(max(n, 2) * 2)))
    X = np.fft.rfft(x, N)
    f = np.fft.rfftfreq(N, 1 / SR)
    bw = freq / q
    X *= 1.0 / (1.0 + ((f - freq) / bw) ** 2)
    return np.fft.irfft(X, N)[:n]


def formantes(x, fs, qs=None, ganhos=None):
    out = np.zeros_like(x)
    for i, f in enumerate(fs):
        q = qs[i] if qs else 6.0
        g = ganhos[i] if ganhos else 1.0
        out += ressonante(x, f, q) * g
    return out


def lowpass_var(x, cut_ini, cut_fim):
    """filtro passa-baixa de 1 polo com corte que muda no tempo (varredura)"""
    n = len(x)
    cuts = np.geomspace(max(cut_ini, 20), max(cut_fim, 20), n)
    a = 1 - np.exp(-2 * np.pi * cuts / SR)
    y = np.zeros(n)
    acc = 0.0
    for i in range(n):
        acc += a[i] * (x[i] - acc)
        y[i] = acc
    return y


# ---------------------------------------------------------------- osciladores
def fase(freq, n):
    """freq pode ser numero ou vetor (glissando)"""
    if np.isscalar(freq):
        freq = np.full(n, float(freq))
    return 2 * np.pi * np.cumsum(freq) / SR


def seno(freq, n):
    return np.sin(fase(freq, n))


def serra(freq, n, harm=None):
    # serra com poucos harmonicos (sem aliasing feio)
    ph = fase(freq, n)
    f0 = freq if np.isscalar(freq) else np.max(freq)
    k = harm or max(1, int((SR / 2.2) / max(f0, 1)))
    k = min(k, 40)
    y = np.zeros(n)
    for h in range(1, k + 1):
        y += np.sin(h * ph) / h
    return y * 0.6


def quadrada(freq, n, harm=None, duty_soft=False):
    ph = fase(freq, n)
    f0 = freq if np.isscalar(freq) else np.max(freq)
    k = harm or max(1, int((SR / 2.2) / max(f0, 1)))
    k = min(k, 31)
    y = np.zeros(n)
    for h in range(1, k + 1, 2):
        y += np.sin(h * ph) / h
    return y * 0.8


def triangulo(freq, n):
    ph = fase(freq, n)
    f0 = freq if np.isscalar(freq) else np.max(freq)
    k = min(15, max(1, int((SR / 2.2) / max(f0, 1))))
    y = np.zeros(n)
    s = 1
    for h in range(1, k + 1, 2):
        y += s * np.sin(h * ph) / (h * h)
        s = -s
    return y * 0.8


def fm(freq, n, razao=2.0, indice=2.0, ind_env=None):
    """modulacao de fase simples (sons de sino, piano eletrico, metal)"""
    if np.isscalar(freq):
        freq = np.full(n, float(freq))
    ph_m = fase(freq * razao, n)
    idx = indice * (ind_env if ind_env is not None else 1.0)
    ph_c = fase(freq, n)
    return np.sin(ph_c + idx * np.sin(ph_m))


def ruido(n, seed=0):
    return rng(seed).uniform(-1, 1, n)


def karplus(freq, dur, seed=0, brilho=0.5, decay=0.996):
    """corda dedilhada (Karplus-Strong)"""
    n = int(dur * SR)
    p = max(2, int(SR / freq))
    buf = rng(seed).uniform(-1, 1, p)
    buf = filtro(buf, hi=SR * (0.1 + 0.35 * brilho)) if p > 8 else buf
    out = np.zeros(n)
    b = buf.copy()
    i = 0
    # processa em blocos do tamanho do periodo (rapido o bastante)
    while i < n:
        m = min(p, n - i)
        out[i:i + m] = b[:m]
        nb = decay * 0.5 * (b + np.roll(b, -1))
        b = nb
        i += m
    return out


# ---------------------------------------------------------------- efeitos
def reverb(x, tempo=1.2, mix=0.25, seed=7, brilho=4000):
    n_ir = int(tempo * SR)
    ir = rng(seed).normal(0, 1, n_ir) * np.exp(-np.arange(n_ir) / (tempo * SR / 6.9))
    ir = filtro(ir, hi=brilho)
    ir[:int(0.012 * SR)] = 0
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-9
    n = len(x) + n_ir
    N = 1 << int(np.ceil(np.log2(n)))
    wet = np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(ir, N), N)[:n]
    out = np.zeros(n)
    out[:len(x)] += x * (1 - mix)
    out += wet * mix
    return out


def eco(x, atraso=0.25, fb=0.35, vezes=4):
    d = int(atraso * SR)
    out = np.zeros(len(x) + d * vezes)
    g = 1.0
    for k in range(vezes + 1):
        out[k * d:k * d + len(x)] += x * g
        g *= fb
    return out


def distorcer(x, k=3.0):
    return np.tanh(x * k) / np.tanh(k)


def normalizar(x, pico=0.9):
    m = np.max(np.abs(x)) + 1e-9
    return x / m * pico


def mix(*partes):
    n = max(len(p) for p in partes)
    out = np.zeros(n)
    for p in partes:
        out[:len(p)] += p
    return out


def em(x, inicio, total=None):
    """coloca x a partir de 'inicio' segundos"""
    i = int(inicio * SR)
    n = total if total else i + len(x)
    out = np.zeros(max(n, i + len(x)))
    out[i:i + len(x)] += x
    return out


def silencio_fim(x, limiar=0.002):
    idx = np.where(np.abs(x) > limiar)[0]
    if not len(idx):
        return x[:1]
    return x[:idx[-1] + 1]


# ---------------------------------------------------------------- arquivos
def salvar_wav(caminho, x, pico=0.9, alinhar=True):
    x = normalizar(x, pico) if pico else x
    x = np.clip(x, -1, 1)
    pcm = (x * 32767).astype('<i2')
    if alinhar:
        # o DSP do 3DS gosta de blocos multiplos de 128 bytes (64 amostras)
        resto = (-len(pcm)) % 64
        pcm = np.concatenate([pcm, np.zeros(resto, dtype='<i2')])
    with wave.open(caminho, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def nota(n):
    """'C4', 'F#3', 'Bb5' ou numero MIDI -> Hz"""
    if isinstance(n, (int, float)):
        return 440.0 * 2 ** ((n - 69) / 12)
    nomes = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
    s = nomes[n[0]]
    i = 1
    while i < len(n) and n[i] in '#b':
        s += 1 if n[i] == '#' else -1
        i += 1
    oit = int(n[i:])
    return 440.0 * 2 ** ((s + 12 * (oit + 1) - 69) / 12)


def midi(n):
    nomes = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
    s = nomes[n[0]]
    i = 1
    while i < len(n) and n[i] in '#b':
        s += 1 if n[i] == '#' else -1
        i += 1
    return s + 12 * (int(n[i:]) + 1)
