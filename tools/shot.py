#!/usr/bin/env python3
"""A screenshot from an x16emu dump (-dump V or RV), as VERA shows it:
the bitmap and the colour map (the SDK's vram2png.compose) with the
sprites on top (4 bpp ones, in front of the layers: the pointer and the
buoys), in index order (sprite 0 in front).

usage: shot.py <sdk tools dir> <dump.bin> <out.png> [--no-pointer]
"""
import sys

sys.path.insert(0, sys.argv[1])
import vram2png                     # noqa: E402
from PIL import Image               # noqa: E402

VDUMP = vram2png.VDUMP
VRAM = 131072


def main():
    d = open(sys.argv[2], 'rb').read()
    v = d[len(d) - VDUMP:]
    rows = vram2png.compose(v, 640, 480)
    im = Image.new('RGB', (640, 480))
    px = im.load()
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            px[x, y] = c
    pal = v[VRAM + 256:VRAM + 256 + 512]
    spr = v[VDUMP - 1024:]

    def rgb(i):
        gb, r = pal[2 * i], pal[2 * i + 1]
        return ((r & 15) * 17, (gb >> 4) * 17, (gb & 15) * 17)
    first = 1 if '--no-pointer' in sys.argv else 0
    for n in range(127, first - 1, -1):
        a = spr[8 * n:8 * n + 8]
        z = (a[6] >> 2) & 3
        if not z or a[1] & 0x80:
            continue
        addr = ((a[1] & 15) << 13) | (a[0] << 5)
        x = a[2] | ((a[3] & 3) << 8)
        y = a[4] | ((a[5] & 3) << 8)
        if x >= 1024 - 64:
            x -= 1024
        if y >= 1024 - 64:
            y -= 1024
        w = 8 << ((a[7] >> 4) & 3)
        h = 8 << ((a[7] >> 6) & 3)
        po = a[7] & 15
        hf, vf = a[6] & 1, a[6] & 2
        for sy in range(h):
            for sx in range(w):
                b = v[addr + (sy * w + sx) // 2]
                c = (b >> 4) if sx % 2 == 0 else (b & 15)
                if not c:
                    continue
                dx = x + (w - 1 - sx if hf else sx)
                dy = y + (h - 1 - sy if vf else sy)
                if 0 <= dx < 640 and 0 <= dy < 480:
                    px[dx, dy] = rgb(c + 16 * po)
    im.save(sys.argv[3])


if __name__ == '__main__':
    main()
