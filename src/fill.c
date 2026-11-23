/* Harbour Walls: the capture's flood fill, in C (the host; the X16 has the
   same in fill.s). A scan-line fill with a stack of seeds: a seed's run of
   free cards (sea, crates, growing arms: grid value below C_WALL, and so
   not yet marked) is marked, and the runs above and below it become
   seeds. It stops after about budget cards and goes on at the next call.
   A full stack is not an error: the seed is dropped and fill_rescan()
   (when the stack runs dry) finds every free card next to a marked one. */
#include "engine.h"

u8 fsx[FSTACK], fsy[FSTACK];
u8 fsp;
u8 fill_lostf;
#define lost fill_lostf

void fill_push(u8 x, u8 y)
{
    if (fsp == FSTACK) {
        lost = 1;
        return;
    }
    fsx[fsp] = x;
    fsy[fsp] = y;
    ++fsp;
}

#ifndef FILL_ASM
/* the runs of free cards in row y between xl and xr as seeds */
static void seeds(u8 y, u8 xl, u8 xr)
{
    u8 *row = grid[y];
    u8 in = 0;
    for (; xl <= xr; ++xl) {
        if (row[xl] < C_WALL) {
            if (!in)
                fill_push(xl, y);
            in = 1;
        } else
            in = 0;
    }
}

/* a seed after the stack ran over: a free card beside a marked one */
static void rescan(void)
{
    u8 x, y, *r;
    lost = 0;
    for (y = 1; y < GH - 1; ++y) {
        r = grid[y];
        for (x = 1; x < GW - 1; ++x)
            if (r[x] < C_WALL && ((r[x - 1] & MARK) || (r[x + 1] & MARK) || (grid[y - 1][x] & MARK)
                                  || (grid[y + 1][x] & MARK)))
                fill_push(x, y);
    }
}

u8 fill_run(u16 budget)
{
    u8 x, y, xl, xr, *row;
    for (;;) {
        if (!fsp) {
            if (!lost)
                return 1;
            rescan();
            continue;
        }
        --fsp;
        x = fsx[fsp];
        y = fsy[fsp];
        row = grid[y];
        if (row[x] >= C_WALL)
            continue;           /* solid, or marked already */
        xl = x;
        while (row[xl - 1] < C_WALL)
            --xl;
        xr = x;
        while (row[xr + 1] < C_WALL)
            ++xr;
        for (x = xl; x <= xr; ++x)
            row[x] |= MARK;
        seeds(y - 1, xl, xr);
        seeds(y + 1, xl, xr);
        x = xr - xl + 1;
        if (budget <= x)
            return 0;
        budget -= x;
    }
}
#else
/* fill.s's fill_run calls this when its stack ran over */
void fill_rescan(void)
{
    u8 x, y, *r;
    lost = 0;
    for (y = 1; y < GH - 1; ++y) {
        r = grid[y];
        for (x = 1; x < GW - 1; ++x)
            if (r[x] < C_WALL && ((r[x - 1] & MARK) || (r[x + 1] & MARK) || (grid[y - 1][x] & MARK)
                                  || (grid[y + 1][x] & MARK)))
                fill_push(x, y);
    }
}
#endif
