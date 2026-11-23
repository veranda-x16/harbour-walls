/* Harbour Walls: the rules (see engine.h) */
#include "engine.h"

u8 grid[GH][GW];

u8 nballs;
u16 bx[MAXBALLS], by[MAXBALLS];
u8 bdir[MAXBALLS], bspd[MAXBALLS], bacc[MAXBALLS], bkind[MAXBALLS];

u8 arm_on[2], arm_x0[2], arm_y0[2], arm_len[2];
u8 wall_vert;

u8 ncrates;
u8 crx[MAXCRATES], cry[MAXCRATES], crk[MAXCRATES];
u16 crlife[MAXCRATES];

u8 state, level, lives, target, calm, dirty;
u32 score;
u16 tide, filled, step_no, clear_wait, anchor;
u8 concrete, storm, cap_state;
u16 cap_cells;
u16 seed;
u16 breaks;

static u8 cap_req, cap_row, tick60, grow_tick;
u8 *prow;

#ifndef FILL_ASM
/* from x on in row prow: the marks cleared up to the next free card
   not marked (sea or a crate); its column, or GW - 1 (fill.s has it in
   assembly) */
u8 paint_scan(u8 x)
{
    u8 c;
    for (; x < GW - 1; ++x) {
        c = prow[x];
        if (c & MARK)
            prow[x] = c & KIND;
        else if (c < C_ARMA)
            break;
    }
    return x;
}
#endif
static u16 crate_tick;

/* tuning */
#define GROW_FRAMES   3         /* frames a card, an arm */
#define GROW_FAST     1         /* with quick-set concrete */
#define FILL_BUDGET 400         /* cards marked a frame */
#define PAINT_ROWS    4         /* rows calmed a frame */
#define CRATE_EVERY 420         /* frames between crates */
#define CRATE_LIFE  900
#define ANCHOR_TIME 240
#define CLEAR_WAIT  180

static const u8 kind_speed[3] = { 4, 6, 8 };

/* a wall growing or its capture still to come */
u8 game_busy(void)
{
    return arm_on[0] || arm_on[1] || cap_state != CAP_IDLE || cap_req;
}

u16 rnd(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return seed;
}

/* the calmed share in per cent */
static void recalc(void)
{
    calm = (u8)(((u32)filled * 100) / INTERIOR);
    dirty |= D_CALM;
}

void game_new(u16 s)
{
    seed = s ? s : 0x5EED;
    score = 0;
    lives = 3;
    breaks = 0;
    step_no = 0;
    game_level(1);
}

void game_level(u8 l)
{
    u8 x, y, i, n, nf, nm;
    level = l;
    for (y = 0; y < GH; ++y)
        for (x = 0; x < GW; ++x)
            grid[y][x] = (x == 0 || y == 0 || x == GW - 1 || y == GH - 1) ? C_EDGE : C_SEA;
    n = l + 1;
    if (n > MAXBALLS)
        n = MAXBALLS;
    nf = l / 3;                 /* fast ones */
    nm = (l + 1) / 3;           /* medium ones */
    nballs = n;
    for (i = 0; i < n; ++i) {
        bx[i] = 8 + (rnd() % ((GW - 2) * 8 - BALL));
        by[i] = 8 + (rnd() % ((GH - 2) * 8 - BALL));
        bdir[i] = (u8)(rnd() & 3);
        bkind[i] = i < nf ? K_FAST : i < nf + nm ? K_MED : K_SLOW;
        bspd[i] = kind_speed[bkind[i]];
        bacc[i] = 0;
    }
    arm_on[0] = arm_on[1] = 0;
    arm_len[0] = arm_len[1] = 0;
    wall_vert = 0;
    ncrates = 0;
    cap_state = CAP_IDLE;
    cap_req = 0;
    filled = 0;
    calm = 0;
    anchor = 0;
    storm = 0;
    concrete = 0;
    tick60 = 0;
    grow_tick = 0;
    crate_tick = 0;
    tide = 75 + 15 * (u16)l;
    if (tide > 240)
        tide = 240;
    target = 60 + 5 * (l - 1);
    if (l > 5)
        target = 80;
    state = S_PLAY;
    dirty = D_ALL;
    plat_field();
}

