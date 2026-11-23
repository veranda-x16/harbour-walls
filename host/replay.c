/* Harbour Walls: the X16's game replayed on the host. The test build logs
   every wall with the step it was applied at (IN), a checksum of the
   buoys every 64 steps (HASH), every change of the calmed share (CALM),
   every arm broken (BREAK), the level's end (CLEAR, FULL) and the game's
   (OVER, FULL). Here the same engine (the same C; the X16 runs the flood
   fill in assembly) plays the logged walls at the logged steps, and every
   logged number must come out the same.

     replay <console.txt>      exit 0 if all agree */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"
#include "host.h"

static int fails, checks;

static void upto(unsigned s)
{
    while (step_no != (u16)s)
        game_step();
}

static void eq(const char *what, unsigned s, unsigned a, unsigned b)
{
    ++checks;
    if (a != b) {
        if (fails++ < 10)
            printf("FAIL  replay: %s at step %u: the X16 %u (%X), the host %u (%X)\n", what, s, a, a, b, b);
    }
}

int main(int argc, char **argv)
{
    char line[512], *p;
    FILE *f;
    int games = 0, ins = 0, hashes = 0, events = 0;
    unsigned last = 0;
    if (argc < 2 || !(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: replay <console.txt>\n");
        return 2;
    }
    while (fgets(line, sizeof line, f)) {
        unsigned a, b, c, d, e;
        char pc;
        if ((p = strstr(line, "NEW ")) && sscanf(p, "NEW %x", &a) == 1) {
            game_new((u16)a);
            ++games;
            continue;
        }
        if (!games)
            continue;
        if (!strncmp(line, "IN ", 3) && sscanf(line, "IN %u %u %u %u", &a, &b, &c, &d) == 4) {
            upto(a);
            eq("a wall's placing", a, 1, game_place((u8)b, (u8)c, (u8)d));
            ++ins;
            last = a;
        } else if (!strncmp(line, "HASH ", 5) && sscanf(line, "HASH %u %x", &a, &b) == 2) {
            upto(a);
            eq("the buoys' checksum", a, b, game_hash_balls());
            ++hashes;
            last = a;
        } else if (!strncmp(line, "CALM ", 5)
                   && sscanf(line, "CALM %u %u%c FILLED %u SCORE %u %x", &a, &b, &pc, &c, &d, &e) == 6) {
            upto(a);
            eq("calm", a, b, calm);
            eq("filled", a, c, filled);
            eq("score", a, d, (unsigned)score);
            eq("the buoys' checksum", a, e, game_hash_balls());
            ++events;
        } else if (!strncmp(line, "BREAK ", 6) && sscanf(line, "BREAK %u LIVES %u %x", &a, &b, &c) == 3) {
            upto(a);
            eq("lives", a, b, lives);
            eq("the buoys' checksum", a, c, game_hash_balls());
            ++events;
        } else if (!strncmp(line, "CLEAR LEVEL ", 12) && sscanf(line, "CLEAR LEVEL %u CALM %u SCORE %u", &a, &b, &c) == 3) {
            /* (the step is the FULL line's) */
            ++events;
        } else if (!strncmp(line, "FULL ", 5) && sscanf(line, "FULL %u %x", &a, &b) == 2) {
            upto(a);
            eq("the whole state's checksum", a, b, game_hash());
            ++events;
        } else if (!strncmp(line, "OVER SCORE ", 11) && sscanf(line, "OVER SCORE %u LEVEL %u", &a, &b) == 2) {
            eq("game over", step_no, S_OVER, state);
            eq("the final score", step_no, a, (unsigned)score);
            eq("the final level", step_no, b, level);
            ++events;
        } else if (!strncmp(line, "LEVEL ", 6) && sscanf(line, "LEVEL %u BUOYS %u TARGET %u STEP %u", &a, &b, &c, &d) == 4) {
            upto(d);
            eq("level", d, a, level);
            eq("buoys", d, b, nballs);
            eq("target", d, c, target);
            ++events;
        }
    }
    if (fails) {
        printf("FAIL  replay: %d of %d checks differ\n", fails, checks);
        return 1;
    }
    printf("ok    replay: %d game(s), %d walls, %d checksums every 64 steps, %d events (calm, breaks, levels, the end) "
           "to step %u: all %d numbers equal the host engine's\n", games, ins, hashes, events, last, checks);
    return 0;
}
