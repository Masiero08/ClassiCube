#!/usr/bin/env python3
"""Grava misc/3ds/icon.png e misc/3ds/banner.png dentro de icon.bin (SMDH) e banner.bin (CBMD),
que o makerom usa para montar o .cia. Mantem os textos do icone e o som do banner como estao.

Nao precisa do bannertool: o SMDH tem os icones em posicoes fixas, e o banner e um CBMD com um
CGFX comprimido em LZ11 cujo final e a imagem 256x128 em RGBA4444 (blocos 8x8 em ordem Z).
Uso: python3 misc/3ds/arte/atualizar_bins.py
"""
import os, struct
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
PASTA = os.path.normpath(os.path.join(AQUI, ".."))

def morton(x, y, w):
    return (((y >> 3) * (w >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                                     ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))

def tiles(img, fmt):
    w, h = img.size
    px = img.convert("RGBA").load()
    out = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if fmt == "565":
                f = a / 255.0
                r, g, b = int(r * f), int(g * f), int(b * f)
                c = ((r & ~7) << 8) | ((g & ~3) << 3) | (b >> 3)
            else:
                c = ((r & ~0xF) << 8) | ((g & ~0xF) << 4) | (b & ~0xF) | (a >> 4)
            out[morton(x, y, w)] = c
    return struct.pack("<%dH" % len(out), *out)

# ---------------------------------------------------------------- LZ11
def lz11_decompress(data):
    assert data[0] == 0x11, "nao e LZ11"
    size = data[1] | (data[2] << 8) | (data[3] << 16)
    pos, out = 4, bytearray()
    while len(out) < size:
        flags = data[pos]; pos += 1
        for bit in range(8):
            if len(out) >= size: break
            if flags & (0x80 >> bit):
                b0 = data[pos]; ind = b0 >> 4
                if ind == 0:
                    ln = (((b0 & 0xF) << 4) | (data[pos + 1] >> 4)) + 0x11
                    disp = (((data[pos + 1] & 0xF) << 8) | data[pos + 2]) + 1; pos += 3
                elif ind == 1:
                    ln = (((b0 & 0xF) << 12) | (data[pos + 1] << 4) | (data[pos + 2] >> 4)) + 0x111
                    disp = (((data[pos + 2] & 0xF) << 8) | data[pos + 3]) + 1; pos += 4
                else:
                    ln = ind + 1
                    disp = (((b0 & 0xF) << 8) | data[pos + 1]) + 1; pos += 2
                for _ in range(ln):
                    out.append(out[-disp])
            else:
                out.append(data[pos]); pos += 1
    return bytes(out)

def lz11_compress(src):
    n = len(src)
    out = bytearray([0x11, n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF])
    pos = 0
    cadeia = {}  # 3 bytes -> lista de posicoes recentes
    def lembra(i):
        if i + 3 <= n:
            cadeia.setdefault(src[i:i + 3], []).append(i)
    while pos < n:
        flag_i = len(out); out.append(0); flags = 0
        for bit in range(8):
            if pos >= n: break
            best_len, best_disp = 0, 0
            for cand in reversed(cadeia.get(src[pos:pos + 3], [])[-64:]):
                disp = pos - cand
                if disp > 0x1000: break
                ln, lim = 0, min(n - pos, 0x10110)
                while ln < lim and src[cand + ln] == src[pos + ln]:
                    ln += 1
                if ln > best_len:
                    best_len, best_disp = ln, disp
                    if ln == lim: break
            if best_len >= 3:
                flags |= 0x80 >> bit
                d = best_disp - 1
                if best_len <= 0x10:
                    out += bytes([((best_len - 1) << 4) | (d >> 8), d & 0xFF])
                elif best_len <= 0x110:
                    l = best_len - 0x11
                    out += bytes([l >> 4, ((l & 0xF) << 4) | (d >> 8), d & 0xFF])
                else:
                    l = best_len - 0x111
                    out += bytes([0x10 | (l >> 12), (l >> 4) & 0xFF, ((l & 0xF) << 4) | (d >> 8), d & 0xFF])
                for k in range(best_len):
                    lembra(pos + k)
                pos += best_len
            else:
                out.append(src[pos]); lembra(pos); pos += 1
        out[flag_i] = flags
    return bytes(out)

# ---------------------------------------------------------------- SMDH
TITULO_LONGO = "ClassiCube 3DS com Mods"

def atualiza_icone():
    caminho = os.path.join(PASTA, "icon.bin")
    smdh = bytearray(open(caminho, "rb").read())
    assert smdh[:4] == b"SMDH" and len(smdh) == 0x36C0, "icon.bin inesperado"
    grande = Image.open(os.path.join(PASTA, "icon.png")).convert("RGBA")
    assert grande.size == (48, 48)
    # 24x24: media de cada 2x2, como o bannertool faz
    pequeno = grande.resize((24, 24), Image.BOX)
    smdh[0x2040:0x2040 + 0x480] = tiles(pequeno, "565")
    smdh[0x24C0:0x24C0 + 0x1200] = tiles(grande, "565")
    # nome longo (aparece no menu HOME ao selecionar o jogo), igual em todas as linguas
    longo = TITULO_LONGO.encode("utf-16le")[:0xFE].ljust(0x100, b"\0")
    for lingua in range(16):
        o = 0x08 + lingua * 0x200 + 0x80
        smdh[o:o + 0x100] = longo
    open(caminho, "wb").write(smdh)
    print("icon.bin atualizado")

# ---------------------------------------------------------------- CBMD
CGFX_HEADER = 0x1580

def atualiza_banner():
    caminho = os.path.join(PASTA, "banner.bin")
    bnr = open(caminho, "rb").read()
    assert bnr[:4] == b"CBMD", "banner.bin inesperado"
    offs = struct.unpack_from("<14I", bnr, 0x08)
    cwav_off = struct.unpack_from("<I", bnr, 0x84)[0]
    assert offs[0] == 0x88 and all(o == 0 for o in offs[1:]), "banner com varias linguas: nao suportado"
    cgfx = lz11_decompress(bnr[offs[0]:cwav_off])
    assert len(cgfx) == CGFX_HEADER + 256 * 128 * 2, "CGFX com tamanho inesperado: %d" % len(cgfx)
    img = Image.open(os.path.join(PASTA, "banner.png")).convert("RGBA")
    assert img.size == (256, 128)
    novo = cgfx[:CGFX_HEADER] + tiles(img, "4444")
    comp = lz11_compress(novo)
    assert lz11_decompress(comp) == novo
    header = bytearray(bnr[:0x88])
    fim = 0x88 + len(comp)
    fim = (fim + 0xF) & ~0xF
    struct.pack_into("<I", header, 0x84, fim)
    saida = bytes(header) + comp + bytes(fim - 0x88 - len(comp)) + bnr[cwav_off:]
    open(caminho, "wb").write(saida)
    print("banner.bin atualizado (%d bytes)" % len(saida))

if __name__ == "__main__":
    atualiza_icone()
    atualiza_banner()
