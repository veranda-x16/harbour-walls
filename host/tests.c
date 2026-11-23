/* Harbour Walls: host tests of the flood fill and the capture rules
   (cc -o tests host/tests.c host/plat.c src/engine.c src/fill.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"
#include "host.h"

static int fails, checks;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++fails; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/*---------------------------------------------------------------- flood fill */

static u8 ref[GH][GW];

/* the reference: a breadth-first search over free cards */
static void bfs(const u8 *sx, const u8 *sy, int n)
{
    static u8 qx[GW * GH], qy[GW * GH];
    int h = 0, t = 0, i;
    memset(ref, 0, sizeof ref);
    for (i = 0; i < n; ++i)
        if (grid[sy[i]][sx[i]] < C_WALL && !ref[sy[i]][sx[i]]) {
            ref[sy[i]][sx[i]] = 1;
            qx[t] = sx[i], qy[t++] = sy[i];
        }
    while (h < t) {
        int x = qx[h], y = qy[h++], d;
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (grid[ny][nx] < C_WALL && !ref[ny][nx]) {
                ref[ny][nx] = 1;
                qx[t] = nx, qy[t++] = ny;
            }
        }
    }
}

static void random_field(unsigned r)
{
    int x, y, k, n;
    srand(r);
    for (y = 0; y < GH; ++y)
        for (x = 0; x < GW; ++x) {
            if (x == 0 || y == 0 || x == GW - 1 || y == GH - 1)
                grid[y][x] = C_EDGE;
            else {
                int p = rand() % 100;
                int dens = r % 4 == 0 ? 0 : r % 4 == 1 ? 5 : r % 4 == 2 ? 25 : 45;
                grid[y][x] = p < dens ? (rand() & 1 ? C_WALL : C_LAND)
                           : (p % 7 == 0 ? C_CRATE : p % 11 == 0 ? C_ARMA : p % 13 == 0 ? C_ARMB : C_SEA);
            }
        }
    /* straight walls, as the game makes them */
    n = rand() % 12;
    for (k = 0; k < n; ++k) {
        if (rand() & 1) {
            x = 1 + rand() % (GW - 2);
            for (y = 1; y < GH - 1; ++y)
                if (rand() % 20)
                    grid[y][x] = C_WALL;
        } else {
            y = 1 + rand() % (GH - 2);
            for (x = 1; x < GW - 1; ++x)
                if (rand() % 20)
                    grid[y][x] = C_WALL;
        }
    }
}

static void test_fill(void)
{
    static const u16 budgets[] = { 1, 7, 100, 700, 60000 };
    unsigned r;
    int bad = 0, calls_max = 0;
    for (r = 0; r < 3000; ++r) {
        u8 sx[MAXBALLS], sy[MAXBALLS];
        int n, i, x, y, calls = 0;
        u16 budget = budgets[r % 5];
        random_field(r + 1);
        n = 1 + rand() % (MAXBALLS < FSTACK ? MAXBALLS : FSTACK);
        for (i = 0; i < n; ++i) {
            sx[i] = 1 + rand() % (GW - 2);
            sy[i] = 1 + rand() % (GH - 2);
        }
        bfs(sx, sy, n);
        fsp = 0;
        for (i = 0; i < n; ++i)
            fill_push(sx[i], sy[i]);
        while (!fill_run(budget))
            ++calls;
        if (calls > calls_max)
            calls_max = calls;
        for (y = 0; y < GH; ++y)
            for (x = 0; x < GW; ++x) {
                int m = !!(grid[y][x] & MARK);
                if (m != ref[y][x]) {
                    if (!bad++)
                        printf("FAIL fill: field %u, card %d %d: marked %d, the reference %d\n", r, x, y, m, ref[y][x]);
                }
                if ((grid[y][x] & KIND) > C_EDGE)
                    bad++;
            }
    }
    ++checks;
    if (bad)
        ++fails;
    printf("ok    fill: 3000 random fields (walls, land, crates, arms), 1-%d seeds, budgets 1-60000 (up to %d calls): "
           "the marks equal a breadth-first search's%s\n", MAXBALLS, calls_max + 1,
           FSTACK < 64 ? " (a stack of 8 seeds: overflows rescanned)" : "");
}

/*---------------------------------------------------------------- the game */

static int count(u8 kind)
{
    int x, y, n = 0;
    for (y = 0; y < GH; ++y)
        for (x = 0; x < GW; ++x)
            n += (grid[y][x] & KIND) == kind;
    return n;
}

static int marks(void)
{
    int x, y, n = 0;
    for (y = 0; y < GH; ++y)
        for (x = 0; x < GW; ++x)
            n += !!(grid[y][x] & MARK);
    return n;
}

