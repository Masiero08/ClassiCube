# Generates the "Remaster" texture pack: original 16x16 pixel art for the classic blocks,
# with richer shading than the default pack. Only the tiles listed here are redrawn; every
# other tile of misc/cc_textures.zip (including the empty ones that the mods draw into) is kept.
#
#   python3 misc/remaster/gen_pack.py            -> misc/remaster/remaster.zip
#   python3 misc/remaster/gen_pack.py preview.png  (also writes an enlarged preview)
import io, math, os, random, sys, zipfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, "..", "cc_textures.zip")
OUT  = os.path.join(HERE, "remaster.zip")
S = 16

# ---------------------------------------------------------------- helpers
def hexc(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)

def ramp(*hexes):
    return [hexc(h) for h in hexes]

def pick(pal, t):
    """Picks a colour from a dark->light palette for t in 0..1"""
    t = min(max(t, 0.0), 0.999)
    return pal[int(t * len(pal))]

def shade(c, f):
    return tuple(min(255, max(0, int(v * f))) for v in c[:3]) + (c[3],)

class Noise:
    """Tileable value noise on a 16x16 grid"""
    def __init__(self, seed, cells):
        r = random.Random(seed)
        self.n = cells
        self.v = [[r.random() for _ in range(cells)] for _ in range(cells)]
    def at(self, x, y):
        f = self.n / S
        gx, gy = x * f, y * f
        x0, y0 = int(gx) % self.n, int(gy) % self.n
        x1, y1 = (x0 + 1) % self.n, (y0 + 1) % self.n
        tx, ty = gx - int(gx), gy - int(gy)
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
        a = self.v[y0][x0] * (1 - tx) + self.v[y0][x1] * tx
        b = self.v[y1][x0] * (1 - tx) + self.v[y1][x1] * tx
        return a * (1 - ty) + b * ty

def fbm(seed, x, y, octaves=((4, 0.6), (8, 0.3), (16, 0.1))):
    return sum(Noise(seed + i, n).at(x, y) * w for i, (n, w) in enumerate(octaves))

_noise_cache = {}
def nz(seed, n):
    if (seed, n) not in _noise_cache: _noise_cache[(seed, n)] = Noise(seed, n)
    return _noise_cache[(seed, n)]

def field(seed, octaves=((4, 0.6), (8, 0.3), (16, 0.1))):
    return [[sum(nz(seed + i, n).at(x, y) * w for i, (n, w) in enumerate(octaves)) for x in range(S)] for y in range(S)]

def new():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))

def voronoi(seed, count, jitter=1.0):
    """Returns (cell id, distance to edge-ish) per pixel for tileable cells"""
    r = random.Random(seed)
    pts = [(r.uniform(0, S), r.uniform(0, S)) for _ in range(count)]
    ids = [[0] * S for _ in range(S)]
    edge = [[0.0] * S for _ in range(S)]
    for y in range(S):
        for x in range(S):
            ds = []
            for i, (px, py) in enumerate(pts):
                dx = min(abs(x + 0.5 - px), S - abs(x + 0.5 - px))
                dy = min(abs(y + 0.5 - py), S - abs(y + 0.5 - py))
                ds.append((dx * dx + dy * dy, i))
            ds.sort()
            ids[y][x] = ds[0][1]
            edge[y][x] = math.sqrt(ds[1][0]) - math.sqrt(ds[0][0])
    return ids, edge, pts

# ---------------------------------------------------------------- palettes
GRASS  = ramp("3f6b1f", "4f8226", "5e962c", "6ea835", "82bb43", "97cc55")
DIRT   = ramp("4a3020", "5c3d28", "6e4b31", "7d583a", "8e6845", "a07a55")
STONE  = ramp("5d5d61", "6b6b70", "78787d", "86868a", "939397", "a3a3a6")
COBBLE = ramp("3e3e42", "55555a", "6a6a70", "7e7e84", "93939a", "a8a8ae")
SAND   = ramp("c9b27a", "d6c089", "dfcb95", "e8d6a3", "efdfb1", "f5e8c3")
GRAVEL = ramp("514b48", "675f5a", "7b726c", "8f8680", "a39a93", "b8b0a8")
PLANK  = ramp("6b4a26", "7f5a30", "936b3a", "a57b45", "b68c52", "c49b60")
BARK   = ramp("3a2a1a", "4a3622", "5a432b", "6a5034", "7a5e3e")
RINGS  = ramp("8c6a3e", "a07c4a", "b38f5a", "c4a26c")
LEAVES = ramp("2a5e1c", "357326", "428a30", "52a03a", "66b546", "7cc757")
BRICK  = ramp("7a2e22", "8f3a2b", "a24634", "b2553f", "c0654c")
MORTAR = ramp("8f8a84", "a19c95", "b3aea7")
OBSID  = ramp("0e0a18", "17112a", "211838", "2c2048", "3a2c5c", "54428a")
BEDROCK= ramp("1c1c1c", "2e2e2e", "424242", "575757", "6e6e6e", "8a8a8a")
SNDSTN = ramp("bfa36a", "ccb077", "d8bd85", "e2c992", "ead3a0")
IRON   = ramp("8d8d92", "a4a4a9", "bcbcc0", "d2d2d5", "e6e6e8", "f6f6f7")
GOLD   = ramp("a3700f", "c48a14", "dba320", "eebd32", "f8d652", "fff08a")
MOSS   = ramp("2f5a1f", "3f7128", "538a33")