/*---------------------------------------------------------------- crates */

static u8 crate_at(u8 x, u8 y)
{
    u8 i;
    for (i = 0; i < ncrates; ++i)
        if (crx[i] == x && cry[i] == y)
            return i;
    return 0xFF;
}

static void crate_drop(u8 i)
{
    --ncrates;
    crx[i] = crx[ncrates];
    cry[i] = cry[ncrates];
    crk[i] = crk[ncrates];
    crlife[i] = crlife[ncrates];
}

/* a crate inside a capture: its power */
static void crate_collect(u8 x, u8 y)
{
    u8 i = crate_at(x, y);
    if (i == 0xFF)
        return;
    switch (crk[i]) {
    case P_CONCRETE:
        concrete = 3;
        break;
    case P_ANCHOR:
        anchor = ANCHOR_TIME;
        break;
    case P_LIFEBUOY:
        if (lives < 9)
            ++lives;
        dirty |= D_LIVES;
        break;
    default:
        score += 250 * (u32)level;
        dirty |= D_SCORE;
    }
    dirty |= D_POWER;
    crate_drop(i);
    plat_sfx(SFX_POWER);
}

static u8 ball_on(u8 x, u8 y);

static void crate_spawn(void)
{
    u8 t, x, y, r;
    for (t = 0; t < 8; ++t) {
        x = 1 + (u8)(rnd() % (GW - 2));
        y = 1 + (u8)(rnd() % (GH - 2));
        if (grid[y][x] != C_SEA || ball_on(x, y))
            continue;
        r = (u8)(rnd() & 15);
        crx[ncrates] = x;
        cry[ncrates] = y;
        crk[ncrates] = r < 6 ? P_CONCRETE : r < 11 ? P_ANCHOR : r < 13 ? P_LIFEBUOY : P_PEARL;
        crlife[ncrates] = CRATE_LIFE;
        grid[y][x] = C_CRATE;
        plat_crate(x, y, crk[ncrates]);
        ++ncrates;
        plat_sfx(SFX_CRATE);
        return;
    }
}

static void crates_age(void)
{
    u8 i = 0;
    while (i < ncrates) {
        if (--crlife[i] == 0) {
            if ((grid[cry[i]][crx[i]] & KIND) == C_CRATE) {
                grid[cry[i]][crx[i]] = C_SEA | (grid[cry[i]][crx[i]] & MARK);
                plat_rect(crx[i], cry[i], 1, 1, C_SEA);
            }
            crate_drop(i);
        } else
            ++i;
    }
}

/*---------------------------------------------------------------- the wall */

/* is a buoy over card (x, y)? */
static u8 ball_on(u8 x, u8 y)
{
    u8 i;
    u16 px = (u16)x << 3, py = (u16)y << 3;
    for (i = 0; i < nballs; ++i)
        if (bx[i] < px + 8 && bx[i] + BALL > px && by[i] < py + 8 && by[i] + BALL > py)
            return 1;
    return 0;
}

/* arm a's cards as a rectangle */
static void arm_rect(u8 a, u8 kind)
{
    u8 x = arm_x0[a], y = arm_y0[a], n = arm_len[a];
    if (!n)
        return;
    if (wall_vert) {
        if (a == 0)
            y = y - n + 1;
        plat_rect(x, y, 1, n, kind);
    } else {
        if (a == 0)
            x = x - n + 1;
        plat_rect(x, y, n, 1, kind);
    }
}

static void arm_fill(u8 a, u8 kind)
{
    u8 x = arm_x0[a], y = arm_y0[a], n = arm_len[a];
    while (n--) {
        grid[y][x] = kind;
        if (wall_vert)
            y += a ? 1 : -1;
        else
            x += a ? 1 : -1;
    }
}

static void check_clear(void)
{
    u16 b;
    if (state != S_PLAY || arm_on[0] || arm_on[1] || cap_state != CAP_IDLE || cap_req)
        return;
    if (calm < target)
        return;
    b = tide * 5 * level;
    score += b + (u32)(calm - target) * 100 * level;
    if (lives < 9)
        ++lives;
    state = S_CLEAR;
    clear_wait = CLEAR_WAIT;
    dirty |= D_SCORE | D_LIVES;
    plat_sfx(SFX_CLEAR);
}

