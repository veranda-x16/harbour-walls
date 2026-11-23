#!/usr/bin/env python3
"""Harbour Walls' field in full colour: each kind of card (sea, sand-
stone wall, harbour land, the quay, the two growing arms) a 32 x 32
picture of 4 x 4 VERA 4 bpp tiles in its own palette row, and the four
crates a tile each (Veranda's vr_pic: the kernel's tile mode under the
bitmap, DECISIONS V105).

    mkpic.py <out dir> <sdk dir>

A card (x, y) of kind k shows tile (y & 3) * 4 + (x & 3) of k's picture,
so the field is textured, not tiled card by card, and drawing a run of
cards stays a run of map entries (x16/pic.s). The sea's wave crests are
palette entries 13-15 of its row, turned round every few frames by the
interrupt (the sea moves; three entries). Writes into <out dir>:
    pic.vram.1 ...  records for the SDK's vr_vram_load (record 1 the
                    palette rows 6-12: the buoys keep 2-5)
    pic.h           PIC_RECORDS, each kind's 16 map entries, the crates',
                    the sea's crest colours
    pic.png         a field of every kind, as VERA shows it
Everything is drawn here, this program's own.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
"""
import math
import os
import sys

FIRST_TILE = 16
ROW0 = 6

# ---------------------------------------------------------------- palettes ($0RGB; index 0 unused)
PAL = {
    'sea': [0, 0x135, 0x146, 0x157, 0x168, 0x279, 0x38A, 0x49B, 0x6AC, 0x8CE, 0xBDF, 0xEFF, 0x024, 0x8BE, 0x5AD, 0x37A],
    'land': [0, 0x152, 0x263, 0x373, 0x484, 0x595, 0x6A5, 0x8B6, 0xAC7, 0xFE6, 0xFFF, 0xE8A, 0x795, 0xA96, 0xCB8, 0x241],
    'wall': [0, 0x321, 0x543, 0x754, 0x965, 0xA76, 0xB87, 0xC98, 0xDA9, 0xEBA, 0xFDC, 0x432, 0x876, 0x998, 0xBBA, 0x210],
    'quay': [0, 0x223, 0x334, 0x445, 0x556, 0x667, 0x778, 0x889, 0x99A, 0xAAB, 0xCCD, 0x453, 0x564, 0x111, 0xDDE, 0x000],
    'arma': [0, 0x410, 0x620, 0x830, 0xA40, 0xC60, 0xE70, 0xF92, 0xFB5, 0xFD8, 0xFEB, 0x210, 0x530, 0x751, 0xFFF, 0x000],
    'armb': [0, 0x214, 0x326, 0x438, 0x54A, 0x66C, 0x77D, 0x99E, 0xBBF, 0xDCF, 0xFEF, 0x102, 0x324, 0x546, 0xFFF, 0x000],
    'crate': [0, 0x210, 0x431, 0x642, 0x863, 0xA84, 0xCA6, 0xFD3, 0xFF8, 0xE33, 0xFFF, 0x445, 0x889, 0x146, 0x38A, 0x000],
}
ROWS = ['sea', 'land', 'wall', 'quay', 'arma', 'armb', 'crate']      # rows 6-12
CRESTS = PAL['sea'][13:16]                                          # turned round by the interrupt


def per(v, n=32):
    return 2 * math.pi * v / n