# ---------------------------------------------------------------- tiles
def t_grass_top(seed=1):
    im = new(); px = im.load(); f = field(seed); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            v = 0.3 + f[y][x] * 0.45 + r.random() * 0.2
            px[x, y] = pick(GRASS, v)
    # little blades: a light pixel above a darker one
    for _ in range(18):
        x, y = r.randrange(S), r.randrange(S)
        px[x, y] = GRASS[4]; px[x, (y + 1) % S] = GRASS[2]
    return im

def t_dirt(seed=2):
    im = new(); px = im.load(); f = field(seed); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            px[x, y] = pick(DIRT, f[y][x] * 0.75 + r.random() * 0.3 - 0.05)
    # pebbles and roots
    for _ in range(7):
        x, y = r.randrange(S), r.randrange(S)
        px[x, y] = DIRT[5]; px[(x + 1) % S, y] = DIRT[4]; px[x, (y + 1) % S] = DIRT[1]
    return im

def t_grass_side(seed=3):
    im = t_dirt(seed + 10); px = im.load(); top = t_grass_top(seed + 20).load()
    r = random.Random(seed)
    for x in range(S):
        depth = 3 + (1 if r.random() < 0.55 else 0) + (2 if r.random() < 0.18 else 0)
        for y in range(depth):
            px[x, y] = top[x, y]
        px[x, depth] = shade(GRASS[0], 0.85)   # shadow line under the grass
    return im

def t_stone(seed=4):
    im = new(); px = im.load(); f = field(seed, ((2, 0.45), (4, 0.35), (16, 0.2))); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            px[x, y] = pick(STONE, f[y][x] * 0.85 + r.random() * 0.15)
    # a few soft cracks
    for _ in range(3):
        x, y = r.randrange(S), r.randrange(S)
        for _ in range(r.randint(3, 6)):
            px[x % S, y % S] = STONE[0]
            px[(x + 1) % S, y % S] = STONE[1]
            x += r.choice((1, 1, 0)); y += r.choice((1, 0, -1))
    return im

def stones(seed, count, pal, mortar, rough=0.2):
    im = new(); px = im.load(); ids, edge, pts = voronoi(seed, count); r = random.Random(seed)
    tone = [r.uniform(-0.15, 0.15) for _ in pts]
    for y in range(S):
        for x in range(S):
            e = edge[y][x]
            if e < 0.9:
                px[x, y] = mortar
                continue
            cx, cy = pts[ids[y][x]]
            # light from the top-left: brighter on that side of each stone
            dx = ((x + 0.5 - cx + S / 2) % S) - S / 2
            dy = ((y + 0.5 - cy + S / 2) % S) - S / 2
            light = 0.55 - (dx + dy) * 0.06 + tone[ids[y][x]] + r.uniform(-rough, rough) * 0.5
            if e < 1.6: light -= 0.18
            px[x, y] = pick(pal, light)
    return im

def t_cobble():   return stones(5, 9, COBBLE, COBBLE[0])
def t_bedrock():  return stones(6, 14, BEDROCK, BEDROCK[0], 0.6)
def t_mossy():
    im = t_cobble(); px = im.load(); f = field(55, ((4, 0.7), (16, 0.3)))
    for y in range(S):
        for x in range(S):
            if f[y][x] > 0.55: px[x, y] = pick(MOSS, (f[y][x] - 0.55) * 3)
    return im

