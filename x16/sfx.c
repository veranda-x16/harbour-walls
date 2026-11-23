/* Harbour Walls for Veranda: the sound effects, from the interrupt, on
   PSG voices 0 and 1 (Veranda keeps 14 and 15 for its own sounds).

   VERA's PSG volume is logarithmic (0.5 dB a step, DECISIONS V72): every
   note is loud (58-63) and fades no lower than 30 before it ends; the
   pulses are squares (width 63), triangles and saws plain (63). */
#include <veranda.h>
#include "../src/engine.h"
#include "hw.h"

#define F(hz) ((u16)((hz) * 2684UL / 1000))    /* Hz -> VERA's frequency word */
#define SQ  0x3F
#define SAW 0x7F
#define TRI 0xBF
#define NZ  0xC0

struct note { u16 f; u8 wave, vol, len; s8 slide; u8 decay; };

static const struct note s_place[] = { { F(880), SQ, 58, 3, 0, 0 }, { F(1320), SQ, 58, 4, 0, 4 }, { 0 } };
static const struct note s_grow[] = { { F(1760), TRI, 44, 1, 0, 0 }, { 0 } };
static const struct note s_set[] = { { F(523), SQ, 60, 5, 0, 0 }, { F(784), SQ, 62, 8, 0, 3 }, { 0 } };
static const struct note s_break[] = { { F(3000), NZ, 63, 12, -60, 2 }, { F(220), SQ, 62, 18, -3, 2 }, { 0 } };
static const struct note s_capture[] = { { F(400), SAW, 60, 20, 107, 1 }, { F(1200), SAW, 50, 6, 0, 3 }, { 0 } };
static const struct note s_crate[] = { { F(1200), TRI, 52, 3, 0, 0 }, { F(1600), TRI, 52, 4, 0, 3 }, { 0 } };
static const struct note s_power[] = { { F(1047), SQ, 60, 4, 0, 0 }, { F(1319), SQ, 60, 4, 0, 0 },
                                       { F(1568), SQ, 60, 4, 0, 0 }, { F(2093), SQ, 62, 9, 0, 3 }, { 0 } };
static const struct note s_clear[] = { { F(523), SQ, 62, 8, 0, 0 }, { F(659), SQ, 62, 8, 0, 0 }, { F(784), SQ, 62, 8, 0, 0 },
                                       { F(1047), SQ, 63, 26, 0, 1 }, { 0 } };
static const struct note s_over[] = { { F(392), SQ, 62, 12, -2, 0 }, { F(330), SQ, 62, 12, -2, 0 },
                                      { F(262), SQ, 63, 32, -1, 1 }, { 0 } };
static const struct note s_storm[] = { { F(900), NZ, 60, 30, -6, 1 }, { F(500), NZ, 50, 12, -4, 1 }, { 0 } };

static const struct note *const tracks[12] = {
    0, s_place, s_grow, s_set, s_break, s_capture, s_crate, s_power, s_clear, s_over, s_storm, 0
};
static const u8 prio[12] = { 0, 3, 1, 4, 7, 5, 2, 6, 8, 9, 6, 0 };
static const u8 chan[12] = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 };

u8 sound_on = 1;
static volatile u8 req[2];
static const struct note *cur[2];
static u8 cprio[2], left[2], vol[2];
static u16 freq[2];
static u8 grows;

void plat_sfx(u8 id)
{
    u8 c;
    hw_log2("SFX", id, step_no);
    if (!sound_on || id >= 12 || !tracks[id])
        return;
    if (id == SFX_GROW && (++grows & 1))
        return;                 /* a tick every other card */
    c = chan[id];
    if (prio[id] >= prio[req[c]])
        req[c] = id;
}

#define VADDR_L (*(volatile u8 *)0x9F20)
#define VADDR_M (*(volatile u8 *)0x9F21)
#define VADDR_H (*(volatile u8 *)0x9F22)
#define VDATA0  (*(volatile u8 *)0x9F23)
#define VCTRL   (*(volatile u8 *)0x9F25)

static void voice(u8 v, u16 f, u8 vl, u8 w)
{
    VCTRL = 0;
    VADDR_L = 0xC0 + 4 * v;
    VADDR_M = 0xF9;
    VADDR_H = 0x11;
    VDATA0 = (u8)f;
    VDATA0 = (u8)(f >> 8);
    VDATA0 = vl ? 0xC0 | vl : 0;
    VDATA0 = w;
}

static void start(u8 c, const struct note *n)
{
    cur[c] = n;
    freq[c] = n->f;
    vol[c] = n->vol;
    left[c] = n->len;
}

/* every frame */
void sfx_irq(void)
{
    u8 c, r;
    const struct note *n;
    for (c = 0; c < 2; ++c) {
        r = req[c];
        if (r) {
            req[c] = 0;
            if (!cur[c] || prio[r] >= cprio[c]) {
                cprio[c] = prio[r];
                start(c, tracks[r]);
            }
        }
        n = cur[c];
        if (!n)
            continue;
        if (!left[c]) {
            ++n;
            if (!n->len) {
                cur[c] = 0;
                cprio[c] = 0;
                voice(c, 0, 0, 0);
                continue;
            }
            start(c, n);
        }
        voice(c, freq[c], vol[c], n->wave);
        freq[c] += n->slide;
        if (vol[c] > 30 + n->decay)
            vol[c] -= n->decay;
        --left[c];
    }
}

void sfx_off(void)
{
    cur[0] = cur[1] = 0;
    req[0] = req[1] = 0;
    cprio[0] = cprio[1] = 0;
    voice(0, 0, 0, 0);
    voice(1, 0, 0, 0);
}