/* until the wall is done and its capture finished */
static int settle(int max)
{
    int n = 0;
    while (n < max && state == S_PLAY && game_busy()) {
        game_step();
        ++n;
    }
    /* (a capture's request is taken at the next step) */
    game_step();
    while (n < max && state == S_PLAY && cap_state != CAP_IDLE) {
        game_step();
        ++n;
    }
    return n;
}

static void one_ball(u16 x, u16 y, u8 dir, u8 kind)
{
    nballs = 1;
    bx[0] = x;
    by[0] = y;
    bdir[0] = dir;
    bkind[0] = kind;
    bspd[0] = kind == K_SLOW ? 4 : kind == K_MED ? 6 : 8;
    bacc[0] = 0;
}

#if FSTACK >= 64
static void test_capture(void)
{
    u32 s0;
    int land;
    game_new(1234);
    CHECK(state == S_PLAY && level == 1 && lives == 3 && nballs == 2 && target == 60, "a new game: level 1, 3 lives, 2 buoys, target 60%%");
    CHECK(count(C_SEA) == INTERIOR && count(C_EDGE) == 2 * GW + 2 * (GH - 2), "the field: %d sea cards", count(C_SEA));
    one_ball(100, 100, 0, K_SLOW);
    s0 = score;
    CHECK(game_place(40, 20, 1), "a vertical wall at column 40");
    CHECK(!game_place(50, 20, 0), "no second wall while one grows");
    settle(2000);
    CHECK(!arm_on[0] && !arm_on[1], "the arms set");
    CHECK(count(C_WALL) == GH - 2, "the wall: %d cards", count(C_WALL));
    land = count(C_LAND);
    CHECK(land == 38 * (GH - 2), "the east side calmed: %d cards (38 columns)", land);
    CHECK(marks() == 0, "no marks left");
    CHECK(filled == land + GH - 2 && calm == (u8)((u32)filled * 100 / INTERIOR) && calm == 50, "calm %u%% (filled %u)", calm, filled);
    CHECK(score - s0 == (u32)land * 2, "the capture's score: %lu (cards x level x 2 for 400 or more)", (unsigned long)(score - s0));
    CHECK(lives == 3 && state == S_PLAY, "no life lost");
    CHECK(bx[0] < 40 * 8, "the buoy stays west of the wall");
    CHECK(!game_place(60, 20, 1), "no wall on land");
    CHECK(!game_place(40, 30, 0), "no wall on a wall");

    /* a second wall: 62% clears the level (target 60) */
    one_ball(40, 200, 1, K_SLOW);
    CHECK(game_place(30, 10, 1), "a second wall at column 30");
    settle(2000);
    CHECK(count(C_LAND) == land + 9 * (GH - 2), "columns 31-39 calmed: %d", count(C_LAND) - land);
    CHECK(calm == 62, "calm %u%%", calm);
    CHECK(state == S_CLEAR, "the level cleared (state %u)", state);
    CHECK(lives == 4, "a life for the level: %u", lives);
    {
        int i;
        for (i = 0; i < 400 && state == S_CLEAR; ++i)
            game_step();
    }
    CHECK(state == S_PLAY && level == 2 && nballs == 3 && target == 65 && count(C_SEA) == INTERIOR,
          "level 2: 3 buoys, target 65%%, the sea back");
    CHECK(bkind[0] == K_MED || bkind[1] == K_MED || bkind[2] == K_MED, "level 2 has a medium buoy");
    printf("ok    capture: a vertical wall sets, the buoy-free side calms (%d cards, 50%%); a second wall: 62%%, level cleared, level 2\n", land);
}

static void test_break(void)
{
    int i, armcards;
    game_new(99);
    /* the buoy moving right along row 20-21: a vertical wall at 30 in its way */
    one_ball(200, 160, 0, K_FAST);          /* right, down */
    CHECK(game_place(30, 21, 1), "the wall's seed");
    /* the buoy goes west: rows ~20, the arm grows over them */
    one_ball(260, 164, 1, K_FAST);          /* left, down: towards column 30 */
    for (i = 0; i < 300 && (arm_on[0] || arm_on[1]); ++i)
        game_step();
    armcards = count(C_ARMA) + count(C_ARMB);
    CHECK(lives == 2, "a life lost: %u", lives);
    CHECK(armcards == 0, "no growing cards left (%d)", armcards);
    CHECK(n_sfx[SFX_BREAK] >= 1, "the break's sound");
    CHECK(count(C_WALL) + count(C_SEA) == INTERIOR, "the broken arm is sea again, the other set: walls %d", count(C_WALL));
    printf("ok    break: a buoy into a growing arm: the arm sinks (sea again), a life lost (3 -> 2), the other arm sets (%d cards)\n",
           count(C_WALL));

    /* the game ends with the last life */
    settle(2000);
    lives = 1;
    one_ball(300, 200, 0, K_SLOW);
    CHECK(game_place(39, 26, 0), "a wall onto the buoy");
    game_step();
    CHECK(state == S_OVER && lives == 0, "game over (state %u, lives %u)", state, lives);
    CHECK(!game_place(10, 10, 0), "no walls after the game");
    printf("ok    game over: the last life lost on a wall placed over a buoy\n");
}