def t_gravel(seed=7):
    im = new(); px = im.load(); ids, edge, pts = voronoi(seed, 22); r = random.Random(seed)
    tones = [r.uniform(0.15, 0.95) for _ in pts]
    for y in range(S):
        for x in range(S):
            v = tones[ids[y][x]]
            if edge[y][x] < 0.45: v = 0.1
            elif edge[y][x] < 1.1: v -= 0.12
            px[x, y] = pick(GRAVEL, v + r.uniform(-0.05, 0.05))
    return im

def t_sand(seed=8):
    im = new(); px = im.load(); f = field(seed, ((4, 0.5), (16, 0.5))); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            px[x, y] = pick(SAND, f[y][x] * 0.7 + r.random() * 0.3)
    return im

def t_planks(seed=9):
    im = new(); px = im.load(); r = random.Random(seed)
    for board in range(4):
        y0 = board * 4; off = r.randrange(S); tone = r.uniform(-0.1, 0.1)
        g = Noise(seed + board, 8)
        for y in range(y0, y0 + 4):
            for x in range(S):
                grain = g.at((x + off) % S, y * 0.3)
                v = 0.55 + tone + (grain - 0.5) * 0.5
                if y == y0: v += 0.15                      # lit top edge
                if y == y0 + 3: v = 0.02                   # seam
                px[x, y] = pick(PLANK, v)
        # board ends with a nail on each
        cut = (off + 7) % S
        for y in range(y0, y0 + 3): px[cut, y] = PLANK[0]
        px[(cut + 2) % S, y0 + 1] = shade(PLANK[0], 0.8)
    return im

def t_log_side(seed=10):
    im = new(); px = im.load(); r = random.Random(seed); g = Noise(seed, 4)
    for y in range(S):
        for x in range(S):
            px[x, y] = pick(BARK, 0.45 + (g.at(x, y) - 0.5) * 0.5 + r.uniform(-0.1, 0.1))
    # wavy vertical grooves with a lit ridge beside each one
    for i, gx in enumerate((1, 5, 9, 12)):
        phase = r.uniform(0, 6.28)
        for y in range(S):
            x = (gx + int(round(math.sin(y * 0.39 + phase) * 0.8))) % S
            if r.random() < 0.85: px[x, y] = BARK[0]
            px[(x - 1) % S, y] = BARK[3] if r.random() < 0.7 else BARK[4]
    return im

def t_log_top(seed=11):
    im = new(); px = im.load(); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            d = math.hypot(x - 7.5, y - 7.5)
            if d > 6.6 or x in (0, 15) or y in (0, 15):
                px[x, y] = pick(BARK, 0.3 + r.random() * 0.5)
                continue
            ring = (d + r.uniform(-0.3, 0.3)) % 2.0
            px[x, y] = RINGS[0] if ring < 0.55 else pick(RINGS, 0.4 + (1 - d / 7) * 0.6)
    return im

def t_leaves(seed=12):
    im = new(); px = im.load(); f = field(seed, ((4, 0.4), (8, 0.3), (16, 0.3))); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            v = f[y][x]
            if v < 0.36 and r.random() < 0.75:
                continue                                   # holes let the sky through
            c = pick(LEAVES, v * 1.2 + r.uniform(-0.15, 0.15) - 0.1)
            px[x, y] = c
    for _ in range(10):
        x, y = r.randrange(S), r.randrange(S)
        if px[x, y][3]: px[x, y] = LEAVES[5]
    return im

def t_ore(seed, ore_pal, blobs=6):
    im = t_stone(seed); px = im.load(); r = random.Random(seed)
    shapes = [((0, 0), (1, 0), (0, 1), (1, 1)), ((0, 0), (1, 0), (2, 0), (1, 1)), ((1, 0), (0, 1), (1, 1), (2, 1), (1, 2)),
              ((0, 0), (1, 0), (1, 1)), ((0, 0), (0, 1), (1, 1), (2, 1))]
    for _ in range(blobs):
        cx, cy = r.randint(0, 15), r.randint(0, 15)
        shape = r.choice(shapes)
        cells = set(((cx + dx) % S, (cy + dy) % S) for dx, dy in shape)
        for (x, y) in cells:                               # dark rim below and right of each nugget
            for (ox, oy) in ((1, 0), (0, 1)):
                n = ((x + ox) % S, (y + oy) % S)
                if n not in cells: px[n] = shade(STONE[0], 0.8)
        for (x, y) in cells:
            px[x, y] = ore_pal[1]
        x0, y0 = min(cells)
        px[x0, y0] = ore_pal[3] if r.random() < 0.7 else ore_pal[2]
        px[max(cells)] = ore_pal[0]
    return im

