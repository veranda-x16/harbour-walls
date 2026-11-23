/* Harbour Walls on the host: the platform's side as counters (the tests,
   the replay, the planner) */
#include <stdio.h>
#include "../src/engine.h"
#include "host.h"

unsigned long n_rect, n_cells, n_field, n_crate;
unsigned long n_sfx[16];
int quiet = 1;

void plat_rect(u8 x, u8 y, u8 w, u8 h, u8 kind)
{
    ++n_rect;
    n_cells += (unsigned long)w * h;
    if (!quiet)
        printf("rect %u %u %u %u %u\n", x, y, w, h, kind);
}

void plat_crate(u8 x, u8 y, u8 kind)
{
    ++n_crate;
    if (!quiet)
        printf("crate %u %u %u\n", x, y, kind);
}

void plat_field(void)
{
    ++n_field;
}

void plat_sfx(u8 id)
{
    if (id < 16)
        ++n_sfx[id];
}
