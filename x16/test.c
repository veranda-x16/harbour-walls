/* Harbour Walls for Veranda: the test builds' hooks (HW_TEST). They log
   to the emulator's console (LogText) what the gates check: the inputs
   with the step they were applied at, checksums of the state, the events,
   the palette's CRC, and the frame timings from the emulator's cycle
   counter ($9FB8: a write resets it, a read of $9FB8 latches it). */
#ifdef HW_TEST
#include <veranda.h>
#include <string.h>
#include "../src/engine.h"
#include "hw.h"
#include <theme.h>

/* for the mouse scripts' "wait": the step counter / 16, and 1 while a
   wall may be placed (no arm growing, no capture running, playing) */
extern volatile u8 hw_sync, hw_ready, hw_lives, hw_state, hw_lvl;     /* hw.s */

static char line[128];
static u8 skip_next;
#ifndef HW_SLOW
#define HW_SLOW 133333UL
#endif
static char *hex(char *p, u16 v)
{
    u8 i, d;
    for (i = 0; i < 4; ++i) {
        d = (u8)(v >> 12);
        *p++ = (char)(d < 10 ? '0' + d : 'A' - 10 + d);
        v <<= 4;
    }
    *p = 0;
    return p;
}

static char *dec(char *p, u32 v)
{
    char d[11];
    u8 n = 0;
    do {
        d[n++] = (char)('0' + (u8)(v % 10));
        v /= 10;
    } while (v);
    while (n)
        *p++ = d[--n];
    *p = 0;
    return p;
}

static char *word(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    *p = 0;
    return p;
}

static void out(void)
{
    LogText(line);
}

void hw_log2(const char *what, u16 a, u16 b)
{
    char *p = word(line, what);
    *p++ = ' ';
    p = hex(p, a);
    *p++ = ' ';
    hex(p, b);
    out();
}

static u32 cycles(void)
{
    /* (the emulator's counter; a read of $9FB8 latches the four bytes) */
    u32 c;
    u8 b0 = *(volatile u8 *)0x9FB8;
    c = b0 | ((u16) * (volatile u8 *)0x9FB9 << 8);
    c |= (u32) * (volatile u8 *)0x9FBA << 16;
    c |= (u32) * (volatile u8 *)0x9FBB << 24;
    return c;
}

void hw_start(void)
{
    LogText("HW TEST BUILD");
}

void hw_in(u8 x, u8 y, u8 v)
{
    char *p = word(line, "IN ");
    p = dec(p, step_no);
    *p++ = ' ';
    p = dec(p, x);
    *p++ = ' ';
    p = dec(p, y);
    *p++ = ' ';
    dec(p, v);
    out();
}

static void full(const char *what)
{
    char *p = word(line, what);
    *p++ = ' ';
    p = dec(p, step_no);
    *p++ = ' ';
    hex(p, game_hash());
    out();
}

static u8 p_lives = 0xFF, p_level, p_state, p_calm;

static u32 t0, tmax, tsum, tcap, tgrow, tlog;
static u32 cycles(void);
static u32 tm[4], tstep, tpanel;
void hw_mark(u8 i)
{
    tm[i] = cycles();
    if (i == 1)
        tstep += tm[1] - tm[0];
    if (i == 3)
        tpanel = tm[3] - tm[2];
}

void hw_step(void)
{
    char *p;
    u32 e = cycles();
    hw_sync = (u8)(step_no >> 4);
    hw_ready = state == S_PLAY && !game_busy();
    hw_lives = lives;
    hw_state = state;
    hw_lvl = level;
    if ((step_no & 63) == 0) {
        p = word(line, "HASH ");
        p = dec(p, step_no);
        *p++ = ' ';
        hex(p, game_hash_balls());
        out();
    }
    if (p_lives != 0xFF && lives < p_lives) {
        p = word(line, "BREAK ");
        p = dec(p, step_no);
        p = word(p, " LIVES ");
        p = dec(p, lives);
        *p++ = ' ';
        hex(p, game_hash_balls());
        out();
    }
    if (calm != p_calm && level == p_level) {
        p = word(line, "CALM ");
        p = dec(p, step_no);
        *p++ = ' ';
        p = dec(p, calm);
        p = word(p, "% FILLED ");
        p = dec(p, filled);
        p = word(p, " SCORE ");
        p = dec(p, score);
        *p++ = ' ';
        hex(p, game_hash_balls());
        out();
    }
    p_lives = lives;
    p_level = level;
    p_state = state;
    p_calm = calm;
    tlog += cycles() - e;       /* the logs are not the game's time */
}

