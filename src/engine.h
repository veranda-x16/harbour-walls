/* Harbour Walls: the game's rules, in portable C (the X16 build and the
   host's tests and replay compile the same file).

   The field is a grid of 8 x 8 cards. Buoys (16 x 16 pixels) drift
   diagonally and bounce off solid cards. A click starts a sea wall: two
   arms grow from the chosen card in opposite directions, one card at a
   time. A buoy that touches an arm while it grows breaks that arm (the
   arm sinks, a life is lost); an arm that reaches something solid sets
   into stone. Each wall that sets is followed by a capture: a flood fill
   from every buoy marks the sea they can still reach; the rest is calmed
   into harbour land. A level is cleared when the calmed share of the
   harbour reaches the level's target. */
#ifndef ENGINE_H
#define ENGINE_H

#ifdef __CC65__
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;
typedef signed char s8;
#else
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
#endif

/* the grid: GW x GH cards, the edge solid; screen card row of grid row 0 */
#define GW 80
#define GH 53
#define FIELD_Y 3
#define INTERIOR ((u16)(GW - 2) * (GH - 2))     /* 3978 cards */

/* card kinds (grid values; bit 7 is the flood fill's mark) */
#define C_SEA    0
#define C_CRATE  1      /* sea with a floating crate (passable) */
#define C_ARMA   2      /* the growing arm going left / up */
#define C_ARMB   3      /* the growing arm going right / down */
#define C_WALL   4      /* a sea wall, set */
#define C_LAND   5      /* calmed harbour */
#define C_EDGE   6      /* the quay round the field */
#define MARK     0x80
#define KIND     0x7F
#define SOLID(c) (((c) & KIND) >= C_WALL)

#define MAXBALLS 12
#define BALL     16     /* pixels */

/* buoy classes (their speed in quarter pixels a frame) */
#define K_SLOW   0
#define K_MED    1
#define K_FAST   2

/* crates (power-ups) */
#define P_CONCRETE 0    /* quick-set concrete: the next 3 walls grow twice as fast */
#define P_ANCHOR   1    /* every buoy held still for 4 seconds */
#define P_LIFEBUOY 2    /* a life */
#define P_PEARL    3    /* 250 points x the level */
#define MAXCRATES  3

/* game states */
#define S_PLAY   0
#define S_CLEAR  1      /* the level cleared: a pause, then the next */
#define S_OVER   2

/* capture phases */
#define CAP_IDLE  0
#define CAP_FILL  1
#define CAP_PAINT 2

/* what the platform should redraw (dirty bits) */
#define D_SCORE  0x01
#define D_LIVES  0x02
#define D_TIDE   0x04
#define D_CALM   0x08
#define D_LEVEL  0x10
#define D_POWER  0x20
#define D_ALL    0x3F

/* sounds the engine asks for */
#define SFX_PLACE    1
#define SFX_GROW     2
#define SFX_SET      3
#define SFX_BREAK    4
#define SFX_CAPTURE  5
#define SFX_CRATE    6
#define SFX_POWER    7
#define SFX_CLEAR    8
#define SFX_OVER     9
#define SFX_STORM   10
#define SFX_BOUNCE  11

extern u8 grid[GH][GW];

/* buoys */
extern u8 nballs;
extern u16 bx[MAXBALLS], by[MAXBALLS];      /* field pixels, top left */
extern u8 bdir[MAXBALLS];                   /* bit 0: x going left, bit 1: y going up */
extern u8 bspd[MAXBALLS];                   /* quarter pixels a frame */
extern u8 bacc[MAXBALLS];
extern u8 bkind[MAXBALLS];

/* the arms: 0 left/up, 1 right/down */
extern u8 arm_on[2];                        /* growing */
extern u8 arm_x0[2], arm_y0[2], arm_len[2];
extern u8 wall_vert;                        /* the wall's orientation */

/* crates */
extern u8 ncrates;
extern u8 crx[MAXCRATES], cry[MAXCRATES], crk[MAXCRATES];
extern u16 crlife[MAXCRATES];

/* the game */
extern u8 state, level, lives, target, calm, dirty;
extern u32 score;
extern u16 tide;            /* seconds of tide left */
extern u16 filled;          /* interior cards walled or calmed */
extern u16 step_no;         /* steps since the game started (wraps) */
extern u16 clear_wait;
extern u8 concrete;         /* fast walls left */
extern u16 anchor;          /* frames the buoys stay held */
extern u8 storm;            /* the tide ran out: the buoys sped up */
extern u8 cap_state;
extern u16 cap_cells;       /* the last capture's size */
extern u16 seed;
extern u16 breaks;          /* arms broken this game */

/* the game's random numbers (xorshift, 16 bits) */
u16 rnd(void);

void game_new(u16 seed);
void game_level(u8 level);
/* a wall at card (x, y), vertical or not: 1 if it starts */
u8 game_place(u8 x, u8 y, u8 vert);
/* one frame (1/60 s) */
void game_step(void);
/* a wall grows, or its capture is still to come */
u8 game_busy(void);
/* the state's checksum (the grid, the buoys, the score) */
u16 game_hash(void);
u16 game_hash_balls(void);

/* the flood fill (fill.c, or the X16's fill.s): from the stack's seeds,
   mark (MARK) every passable card reachable, at most about budget cards
   a call; 1 when it is done. The stack must hold the first seeds (one a
   buoy); a seed pushed later onto a full one is found again by a rescan */
#ifndef FSTACK
#define FSTACK 255
#endif
extern u8 fsx[FSTACK], fsy[FSTACK];
extern u8 fsp;
u8 fill_run(u16 budget);
extern u8 *prow;
u8 paint_scan(u8 x);
void fill_push(u8 x, u8 y);

#ifndef __CC65__
/* the host's snapshots (engine.c) */
unsigned game_state_size(void);
void game_save(u8 *b);
void game_load(const u8 *b);
#endif

/* the platform's side (x16/draw.c, host/plat.c) */
void plat_rect(u8 x, u8 y, u8 w, u8 h, u8 kind);    /* grid cards */
void plat_crate(u8 x, u8 y, u8 kind);
void plat_field(void);                               /* the whole field */
void plat_sfx(u8 id);

#endif
