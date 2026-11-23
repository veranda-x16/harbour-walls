/* Harbour Walls for Veranda: the platform's side. The rules are
   src/engine.c; here the screen (the field: a full-colour picture a kind
   of card in VERA's 4 bpp tiles under the bitmap, Veranda's vr_pic,
   DECISIONS V105, tools/mkpic.py; the panel and the banners: text in the
   bitmap on cards of a paper colour, as before),
   the buoys (VERA sprites), the pointer, the menus, the keys, the sound
   (sfx.c, from the interrupt), the panel, the high scores.

   Every frame the interrupt counts; the main loop's pass (appMain) runs
   one engine step a frame that went by (at most 3 to catch up), then
   moves the sprites and draws what the engine changed. */
#include <veranda.h>
#include <vr.h>
#include <theme.h>
#include <vrpath.h>
#include <vrpic.h>
#include <string.h>
#include "../src/engine.h"
#include "hw.h"
#include "gfx.h"
#include "pic.h"

/*---------------------------------------------------------------- VERA */

#define VADDR_L (*(volatile u8 *)0x9F20)
#define VADDR_M (*(volatile u8 *)0x9F21)
#define VADDR_H (*(volatile u8 *)0x9F22)
#define VDATA0  (*(volatile u8 *)0x9F23)
#define VCTRL   (*(volatile u8 *)0x9F25)

#define SPR_FIRST 8                     /* the buoys' sprites: 8-19 */
#define SPR_PIC   0x17C00UL             /* their picture: sprite slot 2 */
#define PAL_FIRST 2                     /* palette offsets 2-5 (entries 32-95) */

static void vat(u32 a)
{
    VCTRL = 0;
    VADDR_L = (u8)a;
    VADDR_M = (u8)(a >> 8);
    VADDR_H = (u8)(a >> 16) | 0x10;     /* increment 1 */
}

/* the palette as it was, put back at the end: entries 0-31 here, 32-255
   by vr_pic_off (Veranda keeps them from vr_pic_on, which comes after
   theme_palette's 0-15: they are still as found then) */
static u8 pal_keep[64];

static void pal_save(void)
{
    u8 i;
    vat(0x1FA00UL);
    for (i = 0; i < 64; ++i)
        pal_keep[i] = VDATA0;
}

static void pal_restore(void)
{
    u8 i;
    vat(0x1FA00UL);
    for (i = 0; i < 64; ++i)
        VDATA0 = pal_keep[i];
}

/* a CRC of the palette as VERA has it now (the test builds' check) */
u16 pal_crc(void)
{
    u16 i, c = 0xFFFF;
    u8 b, k;
    vat(0x1FA00UL);
    for (i = 0; i < 512; ++i) {
        b = VDATA0;
        c ^= (u16)b << 8;
        for (k = 0; k < 8; ++k)
            c = c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1;
    }
    return c;
}

static void sprites_palette(void)
{
    u8 i, k;
    vat(0x1FA00UL + 32 * PAL_FIRST);
    for (k = 0; k < 4; ++k)
        for (i = 0; i < 16; ++i) {
            u16 c = i < 6 ? buoy_pal[k][i] : 0;
            VDATA0 = (u8)c;
            VDATA0 = (u8)(c >> 8);
        }
}

static void sprites_init(void)
{
    u8 i;
    vat(SPR_PIC);
    for (i = 0; i < 128; ++i)
        VDATA0 = buoy_pic[i];
    sprites_palette();
    vat(0x1FC00UL + 8 * SPR_FIRST);
    for (i = 0; i < MAXBALLS; ++i) {
        VDATA0 = (u8)(SPR_PIC >> 5);
        VDATA0 = (u8)(SPR_PIC >> 13);   /* 4 bpp */
        VDATA0 = 0;
        VDATA0 = 0;
        VDATA0 = 0;
        VDATA0 = 0;
        VDATA0 = 0;                     /* off */
        VDATA0 = 0x50 | PAL_FIRST;      /* 16 x 16 */
    }
}

static u8 shown;                        /* the buoys' sprites are on */