void hw_level(void)
{
    char *p = word(line, "LEVEL ");
    p = dec(p, level);
    p = word(p, " BUOYS ");
    p = dec(p, nballs);
    p = word(p, " TARGET ");
    p = dec(p, target);
    p = word(p, " STEP ");
    dec(p, step_no);
    out();
    p_level = level;
    p_calm = 0;
    skip_next = 1;             /* the field drawn: a long pass */
}

void hw_clear(void)
{
    char *p = word(line, "CLEAR LEVEL ");
    p = dec(p, level);
    p = word(p, " CALM ");
    p = dec(p, calm);
    p = word(p, " SCORE ");
    dec(p, score);
    out();
    full("FULL");
}

static void prof(void);

void hw_over(void)
{
    char *p;
    prof();
    p = word(line, "OVER SCORE ");
    p = dec(p, score);
    p = word(p, " LEVEL ");
    dec(p, level);
    out();
    full("FULL");
}

void hw_scores(void)
{
    LogText("SCORES SHOWN");
}

/*---------------------------------------------------------------- timings */

static u16 nframes, over, missed, nskip;

void hw_frame_begin(u8 n)
{
    if (n > 1 && !skip_next && state == S_PLAY)
        missed += n - 1;
    if (n > 1)
        hw_log2("GAP", n, step_no);
    skip_next = 0;
    *(volatile u8 *)0x9FB8 = 0;
    tlog = 0;
    tstep = 0;
    tpanel = 0;
    t0 = cycles();
}

static void prof(void)
{
    char *p = word(line, "PROF FRAMES ");
    p = dec(p, nframes);
    p = word(p, " MAX ");
    p = dec(p, tmax);
    p = word(p, " AVG ");
    p = dec(p, nframes ? tsum / nframes : 0);
    p = word(p, " CAPMAX ");
    p = dec(p, tcap);
    p = word(p, " GROWMAX ");
    p = dec(p, tgrow);
    p = word(p, " OVER ");
    p = dec(p, over);
    p = word(p, " MISSED ");
    dec(p, missed);
    out();
}

void hw_frame_end(u8 skip)
{
    u32 t = cycles() - t0 - tlog;
    if (skip || state != S_PLAY) {
        skip_next = 1;
        ++nskip;
        return;
    }
    ++nframes;
    tsum += t;
    if (t > tmax)
        tmax = t;
    if (cap_state != CAP_IDLE && t > tcap)
        tcap = t;
    if ((arm_on[0] || arm_on[1]) && t > tgrow)
        tgrow = t;
    if (t > HW_SLOW) {
        ++over;
        { extern u8 fill_lostf, fsp; hw_log2("SLOW", (u16)(t >> 8), step_no); hw_log2("  CAP", cap_state, fsp | (fill_lostf << 8)); hw_log2("  STEP/PANEL", (u16)(tstep >> 4), (u16)(tpanel >> 4)); }
    }
    if ((nframes & 511) == 0)
        prof();
}

void hw_quit(void)
{
    prof();
    hw_log2("PAL1", pal_crc(), 0);
    LogText("QUIT");
}
#endif

#ifdef HW_BENCH
/* the drawing primitives' costs, once at the start */
static u32 tb;
static void bstart(void)
{
    *(volatile u8 *)0x9FB8 = 0;
    tb = cycles();
}
static void bend(const char *what)
{
    u32 t = cycles() - tb;
    char *p = word(line, "BENCH ");
    p = word(p, what);
    *p++ = ' ';
    dec(p, t);
    out();
}
void hw_bench(void)
{
    u8 i;
    bstart(); plat_rect(10, 10, 1, 1, C_ARMA); bend("rect1x1");
    bstart(); plat_rect(10, 10, 1, 29, C_WALL); bend("rect1x29");
    bstart(); plat_rect(10, 10, 60, 1, C_LAND); bend("rect60x1");
    bstart(); ColorRect(10, 1, 13, 1, RED); bend("colorrect1");
    bstart(); ColorRect(10, 60, 13, 1, RED); bend("colorrect60");
    bstart(); WRectangle(80, 104, 87, 111); bend("wrect8x8");
    bstart(); WRectangle(80, 104, 559, 111); bend("wrect480x8");
    bstart(); plat_crate(10, 10, 0); bend("crate");
    bstart(); for (i = 0; i < 6; ++i) ; bend("empty");
    bstart(); sprites_put(); bend("sprites");
    bstart(); game_hash(); bend("hash");
    {
        extern void paint_row(u8 r);
        u8 r;
        game_level(1);
        fsp = 0;
        fill_push(20, 20);
        bstart(); fill_run(700); bend("fill700");
        bstart(); fill_run(60000); bend("fillrest");
        bstart(); for (r = 1; r < 7; ++r) paint_row(r); bend("paint6");
        for (r = 7; r < 52; ++r) paint_row(r);
    }
    plat_field();
}
#endif