/* a buoy touched arm a while it grew: it sinks */
static void arm_break(u8 a)
{
    if (!arm_on[a])
        return;
    arm_on[a] = 0;
    ++breaks;
    arm_fill(a, C_SEA);
    arm_rect(a, C_SEA);
    if (lives)
        --lives;
    dirty |= D_LIVES;
    plat_sfx(SFX_BREAK);
    if (!lives) {
        if (arm_on[!a]) {       /* the other arm sinks with the game */
            arm_on[!a] = 0;
            arm_fill(!a, C_SEA);
            arm_rect(!a, C_SEA);
        }
        state = S_OVER;
        plat_sfx(SFX_OVER);
        return;
    }
    check_clear();
}

/* arm a reached something solid: it sets */
static void arm_set(u8 a)
{
    arm_on[a] = 0;
    if (!arm_len[a]) {
        check_clear();
        return;
    }
    arm_fill(a, C_WALL);
    arm_rect(a, C_WALL);
    filled += arm_len[a];
    recalc();
    cap_req = 1;
    plat_sfx(SFX_SET);
}

u8 game_place(u8 x, u8 y, u8 vert)
{
    u8 i;
    if (state != S_PLAY || arm_on[0] || arm_on[1] || cap_state != CAP_IDLE || cap_req)
        return 0;
    if (x < 1 || x > GW - 2 || y < 1 || y > GH - 2 || grid[y][x] & KIND & ~1)
        return 0;               /* only on open sea (or a crate) */
    if ((grid[y][x] & KIND) == C_CRATE && (i = crate_at(x, y)) != 0xFF)
        crate_drop(i);          /* the crate sinks under the wall */
    wall_vert = vert;
    arm_x0[0] = x;
    arm_y0[0] = y;
    arm_len[0] = 1;
    arm_on[0] = 1;
    arm_x0[1] = vert ? x : x + 1;
    arm_y0[1] = vert ? y + 1 : y;
    arm_len[1] = 0;
    arm_on[1] = 1;
    grid[y][x] = C_ARMA;
    plat_rect(x, y, 1, 1, C_ARMA);
    grow_tick = 0;
    plat_sfx(SFX_PLACE);
    if (ball_on(x, y))
        arm_break(0);
    return 1;
}

/* one card more on each growing arm */
static void grow(void)
{
    u8 a, x, y, n, c, i;
    for (a = 0; a < 2; ++a) {
        if (!arm_on[a])
            continue;
        x = arm_x0[a];
        y = arm_y0[a];
        n = arm_len[a];
        if (wall_vert)
            y = a ? y + n : y - n;
        else
            x = a ? x + n : x - n;
        c = grid[y][x];
        if (SOLID(c)) {
            arm_set(a);
            continue;
        }
        if ((c & KIND) == C_CRATE) {
            i = crate_at(x, y);
            if (i != 0xFF)
                crate_drop(i);  /* crushed under the wall */
        }
        grid[y][x] = a ? C_ARMB : C_ARMA;
        ++arm_len[a];
        plat_rect(x, y, 1, 1, a ? C_ARMB : C_ARMA);
        if (ball_on(x, y))
            arm_break(a);
    }
    if (!arm_on[0] && !arm_on[1] && concrete)
        --concrete, dirty |= D_POWER;
}

/*---------------------------------------------------------------- buoys */

/* what is in column col, rows r0..r1 / row r, columns c0..c1: bit 0
   solid, bit 1 arm 0, bit 2 arm 1 */
static u8 hits;
static void look(u8 c)
{
    c &= KIND;
    if (c >= C_WALL)
        hits |= 1;
    else if (c == C_ARMA)
        hits |= 3;
    else if (c == C_ARMB)
        hits |= 5;
}

static void hit_arms(void)
{
    if (hits & 2)
        arm_break(0);
    if (hits & 4)
        arm_break(1);
}