void sprites_hide(void)
{
    u8 i;
    if (!shown)
        return;
    for (i = 0; i < MAXBALLS; ++i) {
        vat(0x1FC00UL + 8 * (SPR_FIRST + i) + 6);
        VDATA0 = 0;
    }
    shown = 0;
}

/* the buoys where the engine has them (hw.s's sprites_put) */
static void sprites_show(void)
{
    sprites_put();
    shown = 1;
}

/*---------------------------------------------------------------- the field */

#define G(k) (16 + (k))
#define G_MARK   G(12)
#define G_LIFE   G(13)
#define G_RULE   G(15)

/* pic.s: a run of map entries */
extern u8 mr_x, mr_y, mr_w, mr_h, mr_base, mr_row;
void map_run(void);

/* cards of a kind: their pictures' map entries (the bitmap there is empty) */
void plat_rect(u8 x, u8 y, u8 w, u8 h, u8 kind)
{
    mr_x = x;
    mr_y = y + FIELD_Y;
    mr_w = w;
    mr_h = h;
    mr_base = kind_base[kind];
    mr_row = kind_row[kind];
    map_run();
}

/* a crate: its tile */
void plat_crate(u8 x, u8 y, u8 kind)
{
    u16 e = crate_ent[kind];
    VCTRL = 0;
    VADDR_L = (u8)(2 * x);
    VADDR_M = (u8)(0xAC + y + FIELD_Y);
    VADDR_H = 0x11;
    VDATA0 = (u8)e;
    VDATA0 = (u8)(e >> 8);
}

/* the bitmap's card rows y0 .. y1 (the screen's) cleared */
static void blank_rows(u8 y0, u8 y1)
{
    curPattern = (u16)(glyphs + 8 * G(7));
    WRectangle(0, (u16)y0 << 3, 639, ((u16)(y1 + 1) << 3) - 1);
}

/* the field's picture: tile mode on, its tiles and palette rows (vr_pic_on
   first: after the theme's palette, whose paper the covered cards show) */
static u8 rec_bank(u8 n)
{
    return LoadOverlay(n, 0) == 0 ? r1L : 0;
}

static void pic_load(u8 rows_only)
{
    u8 i;
    for (i = 1; i <= (rows_only ? 1 : PIC_RECORDS); ++i)
        vr_vram_load(rec_bank(i));
}

/* rows y0..y1 of the grid drawn again (under a banner) */
static void redraw_rows(u8 y0, u8 y1)
{
    u8 x, x0, k, y;
    for (y = y0; y <= y1; ++y) {
        x = 0;
        while (x < GW) {
            k = grid[y][x] & KIND;
            x0 = x;
            while (x < GW && (grid[y][x] & KIND) == k)
                ++x;
            plat_rect(x0, y, x - x0, 1, k);
            if (k == C_CRATE)
                for (; x0 < x; ++x0) {
                    u8 i;
                    for (i = 0; i < ncrates; ++i)
                        if (crx[i] == x0 && cry[i] == y)
                            plat_crate(x0, y, crk[i]);
                }
        }
    }
}

static void panel(void);
static void panel_update(void);

void plat_field(void)
{
    blank_rows(2, FIELD_Y + GH - 1);
    plat_rect(0, 0, GW, GH, C_EDGE);
    plat_rect(1, 1, GW - 2, GH - 2, C_SEA);
    /* the band under the menu bar: quay */
    plat_rect(0, 2 - FIELD_Y, 80, 1, C_EDGE);
    panel();
    while (dirty)
        panel_update();
    hw_level();
}

/*---------------------------------------------------------------- text */

static void put(u8 x, u8 row, const char *s)
{
    PutCards((void *)s, (void *)glyphs, x, (u8)strlen(s), (u16)row << 3, 0);
}

/* v in decimal, at least w digits (by subtraction: no 32-bit division) */
static char nbuf[12];
static const u32 tens[8] = { 10000000UL, 1000000UL, 100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL };
static char *dec(u32 v, u8 w)
{
    char *p = nbuf, c;
    u8 i;
    for (i = 0; i < 8; ++i) {
        c = '0';
        while (v >= tens[i]) {
            v -= tens[i];
            ++c;
        }
        if (c != '0' || p != nbuf || 8 - i <= w)
            *p++ = c;
    }
    *p = 0;
    return nbuf;
}

