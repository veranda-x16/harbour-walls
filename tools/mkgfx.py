#!/usr/bin/env python3
"""Harbour Walls' pictures, drawn here from text and geometry.

usage: mkgfx.py <out.h>            the C header: the card glyphs and the
                                   font (PutCards), the buoy sprite and its
                                   palettes, the two pointers
       mkgfx.py --icons <dir>      icon.pbm (24 x 21) and tile.txt (32 x 32)

Glyphs 16-31 are the field's and the panel's (0-15 blank), 32-95 a 5 x 7 font made bold
(ASCII, capitals only). The buoy is a 16 x 16 sprite at 4 bits a pixel;
one picture, a palette per class (slow, medium, fast, anchored). The
pointers are 32 x 32 at 4 bits (sprite 0's own picture, PTR_OWN: its hot
spot is the top left corner), an arrow with a badge for the wall's
orientation, in Veranda's pointer colours (palette entries 17-20).
"""
import sys

# ---------------------------------------------------------------- cards

def card(s):
    rows = s.split()
    assert len(rows) == 8 and all(len(r) == 8 for r in rows), s
    return [int(r.replace('#', '1').replace('.', '0'), 2) for r in rows]

GAME = {
    0: card("""........ ........ ........ .##..... #..#.... ....#..# .....##. ........"""),   # sea
    1: card("""######## #......# #.####.# #.#..#.# #.#..#.# #.####.# #......# ########"""),   # crate
    2: card("""...#...# ..#...#. .#...#.. #...#... ...#...# ..#...#. .#...#.. #...#..."""),   # arm A /
    3: card("""#...#... .#...#.. ..#...#. ...#...# #...#... .#...#.. ..#...#. ...#...#"""),   # arm B \\
    4: card("""######## #...#... #...#... #...#... ######## ..#...#. ..#...#. ..#...#."""),   # wall
    5: card("""........ ...#.... ........ .......# ........ .#...... ........ .....#.."""),   # land
    6: card("""######## #.#.#.#. ######## .#.#.#.# ######## #.#.#.#. ######## .#.#.#.#"""),   # quay
    7: card("""........ ........ ........ ........ ........ ........ ........ ........"""),   # blank
    8: card("""....##.. ...##... ..##.... .######. ....##.. ...##... ..##.... .##....."""),   # concrete
    9: card("""...#.... ..#.#... ...#.... .#####.. ...#.... #..#..#. .#.#.#.. ..###..."""),   # anchor
    10: card("""..####.. .#....#. #..##..# #.#..#.# #.#..#.# #..##..# .#....#. ..####.."""),  # lifebuoy
    11: card("""...#.... ...#.... #######. .#####.. ..###... .##.##.. ##...##. ........"""),  # pearl
    12: card("""...##... ...##... ...##... ...##... ...##... ...##... ...##... ...##..."""),  # target mark
    13: card("""..####.. .######. ###..### ##....## ##....## ###..### .######. ..####.."""),  # a life
    14: card("""######## ######## ######## ######## ######## ######## ######## ########"""),  # ink
    15: card("""........ ........ ........ ######## ######## ........ ........ ........"""),  # rule
}