static void move_ball(u8 i)
{
    u8 a, s, d, e0, e1, r, r1, c, c1;
    u16 x, y, n;
    a = bacc[i] + bspd[i];
    s = a >> 2;
    bacc[i] = a & 3;
    if (!s)
        return;
    d = bdir[i];
    x = bx[i];
    y = by[i];
    /* across */
    if (d & 1) {
        n = x - s;
        e0 = (u8)(x >> 3);
        e1 = (u8)(n >> 3);
    } else {
        n = x + s;
        e0 = (u8)((x + BALL - 1) >> 3);
        e1 = (u8)((n + BALL - 1) >> 3);
    }
    if (e0 != e1) {
        hits = 0;
        r1 = (u8)((y + BALL - 1) >> 3);
        for (r = (u8)(y >> 3); r <= r1; ++r)
            look(grid[r][e1]);
        if (hits) {
            bdir[i] = d ^= 1;
            n = x;
            hit_arms();
        }
    }
    bx[i] = x = n;
    /* down */
    if (d & 2) {
        n = y - s;
        e0 = (u8)(y >> 3);
        e1 = (u8)(n >> 3);
    } else {
        n = y + s;
        e0 = (u8)((y + BALL - 1) >> 3);
        e1 = (u8)((n + BALL - 1) >> 3);
    }
    if (e0 != e1) {
        hits = 0;
        c1 = (u8)((x + BALL - 1) >> 3);
        for (c = (u8)(x >> 3); c <= c1; ++c)
            look(grid[e1][c]);
        if (hits) {
            bdir[i] = d ^ 2;
            n = y;
            hit_arms();
        }
    }
    by[i] = n;
}

/*---------------------------------------------------------------- capture */

/* row r of the capture: marks cleared, the unmarked sea calmed */
#ifndef HW_BENCH
static
#endif
void paint_row(u8 r)
{
    u8 *row = grid[r];
    u8 x = 1, x0, c;
    prow = row;
    while ((x = paint_scan(x)) < GW - 1) {
        x0 = x;
        c = row[x];
        do {
            if (c == C_CRATE)
                crate_collect(x, r);
            row[x] = C_LAND;
            c = row[++x];
        } while (c < C_ARMA);
        plat_rect(x0, r, x - x0, 1, C_LAND);
        cap_cells += x - x0;
    }
}

static void capture(void)
{
    u8 i, n;
    if (cap_state == CAP_IDLE) {
        if (!cap_req)
            return;
        cap_req = 0;
        fsp = 0;
        for (i = 0; i < nballs; ++i)
            fill_push((u8)((bx[i] + BALL / 2) >> 3), (u8)((by[i] + BALL / 2) >> 3));
        cap_state = CAP_FILL;
        return;                 /* (the fill from the next frame on: the wall drew in this one) */
    }
    if (cap_state == CAP_FILL) {
        if (fill_run(FILL_BUDGET)) {
            cap_state = CAP_PAINT;
            cap_row = 1;
            cap_cells = 0;
        }
        return;
    }
    for (n = 0; n < PAINT_ROWS && cap_row < GH - 1; ++n)
        paint_row(cap_row++);
    if (cap_row < GH - 1)
        return;
    cap_state = CAP_IDLE;
    if (cap_cells) {
        filled += cap_cells;
        score += (u32)cap_cells * level * (cap_cells >= 400 ? 2 : 1);
        dirty |= D_SCORE;
        recalc();
        plat_sfx(SFX_CAPTURE);
    }
    check_clear();
}

/*---------------------------------------------------------------- a frame */