/*---------------------------------------------------------------- the panel */

#define PR1 57          /* the panel's text rows */
#define PR2 58
#define BAR_X 11        /* the calm bar: 50 cards, 2% each */
#define BAR_W 50

static u8 shown_calm = 0xFF, shown_lives = 0xFF;

static void panel(void)
{
    curPattern = (u16)(glyphs + 8 * G(7));
    WRectangle(0, 448, 639, 479);
    ColorRect(0, 80, 56, 4, CHROME);
    put(1, PR1, "LEVEL");
    put(11, PR1, "SCORE");
    put(27, PR1, "TIDE");
    put(58, PR1, "LIVES");
    put(1, PR2, "CALM");
    put(63, PR2, "GOAL");
    shown_calm = shown_lives = 0xFF;
    dirty = D_ALL;
}

/* one part of the panel a call (the main loop calls it each frame while
   something is left), so that a frame draws little */
static void panel_update(void)
{
    u8 d = dirty, i;
    d &= -d;                    /* the lowest bit */
    dirty &= ~d;
    if (d & D_LEVEL) {
        put(7, PR1, dec(level, 2));
        put(68, PR2, dec(target, 2));
        put(70, PR2, "%");
    }
    if (d & D_SCORE)
        put(17, PR1, dec(score, 7));
    if (d & D_TIDE) {
        put(32, PR1, dec(tide, 3));
        if (storm) {
            put(36, PR1, "STORM!");
            ColorRect(36, 6, PR1, 1, RED);
        } else
            put(36, PR1, "      ");
    }
    if ((d & D_LIVES) && lives != shown_lives) {
        static u8 rings[10];
        for (i = 0; i < 9; ++i)
            rings[i] = i < lives ? G_LIFE : ' ';
        PutCards(rings, (void *)glyphs, 64, 9, PR1 << 3, 0);
        if (lives)
            ColorRect(64, lives, PR1, 1, RED);
        if (lives < 9)
            ColorRect(64 + lives, 9 - lives, PR1, 1, CHROME);
        shown_lives = lives;
    }
    if ((d & D_CALM) && calm != shown_calm) {
        u8 f = calm / 2, t = target / 2;
        put(6, PR2, dec(calm, 3));
        put(9, PR2, "%");
        if (f > BAR_W)
            f = BAR_W;
        if (f)
            ColorRect(BAR_X, f, PR2, 1, GREEN);
        if (f < BAR_W)
            ColorRect(BAR_X + f, BAR_W - f, PR2, 1, SKY2);
        if (shown_calm == 0xFF) {
            static u8 bar[BAR_W + 1];
            memset(bar, G(7), BAR_W);
            bar[t] = G_MARK;
            PutCards(bar, (void *)glyphs, BAR_X, BAR_W, PR2 << 3, 0);
        }
        ColorRect(BAR_X + t, 1, PR2, 1, f > t ? HILL : YELLOW);
        shown_calm = calm;
    }
    if (d & D_POWER) {
        static u8 pw[8];
        pw[0] = concrete ? G(8) : ' ';
        pw[1] = concrete ? '0' + concrete : ' ';
        pw[2] = ' ';
        pw[3] = anchor ? G(9) : ' ';
        pw[4] = 0;
        PutCards(pw, (void *)glyphs, 74, 4, PR2 << 3, 0);
        ColorRect(74, 1, PR2, 1, concrete ? ORANGE : CHROME);
        ColorRect(77, 1, PR2, 1, anchor ? SKY2 : CHROME);
    }
}

/*---------------------------------------------------------------- banners */

#define BN_Y 22         /* grid rows 22-28, cards 20-59 */
#define BN_H 7
#define BN_X 20
#define BN_W 40
static u8 banner_on;