FONT5 = {
    ' ': '..... ..... ..... ..... ..... ..... .....',
    '!': '..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#..',
    '%': '##... ##..# ...#. ..#.. .#... #..## ...##',
    "'": '..#.. ..#.. .#... ..... ..... ..... .....',
    '(': '...#. ..#.. .#... .#... .#... ..#.. ...#.',
    ')': '.#... ..#.. ...#. ...#. ...#. ..#.. .#...',
    '*': '..... ..#.. #.#.# .###. #.#.# ..#.. .....',
    '+': '..... ..#.. ..#.. ##### ..#.. ..#.. .....',
    ',': '..... ..... ..... ..... .##.. ..#.. .#...',
    '-': '..... ..... ..... ##### ..... ..... .....',
    '.': '..... ..... ..... ..... ..... .##.. .##..',
    '/': '..... ....# ...#. ..#.. .#... #.... .....',
    ':': '..... .##.. .##.. ..... .##.. .##.. .....',
    '<': '...#. ..#.. .#... #.... .#... ..#.. ...#.',
    '=': '..... ..... ##### ..... ##### ..... .....',
    '>': '.#... ..#.. ...#. ....# ...#. ..#.. .#...',
    '?': '.###. #...# ....# ...#. ..#.. ..... ..#..',
    '0': '.###. #...# #..## #.#.# ##..# #...# .###.',
    '1': '..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.',
    '2': '.###. #...# ....# ...#. ..#.. .#... #####',
    '3': '##### ...#. ..#.. ...#. ....# #...# .###.',
    '4': '...#. ..##. .#.#. #..#. ##### ...#. ...#.',
    '5': '##### #.... ####. ....# ....# #...# .###.',
    '6': '..##. .#... #.... ####. #...# #...# .###.',
    '7': '##### ....# ...#. ..#.. .#... .#... .#...',
    '8': '.###. #...# #...# .###. #...# #...# .###.',
    '9': '.###. #...# #...# .#### ....# ...#. .##..',
    'A': '.###. #...# #...# ##### #...# #...# #...#',
    'B': '####. #...# #...# ####. #...# #...# ####.',
    'C': '.###. #...# #.... #.... #.... #...# .###.',
    'D': '####. #...# #...# #...# #...# #...# ####.',
    'E': '##### #.... #.... ####. #.... #.... #####',
    'F': '##### #.... #.... ####. #.... #.... #....',
    'G': '.###. #...# #.... #.### #...# #...# .####',
    'H': '#...# #...# #...# ##### #...# #...# #...#',
    'I': '.###. ..#.. ..#.. ..#.. ..#.. ..#.. .###.',
    'J': '..### ...#. ...#. ...#. ...#. #..#. .##..',
    'K': '#...# #..#. #.#.. ##... #.#.. #..#. #...#',
    'L': '#.... #.... #.... #.... #.... #.... #####',
    'M': '#...# ##.## #.#.# #.#.# #...# #...# #...#',
    'N': '#...# #...# ##..# #.#.# #..## #...# #...#',
    'O': '.###. #...# #...# #...# #...# #...# .###.',
    'P': '####. #...# #...# ####. #.... #.... #....',
    'Q': '.###. #...# #...# #...# #.#.# #..#. .##.#',
    'R': '####. #...# #...# ####. #.#.. #..#. #...#',
    'S': '.###. #...# #.... .###. ....# #...# .###.',
    'T': '##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..',
    'U': '#...# #...# #...# #...# #...# #...# .###.',
    'V': '#...# #...# #...# #...# #...# .#.#. ..#..',
    'W': '#...# #...# #...# #.#.# #.#.# #.#.# .#.#.',
    'X': '#...# #...# .#.#. ..#.. .#.#. #...# #...#',
    'Y': '#...# #...# .#.#. ..#.. ..#.. ..#.. ..#..',
    'Z': '##### ....# ...#. ..#.. .#... #.... #####',
}


def font_glyph(ch):
    rows = FONT5.get(ch, FONT5['?']).split()
    out = []
    for r in rows:
        v = int(r.replace('#', '1').replace('.', '0'), 2) << 2     # columns 1-5
        out.append((v | (v >> 1)) & 0xFF)                          # bold
    return out + [0]


def glyphs():
    g = []
    for c in range(96):
        if c < 32:
            g += GAME.get(c - 16, GAME[7])     # glyph k at code 16 + k (0 ends a PutCards text)
        else:
            g += font_glyph(chr(c))
    return g

# ---------------------------------------------------------------- sprites