void game_step(void)
{
    u8 i;
    ++step_no;
    if (state == S_CLEAR) {
        if (--clear_wait == 0)
            game_level(level + 1);
        return;
    }
    if (state != S_PLAY)
        return;
    if (anchor) {
        if (--anchor == 0)
            dirty |= D_POWER;
    } else
        for (i = 0; i < nballs && state == S_PLAY; ++i)
            move_ball(i);
    if (state != S_PLAY)
        return;
    if (arm_on[0] || arm_on[1]) {
        if (++grow_tick >= (concrete ? GROW_FAST : GROW_FRAMES)) {
            grow_tick = 0;
            grow();
        }
    }
    if (state != S_PLAY)
        return;
    capture();
    if (ncrates)
        crates_age();
    if (cap_state == CAP_IDLE && ++crate_tick >= CRATE_EVERY) {
        crate_tick = 0;
        if (ncrates < MAXCRATES)
            crate_spawn();
    }
    if (++tick60 == 60) {
        tick60 = 0;
        if (tide) {
            --tide;
            dirty |= D_TIDE;
            if (!tide) {
                storm = 1;
                for (i = 0; i < nballs; ++i)
                    bspd[i] += 2;
                plat_sfx(SFX_STORM);
            }
        }
    }
}

/*---------------------------------------------------------------- checksums */

static u16 h;
static void mix(u8 b)
{
    h = ((h << 3) | (h >> 13)) + b;
}

u16 game_hash_balls(void)
{
    u8 i;
    h = step_no;
    for (i = 0; i < nballs; ++i) {
        mix((u8)bx[i]);
        mix((u8)(bx[i] >> 8));
        mix((u8)by[i]);
        mix((u8)(by[i] >> 8));
        mix(bdir[i]);
        mix(bspd[i]);
    }
    mix(arm_len[0]);
    mix(arm_len[1]);
    mix(lives);
    mix((u8)score);
    mix((u8)(score >> 8));
    return h;
}

u16 game_hash(void)
{
    /* the grid by Fletcher's sums (cheap on a 6502), then the rest */
    u8 a = 0, b = 0, *p = &grid[0][0];
    u16 n = (u16)GW * GH, g = game_hash_balls();
    while (n--) {
        a += *p++;
        b += a;
    }
    h = g;
    mix(a);
    mix(b);
    mix(level);
    mix(calm);
    mix(ncrates);
    mix((u8)(score >> 16));
    mix((u8)tide);
    return h;
}

#ifndef __CC65__
/*---------------------------------------------------------------- the host's
   snapshots of the whole state (the planner tries walls and goes back) */
#include <string.h>
extern u8 fill_lostf;
static const struct { void *p; unsigned n; } vars[] = {
    { grid, sizeof grid }, { &nballs, 1 }, { bx, sizeof bx }, { by, sizeof by }, { bdir, sizeof bdir },
    { bspd, sizeof bspd }, { bacc, sizeof bacc }, { bkind, sizeof bkind }, { arm_on, 2 }, { arm_x0, 2 },
    { arm_y0, 2 }, { arm_len, 2 }, { &wall_vert, 1 }, { &ncrates, 1 }, { crx, sizeof crx }, { cry, sizeof cry },
    { crk, sizeof crk }, { crlife, sizeof crlife }, { &state, 1 }, { &level, 1 }, { &lives, 1 }, { &target, 1 },
    { &calm, 1 }, { &dirty, 1 }, { &score, sizeof score }, { &tide, 2 }, { &filled, 2 }, { &step_no, 2 },
    { &clear_wait, 2 }, { &anchor, 2 }, { &concrete, 1 }, { &storm, 1 }, { &cap_state, 1 }, { &cap_cells, 2 },
    { &seed, 2 }, { &breaks, 2 }, { &cap_req, 1 }, { &cap_row, 1 }, { &tick60, 1 }, { &grow_tick, 1 }, { &crate_tick, 2 },
    { fsx, sizeof fsx }, { fsy, sizeof fsy }, { &fsp, 1 }, { &fill_lostf, 1 },
};

unsigned game_state_size(void)
{
    unsigned i, n = 0;
    for (i = 0; i < sizeof vars / sizeof vars[0]; ++i)
        n += vars[i].n;
    return n;
}

void game_save(u8 *b)
{
    unsigned i;
    for (i = 0; i < sizeof vars / sizeof vars[0]; ++i) {
        memcpy(b, vars[i].p, vars[i].n);
        b += vars[i].n;
    }
}

void game_load(const u8 *b)
{
    unsigned i;
    for (i = 0; i < sizeof vars / sizeof vars[0]; ++i) {
        memcpy(vars[i].p, b, vars[i].n);
        b += vars[i].n;
    }
}
#endif