static void banner(const char *l1, const char *l2, u8 colour)
{
    u16 x0 = BN_X * 8, y0 = (BN_Y + FIELD_Y) * 8;
    curPattern = (u16)(glyphs + 8 * G(7));
    WRectangle(x0, y0, x0 + BN_W * 8 - 1, y0 + BN_H * 8 - 1);
    WFrameRectangle(0xFF, x0 + 2, y0 + 2, x0 + BN_W * 8 - 3, y0 + BN_H * 8 - 3);
    ColorRect(BN_X, BN_W, BN_Y + FIELD_Y, BN_H, colour);
    put(BN_X + (BN_W - strlen(l1)) / 2, BN_Y + FIELD_Y + 2, l1);
    put(BN_X + (BN_W - strlen(l2)) / 2, BN_Y + FIELD_Y + 4, l2);
    banner_on = 1;
}

static void banner_off(void)
{
    if (!banner_on)
        return;
    blank_rows(BN_Y + FIELD_Y, BN_Y + FIELD_Y + BN_H - 1);
    redraw_rows(BN_Y, BN_Y + BN_H - 1);
    banner_on = 0;
}

/*---------------------------------------------------------------- the pointer */

static u8 vert = 1;             /* the next wall's orientation */
static u8 ptr_now = 0xFF;

static void pointer_pic(void)
{
    const u8 *p = vert ? ptr_v : ptr_h;
    u16 i;
    vat(0x16C00UL);
    for (i = 0; i < 512; ++i)
        VDATA0 = p[i];
}

static void pointer_update(void)
{
    u16 y = mouseYPosW;
    u8 want = (state == S_PLAY && y >= (FIELD_Y + 1) * 8 && y < (FIELD_Y + GH - 1) * 8 && !paused)
              ? PTR_ARROW | PTR_OWN : PTR_ARROW;
    if (want != ptr_now) {
        SetPointer(want);
        ptr_now = want;
    }
}

void toggle_orient(void)
{
    vert ^= 1;
    pointer_pic();
    hw_log2("ORIENT", vert, 0);
}

/*---------------------------------------------------------------- the game */

u8 paused;
static u8 over_shown, over_done;
static u16 over_wait;
static volatile u8 frames;      /* the interrupt's count */
static u8 last;
static u8 rbtn;

/* the sea's wave crests turned round (palette entries 13-15 of its row) */
static u8 wave_t, wave_p;

static void waves(void)
{
    u8 i, k;
    if (++wave_t < 10)
        return;
    wave_t = 0;
    if (++wave_p == 3)
        wave_p = 0;
    vat(0x1FA00UL + 32 * PIC_SEAROW + 26);
    for (i = 0; i < 3; ++i) {
        k = i + wave_p;
        if (k >= 3)
            k -= 3;
        VDATA0 = (u8)crest[k];
        VDATA0 = (u8)(crest[k] >> 8);
    }
}

static void each_frame(void)
{
    ++frames;
    sfx_irq();
    waves();
}

static void start_game(void)
{
    u16 s;
#ifdef HW_SEED
    s = HW_SEED;
#else
    s = random ^ ((u16)frames << 8) ^ frames;
#endif
    banner_on = 0;
    paused = 0;
    over_shown = over_done = 0;
    hw_log2("NEW", s, 0);
    game_new(s);
    last = frames;
}

/* after a box or a long pause: no frames to catch up */
void resync(void)
{
    last = frames;
}

void do_new(void)
{
    sprites_hide();
    start_game();
}

void do_pause(void)
{
    if (state != S_PLAY)
        return;
    paused ^= 1;
    if (paused) {
        sprites_hide();
        banner("PAUSED", "PRESS P OR CHOOSE GAME > PAUSE", TITLE);
    } else {
        banner_off();
        last = frames;
    }
    hw_log2("PAUSE", paused, 0);
}

#ifdef HW_TEST
static u8 q_x, q_y, q_v, q_on;
#endif

/* a press on the field: a wall */
static void press(void)
{
    u16 x, y;
    if (mouseData & 0x80)
        return;                 /* a release */
    x = mouseXPos;
    y = mouseYPosW;
    if (state == S_OVER) {
        if (over_done)
            do_new();
        return;
    }
    if (paused || y < FIELD_Y * 8)
        return;
    x >>= 3;
    y = (y >> 3) - FIELD_Y;
    if (y >= GH)
        return;
#ifdef HW_TEST
    /* the test build holds a click for the next step 8 of 16, so that a
       mouse script's click lands on a step known ahead (the gates' plans) */
    q_x = (u8)x, q_y = (u8)y, q_v = vert, q_on = 1;
#else
    game_place((u8)x, (u8)y, vert);
#endif
}