static void test_crates(void)
{
    game_new(7);
    one_ball(60, 60, 0, K_SLOW);
    grid[30][60] = C_CRATE;
    crx[0] = 60, cry[0] = 30, crk[0] = P_LIFEBUOY, crlife[0] = 900, ncrates = 1;
    grid[40][70] = C_CRATE;
    crx[1] = 70, cry[1] = 40, crk[1] = P_ANCHOR, crlife[1] = 900, ncrates = 2;
    CHECK(game_place(50, 25, 1), "a wall");
    settle(2000);
    CHECK(lives == 4 && anchor > 0 && ncrates == 0, "two crates calmed in: a life (%u), the anchor (%u)", lives, anchor);
    {
        u16 x = bx[0], y = by[0];
        game_step();
        game_step();
        CHECK(bx[0] == x && by[0] == y, "the anchor holds the buoy");
    }
    while (anchor)
        game_step();
    {
        u16 x = bx[0];
        int i;
        for (i = 0; i < 8; ++i)
            game_step();
        CHECK(bx[0] != x, "the buoy drifts again");
    }
    /* concrete: walls grow every frame */
    game_new(8);
    one_ball(60, 60, 0, K_SLOW);
    concrete = 3;
    CHECK(game_place(40, 26, 1), "a wall with concrete");
    {
        int n = 0;
        while (arm_on[0] || arm_on[1])
            game_step(), ++n;
        CHECK(n <= 27, "quick-set: %d frames for 51 cards", n);
        CHECK(concrete == 2, "two quick walls left");
    }
    /* crates come by themselves, and go */
    game_new(9);
    {
        int i, seen = 0;
        for (i = 0; i < 1400; ++i) {
            game_step();
            if (ncrates > seen)
                seen = ncrates;
        }
        CHECK(seen >= 1 && count(C_CRATE) == ncrates, "crates float up (%d at most)", seen);
    }
    printf("ok    crates: a lifebuoy and an anchor calmed in (a life, the buoys held); quick-set concrete; crates come\n");
}

static void test_tide(void)
{
    int i;
    game_new(5);
    tide = 2;
    for (i = 0; i < 130; ++i)
        game_step();
    CHECK(tide == 0 && storm && bspd[0] == 6, "the tide out: a storm, the buoys faster (%u)", bspd[0]);
    printf("ok    tide: at 0 the storm speeds the buoys up\n");
}

/* the same inputs, the same game */
static u16 run(u16 s, u16 *hs)
{
    int i, k = 0;
    static const struct { int at; u8 x, y, v; } in[] = {
        { 30, 20, 10, 1 }, { 300, 60, 30, 0 }, { 600, 10, 40, 1 }, { 900, 70, 12, 1 }, { 1300, 40, 40, 0 },
    };
    game_new(s);
    for (i = 0; i < 2000; ++i) {
        if (k < 5 && i == in[k].at)
            game_place(in[k].x, in[k].y, in[k].v), ++k;
        game_step();
        if (i % 100 == 0)
            *hs++ = game_hash_balls();
    }
    return game_hash();
}

static void test_determinism(void)
{
    u16 a[20], b[20], c[20];
    u16 h1 = run(4242, a), h2 = run(4242, b), h3 = run(4243, c);
    CHECK(h1 == h2 && !memcmp(a, b, sizeof a), "the same seed and inputs: the same game");
    CHECK(h1 != h3, "another seed: another game");
    printf("ok    determinism: 2000 steps with 5 walls twice: hash %04X both times (another seed %04X)\n", h1, h3);
}

#endif

int main(void)
{
    test_fill();
#if FSTACK >= 64
#define GAME_TESTS
    test_capture();
    test_break();
    test_crates();
    test_tide();
    test_determinism();
#endif
    if (fails) {
        printf("FAIL  %d of %d checks\n", fails, checks);
        return 1;
    }
    printf("ok    %d checks\n", checks);
    return 0;
}
