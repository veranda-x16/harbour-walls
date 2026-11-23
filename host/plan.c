/* Harbour Walls: the gate's planner. With the engine (the same C as the
   X16 runs) it plays a game of a given seed ahead of time and chooses the
   walls a mouse script will place:

     plan <seed>                  -> stdout, one line a wall:
       P <step> <x> <y> <vertical> <capture|break> <expected effect>

   Level 1 is cleared with walls that calm the most sea and that no buoy
   breaks, whether the click lands up to 3 steps earlier or later than
   planned; then level 2 is lost with walls that buoys do break (on a
   growing arm, not at the click), until the game is over. A wall is
   placed at step 16 v + 8 for the first v at least 80 steps after the
   previous wall was done: the script waits for the game's step / 16
   (hw_sync) to be v and clicks; the test build holds a click for the next
   step 8 of 16 (main.c), so it lands on the step planned. The walls are
   also checked 3 steps earlier and later, for good measure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"
#include "host.h"

#define SLACK 3

static u8 *base, *tmp, *at_t[2 * SLACK + 1];

/* steps until no arm grows and no capture runs (or the game leaves play) */
static void settle(void)
{
    int n = 0;
    while (state == S_PLAY && game_busy() && n++ < 2000)
        game_step();
}

/* from base: a wall at step t; its result: -1 refused; else the cards it
   calmed (gain), the lives it cost (broke) and the step of the first
   break */
static int trial(int t, u8 x, u8 y, u8 v, int *gain, int *broke, int *brk_at)
{
    u16 f0, b0;
    u8 l0;
    game_load(at_t[t]);
    if (state != S_PLAY)
        return -1;
    l0 = lives;
    f0 = filled;
    b0 = breaks;
    *brk_at = -1;
    if (!game_place(x, y, v))
        return -1;
    if (breaks != b0)
        *brk_at = step_no;
    while (state == S_PLAY && game_busy()) {
        u16 k = breaks;
        game_step();
        if (breaks != k && *brk_at < 0)
            *brk_at = step_no;
    }
    if (state == S_PLAY || state == S_CLEAR) {
        /* (the capture's last step) */
    }
    *gain = (int)filled - f0 + (state == S_CLEAR ? 10000 : 0);
    *broke = breaks - b0;
    (void)l0;
    return 0;
}

static int next_t(void)
{
    int r = step_no + 80;
    return (r + 15) / 16 * 16 + 8;
}

int main(int argc, char **argv)
{
    u16 s;
    int walls = 0, breaks = 0, i0;
    setvbuf(stdout, 0, _IOLBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: plan <seed>\n");
        return 2;
    }
    s = (u16)atoi(argv[1]);
    for (i0 = 0; i0 <= 2 * SLACK; ++i0)
        at_t[i0] = malloc(game_state_size());
    base = malloc(game_state_size());
    tmp = malloc(game_state_size());
    game_new(s);
    printf("seed %u\n", s);
    while (state != S_OVER && walls < 60) {
        int t = next_t(), best = -1, bx_ = 0, by_ = 0, bv = 0, bscore = -1, x, y, v, k, d, n = 0, i;
        int want_break = level >= 2;
        static struct cand { int x, y, v, g; } c[2000];
        game_save(base);
        for (d = 0; d <= 2 * SLACK; ++d) {
            while (step_no < t - SLACK + d && state == S_PLAY)
                game_step();
            game_save(at_t[d]);
        }
        /* every wall at t, then the best ones checked at t - 3 .. t + 3 */
        for (v = 0; v < 2; ++v) {
            int lines = v ? GW - 2 : GH - 2;
            for (k = 1; k <= lines; ++k) {
                int p;
                for (p = 0; p < 7; ++p) {
                    int g, b, at;
                    if (v)
                        x = k, y = 4 + p * 7;
                    else
                        y = k, x = 6 + p * 11;
                    if (trial(SLACK, (u8)x, (u8)y, (u8)v, &g, &b, &at) < 0)
                        continue;
                    if (want_break ? (!b || at < t + 6) : (b || g <= 0))
                        continue;
                    if (!want_break && walls < 2 && g >= 10000)
                        continue;          /* level 1 in three walls at least */
                    c[n].x = x, c[n].y = y, c[n].v = v, c[n].g = want_break ? -at : g;
                    ++n;
                }
            }
        }
        while (n) {
            int bi = 0, ok = 1, sc = 1 << 30;
            for (i = 1; i < n; ++i)
                if (c[i].g > c[bi].g)
                    bi = i;
            for (d = 0; d <= 2 * SLACK && ok; ++d) {
                int g, b, at;
                if (trial(d, (u8)c[bi].x, (u8)c[bi].y, (u8)c[bi].v, &g, &b, &at) < 0)
                    ok = 0;
                else if (want_break ? (!b || at < t - SLACK + d + 6) : (b != 0))
                    ok = 0;
                else if (!want_break && walls < 2 && g >= 10000)
                    ok = 0;
                else if ((want_break ? -at : g) < sc)
                    sc = want_break ? -at : g;
            }
            if (ok) {
                best = 1, bx_ = c[bi].x, by_ = c[bi].y, bv = c[bi].v, bscore = sc;
                break;
            }
            c[bi] = c[--n];
        }
        game_load(base);
        if (best < 0 || (!want_break && bscore <= 0)) {
            /* nothing good now: a little later */
            int n = 16;
            while (n-- && state != S_OVER)
                game_step();
            continue;
        }
        while (step_no < t)
            game_step();
        {
            u8 l0 = lives, lv = level;
            u16 f0 = filled;
            game_place((u8)bx_, (u8)by_, (u8)bv);
            settle();
            printf("P %d %d %d %d %s lives %u->%u filled %u->%u calm %u level %u state %u\n", t, bx_, by_, bv,
                   want_break ? "break" : "capture", l0, lives, f0, filled, calm, lv, state);
            if (lives < l0)
                ++breaks;
        }
        ++walls;
        while (state == S_CLEAR)
            game_step();
        settle();
    }
    printf("end walls %d breaks %d state %u level %u score %lu step %u\n", walls, breaks, state, level,
           (unsigned long)score, step_no);
    return state == S_OVER ? 0 : 1;
}