#ifdef HW_TEST
static void queued(void)
{
    if (q_on && (step_no & 15) == 8) {
        q_on = 0;
        if (game_place(q_x, q_y, q_v))
            hw_in(q_x, q_y, q_v);
    }
}
#else
#define queued()
#endif

static void key(void)
{
    u8 k = keyData;
    switch (k) {
    case ' ':
        toggle_orient();
        break;
    case 'p':
    case 'P':
    case KEY_ESC:
        do_pause();
        break;
    case 'n':
    case 'N':
        if (state == S_OVER)
            do_new();
        break;
    case SHORTCUT | 'n':
        do_new();
        break;
    case SHORTCUT | 'h':
        show_scores();
        break;
    case SHORTCUT | 's':
        toggle_sound();
        break;
    case SHORTCUT | 'q':
        do_quit();
        break;
    }
}

/* the game over: the banner, then the high score */
static void game_over(void)
{
    if (!over_shown) {
        over_shown = 1;
        over_wait = 90;
        sprites_hide();
        banner("THE HARBOUR IS LOST", dec(score, 1), RED);
        hw_over();
        return;
    }
    if (over_done || --over_wait)
        return;
    over_done = 1;
    scores_enter();
    banner("GAME OVER", "CLICK OR PRESS N FOR A NEW GAME", RED);
}

static void tick(void)
{
    u8 n, st;
    if (vr_pic_back()) {        /* back from the switcher: the palette rows again */
        pic_load(1);
        sprites_palette();
    }
    if (menuNumber) {           /* a menu is open over the field */
        sprites_hide();
        last = frames;
        return;
    }
    pointer_update();
    /* the right button turns the wall */
    if (mouseButtons & 2) {
        if (!rbtn)
            toggle_orient();
        rbtn = 1;
    } else
        rbtn = 0;
    n = frames - last;
    if (!n)
        return;
    last += n;
    if (paused)
        return;
    if (state == S_OVER) {
        while (n-- && state == S_OVER)
            game_over();
        return;
    }
    hw_frame_begin(n);
    if (n > 3)
        n = 3;
    st = level;
    while (n--) {
        queued();
        hw_mark(0);
        game_step();
        hw_mark(1);
        hw_step();
        if (state != S_PLAY)
            break;
    }
    if (state == S_PLAY || state == S_CLEAR) {
        if (state == S_CLEAR && !banner_on) {
            banner("THE HARBOUR IS CALM", "WELL BUILT!", GREEN);
            hw_clear();
        }
        if (level != st)
            banner_on = 0;      /* the field was drawn again */
        sprites_show();
    }
    hw_mark(2);
    if (dirty)
        panel_update();
    hw_mark(3);
    hw_frame_end(level != st);
}

/* back to the desktop, everything as it was */
void do_quit(void)
{
    vr_set_irq(0);
    sfx_off();
    sprites_hide();
    vr_pic_off();
    pal_restore();
    hw_quit();
    SetPointer(PTR_ARROW);
    EnterDeskTop();
}

int main(void)
{
    vr_home(r0L);
    hw_start();
    pal_save();
    hw_log2("PAL0", pal_crc(), 0);
    theme_load();
    theme_palette();
    theme_menus();
    dispBufferOn = ST_WR_FORE;
    vr_pic_on();
    pic_load(0);
    sprites_init();
    pointer_pic();
    sound_init();
    menus_init();
    scores_load();
    vr_set_irq(each_frame);
#ifdef HW_BENCH
    { extern void hw_bench(void); hw_bench(); }
#endif
    start_game();
    appMain = VR_FN(tick);
    keyVector = VR_FN(key);
    otherPressVec = VR_FN(press);
    LogText("HARBOUR WALLS RUNNING");
    return 0;
}