def buoy():
    """16 x 16: 1 outline, 2 body, 3 shade, 4 the band, 5 the glint"""
    px = [[0] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
            if d > 7.6:
                continue
            if d > 6.4:
                px[y][x] = 1
            elif 6 <= y <= 9:
                px[y][x] = 4 if x + y < 21 else 3
            elif x + y > 19:
                px[y][x] = 3
            else:
                px[y][x] = 2
            if ((x - 4.5) ** 2 + (y - 3.5) ** 2) ** 0.5 < 1.6:
                px[y][x] = 5
    return px


PALS = [    # slow, medium, fast, anchored: entries 0-5
    [0x000, 0x112, 0x3C5, 0x283, 0xEEE, 0xFFF],
    [0x000, 0x112, 0xFC3, 0xC81, 0xEEE, 0xFFF],
    [0x000, 0x112, 0xF55, 0xB22, 0xEEE, 0xFFF],
    [0x000, 0x112, 0x9AB, 0x678, 0xCCD, 0xEEF],
]


def pack4(px):
    out = []
    for row in px:
        for x in range(0, len(row), 2):
            out.append((row[x] << 4) | row[x + 1])
    return out


def outlined(shape, w, h):
    """shape: a set of (x, y); its 8-neighbourhood outside it is the
    outline (1), the inside white (2)"""
    px = [[0] * w for _ in range(h)]
    for (x, y) in shape:
        px[y][x] = 2
    for y in range(h):
        for x in range(w):
            if px[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if (x + dx, y + dy) in shape:
                        px[y][x] = 1
    return px


def inside(poly, x, y):
    c = False
    n = len(poly)
    for i in range(n):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            c = not c
    return c


def pointer(vertical):
    arrow = {(x, y) for y in range(16) for x in range(12)
             if inside([(1, 1), (1, 14.5), (4.5, 11.2), (7, 16), (9, 15), (6.8, 10.5), (11.5, 10.5)], x + .5, y + .5)}
    badge = set()
    for x in range(14):
        for y in range(11):
            cy = 5
            if 4 <= x <= 9 and 4 <= y <= 6:
                badge.add((x, y))
            if x < 4 and abs(y - cy) <= x + 0.5:
                badge.add((x, y))
            if x > 9 and abs(y - cy) <= 13 - x + 0.5:
                badge.add((x, y))
    if vertical:
        badge = {(y, x) for (x, y) in badge}
    ox, oy = (14, 14) if not vertical else (17, 12)
    shape = arrow | {(x + ox, y + oy) for (x, y) in badge}
    px = outlined(shape, 32, 32)
    px[0][0] = 1        # the hot spot is ink
    return px

# ---------------------------------------------------------------- the icons


def tile_rows():
    rows = [['.'] * 32 for _ in range(32)]
    for i in range(3, 29):          # the harbour's quay
        for t in (3, 28):
            rows[t][i] = rows[i][t] = '#'
    for y in range(3, 29):          # a wall across, bricks
        rows[y][17] = rows[y][18] = '#'
    for y in range(4, 28):          # the calm side: rows of dots
        for x in range(20, 28):
            if (x + 2 * y) % 5 == 0 and y % 2 == 0:
                rows[y][x] = '#'
    for y in range(32):             # a buoy
        for x in range(32):
            d = ((x - 9.5) ** 2 + (y - 17.5) ** 2) ** 0.5
            if 4.2 < d < 5.6 or (d <= 4.2 and 16 <= y <= 18):
                rows[y][x] = '#'
    for x in range(5, 15):          # waves
        if (x % 4) in (0, 1):
            rows[8][x] = '#'
        else:
            rows[7][x] = '#'
    return [''.join(r) for r in rows]


def icon_rows():
    t = tile_rows()
    out = []
    for y in range(21):
        sy = 3 + y * 26 // 21
        out.append(''.join(t[sy][3 + x * 26 // 24] for x in range(24)))
    return out


def main():
    if sys.argv[1] == '--icons':
        d = sys.argv[2]
        with open(d + '/tile.txt', 'w') as f:
            f.write('# Harbour Walls\' tile (Veranda Look 2, DECISIONS V77): a sea wall, a buoy,\n'
                    '# the calmed side; written by tools/mkgfx.py --icons\n')
            f.write('colour sky2\n')
            for r in tile_rows():
                f.write(r + '\n')
        with open(d + '/icon.pbm', 'w') as f:
            f.write('P1\n24 21\n')
            for r in icon_rows():
                f.write(' '.join('1' if c == '#' else '0' for c in r) + '\n')
        return
    out = []
    out.append('/* written by tools/mkgfx.py: do not edit */')
    out.append('static const unsigned char glyphs[96 * 8] = {')
    g = glyphs()
    for i in range(0, len(g), 16):
        out.append('    ' + ', '.join('0x%02X' % b for b in g[i:i + 16]) + ',')
    out.append('};')
    b = pack4(buoy())
    out.append('static const unsigned char buoy_pic[128] = {')
    for i in range(0, 128, 16):
        out.append('    ' + ', '.join('0x%02X' % v for v in b[i:i + 16]) + ',')
    out.append('};')
    out.append('/* palettes 2-5 (entries 32-95): slow, medium, fast, anchored; $0RGB */')
    out.append('static const unsigned int buoy_pal[4][6] = {')
    for p in PALS:
        out.append('    { ' + ', '.join('0x%03X' % v for v in p) + ' },')
    out.append('};')
    for name, v in (('ptr_h', False), ('ptr_v', True)):
        p = pack4(pointer(v))
        out.append('static const unsigned char %s[512] = {' % name)
        for i in range(0, 512, 16):
            out.append('    ' + ', '.join('0x%02X' % x for x in p[i:i + 16]) + ',')
        out.append('};')
    open(sys.argv[1], 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