def hsh(x, y, s=0):
    h = (x * 374761393 + y * 668265263 + s * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFF


def block(f):
    return [[f(x, y) for x in range(32)] for y in range(32)]


# ---------------------------------------------------------------- the kinds
def sea(x, y):
    # deep water with slow swells (seamless at 32), crests in three phases
    v = 0.55 * math.sin(per(x + 2 * y)) + 0.45 * math.sin(per(2 * x - y, 32))
    c = 3 + int(round(v * 1.4))                      # 1-5... clamp
    c = max(2, min(5, c))
    # crests: short arcs, each in one of three phases (entries 13-15)
    for k, (cx, cy) in enumerate([(4, 5), (20, 3), (12, 13), (27, 15), (3, 22), (18, 24), (9, 30), (26, 29)]):
        dx = (x - cx) % 32
        dy = (y - cy) % 32
        if dx < 7 and dy in (0, 1):
            arc = [1, 0, 0, 0, 0, 0, 1][dx]
            if dy == arc:
                return 13 + k % 3
            if dy == 1 and arc == 0 and 1 < dx < 5:
                return 7
    if hsh(x, y, 1) % 97 == 0:
        return 8
    return c


def land(x, y):
    v = 0.5 * math.sin(per(x + y)) + 0.5 * math.sin(per(3 * x - 2 * y)) * 0.6
    c = 4 + int(round(v * 1.5))
    c = max(2, min(6, c))
    h = hsh(x, y, 2)
    if h % 11 == 0:
        c = 7                                         # light blades
    elif h % 13 == 0:
        c = 1 if c < 4 else 2                         # shade
    # flowers: a few fixed ones in the 32 x 32
    for (fx, fy, fc) in [(5, 6, 9), (22, 9, 10), (13, 21, 11), (28, 26, 9), (8, 28, 10)]:
        if (x, y) == (fx, fy):
            return fc
        if abs(x - fx) + abs(y - fy) == 1:
            return 8 if fc != 11 else 12
    # a worn path across (sand)
    py = 16 + 3 * math.sin(per(x))
    if abs(y - py) < 1.1 and hsh(x, 0, 3) % 3:
        return 13 if (x + y) % 3 else 14
    return c


def wall(x, y):
    # sandstone blocks 16 x 8, every other course half a block over
    by = y // 8
    bx = (x + (8 if by % 2 else 0)) % 16
    yy = y % 8
    if yy == 7 or bx == 15:
        return 1                                      # mortar
    if yy == 6 or bx == 14:
        return 3                                      # the block's shadow
    if yy == 0 or bx == 0:
        return 10 if yy == 0 and bx < 14 else 9       # its lit edge
    t = (hsh(x // 2, y // 2, by * 7 + (x + (8 if by % 2 else 0)) // 16) % 5)
    return [6, 7, 7, 8, 5][t]


def quay(x, y):
    # granite setts 8 x 8 with a bevel, a little moss in the joints
    xx, yy = x % 8, y % 8
    if xx == 7 or yy == 7:
        return 11 if hsh(x, y, 4) % 9 == 0 else 2
    if xx == 0 or yy == 0:
        return 9
    if xx == 6 or yy == 6:
        return 4
    return [6, 7, 6, 5, 7, 8][hsh(x // 8, y // 8, 5) % 6] if hsh(x, y, 6) % 7 else 5


def arm(diag):
    # a growing wall: planks in stripes (/ or \), light on their edges
    def f(x, y):
        k = (x + y) % 8 if diag else (x - y) % 8
        if k == 0:
            return 11                                 # the gap
        if k == 1:
            return 9                                  # lit edge
        if k == 7:
            return 3
        return [7, 6, 6, 5, 5, 4][k - 1]
    return f


def crate(icon):
    """an 8 x 8 crate floating: wood, its icon"""
    ICONS = {
        'bolt': ["........", "....77..", "...77...", "..7777..", "...77...", "..77....", ".77.....", "........"],
        'anchor': ["...cc...", "...cc...", ".cccccc.", "...cc...", "...cc...", "c..cc..c", ".cccccc.", "..cccc.."],
        'ring': ["..9aa9..", ".a9..9a.", "9a....a9", "a......a", "a......a", "9a....a9", ".a9..9a.", "..9aa9.."],
        'star': ["...88...", "...88...", "88888888", ".888888.", "..8888..", ".88..88.", "88....88", "........"],
    }
    rows = ICONS[icon]
    out = []
    for y in range(8):
        r = []
        for x in range(8):
            if x in (0, 7) or y in (0, 7):
                v = 2 if (x + y) % 2 else 3                    # the box's frame
                if (x, y) in ((0, 0), (7, 0), (0, 7), (7, 7)):
                    v = 13                                     # the sea at its corners
            else:
                v = 5 if y % 3 else 4                          # planks
            ch = rows[y][x]
            if ch != '.':
                v = {'7': 7, 'c': 12, '9': 9, 'a': 10, '8': 8}[ch]
            r.append(v)
        out.append(r)
    return out


KINDS = [('sea', sea), ('sea', sea), ('arma', arm(True)), ('armb', arm(False)), ('wall', wall), ('land', land),
         ('quay', quay)]
CRATES = ['bolt', 'anchor', 'ring', 'star']


def tile_bytes(px):
    return bytes(px[y][x] << 4 | px[y][x + 1] for y in range(8) for x in range(0, 8, 2))


def main():
    out, sdk = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    sys.path.insert(0, os.path.join(sdk, 'tools'))
    import mkwall                                   # the SDK's: VRAM chunks into records
    tiles = []
    ent = []
    done = {}
    for name, f in KINDS:
        row = ROW0 + ROWS.index(name)
        key = (name, f.__name__ if hasattr(f, '__name__') else id(f))
        if name == 'sea' and 'sea' in done:
            ent.append(done['sea'])
            continue
        b = block(f)
        e = []
        for ty in range(4):
            for tx in range(4):
                px = [r[tx * 8:tx * 8 + 8] for r in b[ty * 8:ty * 8 + 8]]
                assert all(1 <= v <= 15 for rr in px for v in rr), (name, px)
                t = FIRST_TILE + len(tiles)
                tiles.append(tile_bytes(px))
                e.append(t | row << 12)
        ent.append(e)
        done[name] = e
        del key
    cent = []
    for c in CRATES:
        t = FIRST_TILE + len(tiles)
        tiles.append(tile_bytes(crate(c)))
        cent.append(t | (ROW0 + ROWS.index('crate')) << 12)
    assert FIRST_TILE + len(tiles) <= 480
    rows = b''.join(v.to_bytes(2, 'little') for name in ROWS for v in PAL[name])
    recs = mkwall.chunks_to_records([(0x1FA00 + 32 * ROW0, rows)])
    recs += mkwall.chunks_to_records([(0x13000 + 32 * FIRST_TILE, b''.join(tiles))])
    for f in os.listdir(out):
        if f.startswith('pic.vram.'):
            os.remove(os.path.join(out, f))
    for k, r in enumerate(recs):
        open(os.path.join(out, 'pic.vram.%d' % (k + 1)), 'wb').write(r)
    h = ['/* Harbour Walls: the field\'s pictures (generated by tools/mkpic.py; do not edit) */',
         '#ifndef PIC_H', '#define PIC_H',
         '#define PIC_RECORDS %d' % len(recs),
         '#define PIC_SEAROW %d' % (ROW0 + ROWS.index('sea')),
         '/* each kind\'s picture: card (x, y) shows tile kind_base + (y & 3) * 4 + (x & 3) in row kind_row */',
         'static const unsigned char kind_base[7] = { %s };' % ', '.join(str(e[0] & 0x3FF) for e in ent),
         'static const unsigned char kind_row[7] = { %s };' % ', '.join('0x%02X' % ((e[0] >> 12) << 4) for e in ent)]
    assert all(e[0] & 0x3FF < 240 for e in ent)
    h += ['static const unsigned int crate_ent[4] = { %s };' % ', '.join('0x%04X' % v for v in cent),
          '/* the sea\'s wave crests: palette entries 13-15 of its row, turned round */',
          'static const unsigned int crest[3] = { %s };' % ', '.join('0x%03X' % v for v in CRESTS),
          '#endif', '']
    open(os.path.join(out, 'pic.h'), 'w').write('\n'.join(h))
    preview(out, ent, cent, tiles)
    print('%d tiles (%d bytes) in %d records, palette rows %d-%d' % (len(tiles), 32 * len(tiles), len(recs), ROW0,
                                                                   ROW0 + len(ROWS) - 1))


def preview(out, ent, cent, tiles):
    from PIL import Image
    pal = {}
    for i, name in enumerate(ROWS):
        for k, v in enumerate(PAL[name]):
            pal[(ROW0 + i) * 16 + k] = ((v >> 8) * 17, (v >> 4 & 15) * 17, (v & 15) * 17)
    W, H = 320, 128
    im = Image.new('RGB', (W, H))
    px = im.load()
    for cy in range(H // 8):
        for cx in range(W // 8):
            k = [0, 4, 5, 6, 2, 3, 0, 5][(cx // 6 + cy // 8 * 3) % 8]
            e = ent[k][(cy & 3) * 4 + (cx & 3)]
            if (cx, cy) in ((2, 2), (9, 12), (30, 5), (36, 14)):
                e = cent[(cx + cy) % 4]
            t, r = e & 0x3FF, e >> 12
            d = tiles[t - FIRST_TILE]
            for y in range(8):
                for x in range(8):
                    b = d[y * 4 + x // 2]
                    v = b >> 4 if x % 2 == 0 else b & 15
                    px[cx * 8 + x, cy * 8 + y] = pal[r * 16 + v]
    im.resize((W * 2, H * 2), Image.NEAREST).save(os.path.join(out, 'pic.png'))


if __name__ == '__main__':
    main()