def t_bricks(seed=13):
    im = new(); px = im.load(); r = random.Random(seed)
    for y in range(S):
        row = y // 4; off = 4 if row % 2 else 0
        for x in range(S):
            if y % 4 == 3 or (x + off) % 8 == 7:
                px[x, y] = pick(MORTAR, 0.3 + r.random() * 0.6)
                continue
            brick = ((x + off) // 8, row)
            tone = random.Random(hash(brick) + seed).uniform(-0.15, 0.15)
            v = 0.55 + tone + r.uniform(-0.12, 0.12)
            if y % 4 == 0: v += 0.2
            if (x + off) % 8 == 0: v += 0.1
            px[x, y] = pick(BRICK, v)
    return im

def t_stone_bricks(seed=14):
    im = new(); px = im.load(); f = field(seed); r = random.Random(seed)
    for y in range(S):
        row = y // 8; off = 4 if row % 2 else 0
        for x in range(S):
            if y % 8 == 7 or (x + off) % 8 == 7:
                px[x, y] = STONE[0]; continue
            v = f[y][x] * 0.6 + 0.2 + r.uniform(-0.08, 0.08)
            if y % 8 == 0 or (x + off) % 8 == 0: v += 0.2
            if y % 8 == 6 or (x + off) % 8 == 6: v -= 0.15
            px[x, y] = pick(STONE, v)
    return im

def t_obsidian(seed=15):
    im = new(); px = im.load(); f = field(seed, ((4, 0.5), (8, 0.5))); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            v = f[y][x] * 0.7 + r.random() * 0.15
            px[x, y] = pick(OBSID, v - 0.1)
    for _ in range(5):                                     # purple glints
        x, y = r.randrange(S), r.randrange(S)
        px[x, y] = OBSID[5]; px[(x + 1) % S, y] = OBSID[4]
    return im

def t_metal_block(pal, seed, side):
    im = new(); px = im.load(); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            v = 0.55 + r.uniform(-0.06, 0.06)
            if x == 0 or y == 0: v = 0.92
            elif x == 15 or y == 15: v = 0.12
            elif x == 1 or y == 1: v = 0.75
            elif x == 14 or y == 14: v = 0.3
            if side and y == 8: v = 0.25
            if side and y == 7: v = 0.8
            px[x, y] = pick(pal, v)
    for i in range(3, 7):                                  # shine
        px[i, 3 + 6 - i] = pal[-1]
    return im

def t_sandstone(seed, kind):
    im = new(); px = im.load(); f = field(seed, ((4, 0.5), (16, 0.5))); r = random.Random(seed)
    for y in range(S):
        for x in range(S):
            v = f[y][x] * 0.5 + 0.3 + r.uniform(-0.08, 0.08)
            if kind == "side":
                if y in (0, 1): v = 0.85
                if y in (4, 11) : v = 0.2                  # layer lines
                if y in (14, 15): v = 0.25
            elif kind == "top":
                if x in (0, 15) or y in (0, 15): v = 0.25
                if x in (1,) or y in (1,): v = 0.85
            else:
                v -= 0.1
            px[x, y] = pick(SNDSTN, v)
    return im

def t_glass():
    im = new(); px = im.load()
    frame = hexc("d8eef2"); dark = hexc("9cc2cc")
    for i in range(S):
        px[i, 0] = frame; px[0, i] = frame; px[i, 15] = dark; px[15, i] = dark
    for i in range(4):                                     # light streaks
        px[3 + i, 6 - i] = hexc("ffffff", 200)
        px[9 + i, 12 - i] = hexc("ffffff", 160)
    px[11, 11] = hexc("ffffff", 160)
    return im

def t_bookshelf(seed=16):
    im = t_planks(seed); px = im.load(); r = random.Random(seed)
    books = ["8f2b2b", "2b4f8f", "2f7a3a", "8a6a1f", "5a2b7a", "a35a24", "3a6f78"]
    for shelf in (1, 9):
        x = 1
        while x < 15:
            w = 1 if r.random() < 0.7 else 2
            col = hexc(r.choice(books)); h = r.randint(5, 6)
            for xx in range(x, min(x + w, 15)):
                for y in range(shelf + 6 - h, shelf + 6):
                    px[xx, y] = shade(col, 1.15) if xx == x else col
                px[xx, shelf + 6 - h] = shade(col, 1.35)   # light on top of book
            x += w
            if r.random() < 0.15: x += 1                     # gap
        for x in range(1, 15): px[x, shelf + 6] = PLANK[0]
    return im

def t_snow(seed=17):
    im = new(); px = im.load(); r = random.Random(seed); pal = ramp("d4e0ea", "e2ebf2", "eef3f8", "f8fbfd", "ffffff")
    f = field(seed)
    for y in range(S):
        for x in range(S):
            px[x, y] = pick(pal, f[y][x] * 0.6 + 0.4 + r.uniform(-0.1, 0.1))
    return im

def t_ice(seed=18):
    im = new(); px = im.load(); r = random.Random(seed); pal = ramp("7fb0e0", "8fbde8", "a2cbf0", "b7d9f5", "d0e8fa")
    for y in range(S):
        for x in range(S):
            px[x, y] = pick(pal, 0.35 + r.uniform(-0.1, 0.1))[:3] + (200,)
    for i in range(6):
        px[2 + i, 9 - i] = hexc("e8f6ff", 230); px[8 + i, 14 - i] = hexc("e8f6ff", 230)
    return im

def t_sponge(seed=19):
    im = new(); px = im.load(); r = random.Random(seed); pal = ramp("a39324", "bfae2f", "d3c33c", "e2d452", "efe27a")
    f = field(seed, ((8, 0.6), (16, 0.4)))
    for y in range(S):
        for x in range(S):
            v = f[y][x]
            px[x, y] = pal[0] if v < 0.35 else pick(pal, v)
    return im

def t_crate(seed=20):
    im = t_planks(seed); px = im.load()
    for i in range(S):
        for (x, y) in ((i, 0), (i, 15), (0, i), (15, i), (i, i), (i, 15 - i)):
            px[x, y] = PLANK[1] if (x + y) % 2 else PLANK[0]
        if 0 < i < 15:
            px[i, i] = PLANK[4] if i < 15 else px[i, i]
    return im

TILES = {
    0:  t_grass_top,  1: t_stone, 2: t_dirt, 3: t_grass_side, 4: t_planks,
    7:  t_bricks, 16: t_cobble, 17: t_bedrock, 18: t_sand, 19: t_gravel,
    20: t_log_side, 21: t_log_top, 22: t_leaves,
    23: lambda: t_metal_block(IRON, 21, False), 39: lambda: t_metal_block(IRON, 22, True), 55: lambda: t_metal_block(IRON, 23, False),
    24: lambda: t_metal_block(GOLD, 24, False), 40: lambda: t_metal_block(GOLD, 25, True), 56: lambda: t_metal_block(GOLD, 26, False),
    25: lambda: t_sandstone(27, "top"), 41: lambda: t_sandstone(28, "side"), 57: lambda: t_sandstone(29, "bottom"),
    32: lambda: t_ore(30, ramp("a8740e", "e8b828", "ffe066", "fff6c0")),
    33: lambda: t_ore(31, ramp("8a6048", "c99a7a", "e8c0a0", "f6e0cc")),
    34: lambda: t_ore(32, ramp("0c0c0c", "1c1c1c", "303030", "4a4a4a"), 7),
    35: t_bookshelf, 36: t_mossy, 37: t_obsidian,
    48: t_sponge, 49: t_glass, 50: t_snow, 51: t_ice, 52: t_stone_bricks, 53: t_crate,
}

def build_terrain():
    with zipfile.ZipFile(BASE) as z:
        terrain = Image.open(io.BytesIO(z.read("terrain.png"))).convert("RGBA")
    for tile, fn in TILES.items():
        img = fn()
        x, y = (tile % 16) * S, (tile // 16) * S
        terrain.paste(Image.new("RGBA", (S, S), (0, 0, 0, 0)), (x, y))
        terrain.paste(img, (x, y))
    return terrain

def main():
    terrain = build_terrain()
    buf = io.BytesIO(); terrain.save(buf, "PNG")
    with zipfile.ZipFile(BASE) as src, zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as dst:
        for item in src.infolist():
            data = buf.getvalue() if item.filename == "terrain.png" else src.read(item.filename)
            dst.writestr(item.filename, data)
    print("wrote", OUT)
    if len(sys.argv) > 1:
        terrain.crop((0, 0, 256, 96)).resize((1024, 384), Image.NEAREST).save(sys.argv[1])

if __name__ == "__main__":
    main()
