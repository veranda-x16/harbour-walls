/* Harbour Walls for Veranda: the menus (Veranda Look 2: the kernel draws
   the bar), the boxes, the high scores (in Veranda's system data folder,
   /VERANDA/.SYSTEM: vrpath.h, DECISIONS V71), the sound's option. */
#include <veranda.h>
#include <vr.h>
#include <vrpath.h>
#include <string.h>
#include "../src/engine.h"
#include "hw.h"

/*---------------------------------------------------------------- boxes */

/* a dialog box at its own place, y in words (WIDE_DB) */
struct boxhead { u8 pos; u16 top, bottom, left, right; };
struct txt { u8 cmd, x, y; const char *s; };
struct btn { u8 cmd, x, y; };

static void box(const void *b)
{
    sprites_hide();
    DoDlgBox((void *)b);
    resync();
}

#define HEAD(t, b, l, r) { SET_DB_POS | WIDE_DB | 1, t, b, l, r }

static const struct {
    struct boxhead h;
    struct txt t[5];
    struct btn ok;
    u8 end;
} about_box = {
    HEAD(160, 290, 160, 479),
    { { DBTXTSTR, 16, 20, "\x18Harbour Walls 1.0\x1b" },
      { DBTXTSTR, 16, 40, "Build sea walls to calm the harbour." },
      { DBTXTSTR, 16, 56, "An original game for Veranda" },
      { DBTXTSTR, 16, 72, "and the Commander X16." },
      { DBTXTSTR, 16, 92, "(c) 2026 Marcin Spoczynski, MIT licence" } },
    { OK, 15, 106 },
    0
};

static const struct {
    struct boxhead h;
    struct txt t[9];
    struct btn ok;
    u8 end;
} help_box = {
    HEAD(120, 330, 72, 567),
    { { DBTXTSTR, 16, 18, "\x18How to play\x1b" },
      { DBTXTSTR, 16, 38, "Click the sea to start a wall: two arms grow apart from there." },
      { DBTXTSTR, 16, 54, "The right button or Space turns the next wall (across or down)." },
      { DBTXTSTR, 16, 70, "A buoy that hits a growing arm sinks it, and you lose a life." },
      { DBTXTSTR, 16, 86, "An arm that reaches stone sets; the sea no buoy can reach calms." },
      { DBTXTSTR, 16, 102, "Calm the goal's share of the harbour to clear the level." },
      { DBTXTSTR, 16, 122, "Crates: bolt quick-set walls, anchor holds the buoys, ring a life," },
      { DBTXTSTR, 16, 138, "star points; wall them in to take them. No tide left: a storm." },
      { DBTXTSTR, 16, 158, "Keys: P pause, Alt+N new, Alt+H scores, Alt+S sound, Alt+Q quit." } },
    { OK, 28, 172 },
    0
};

/*---------------------------------------------------------------- scores */

#define NSCORES 5
struct entry { char name[11]; u8 level; u32 score; };
static struct entry table[NSCORES];
static const char scores_file[] = "Harbour Walls Scores";
static const char magic[4] = { 'H', 'W', 'S', '1' };
static char name[11];
static char lines[NSCORES][48];
/* the table's columns (screen x; the box is 176-463): the names, the
   scores, the level */
#define SC_NAME  217
#define SC_SCORE 312
#define SC_LEVEL 376
static char newline_txt[40];
static u8 fresh = 0xFF;

/* GOTOX x (the kernel's text code $14 and a word: its 0 high byte is
   read as x's, not as the string's end) */
static char *gotox(char *p, u16 x)
{
    *p = 0x14;
    *(u16 *)(p + 1) = x;
    return p + 3;
}

static char *num(char *p, u32 v)
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

void scores_load(void)
{
    u8 h;
    static u8 buf[4 + sizeof table];
    memset(table, 0, sizeof table);
    vr_cd(VR_DATA);
    h = FOpen((void *)scores_file, 'r');
    if (h >= 2) {
        if (FRead(h, buf, sizeof buf) == sizeof buf && !memcmp(buf, magic, 4))
            memcpy(table, buf + 4, sizeof table);
        FClose(h);
    }
    vr_cdback();
}

static u8 scores_save(void)
{
    u8 h, ok = 0;
    static u8 buf[4 + sizeof table];
    memcpy(buf, magic, 4);
    memcpy(buf + 4, table, sizeof table);
    vr_cd(VR_DATA);
    h = FOpen((void *)scores_file, 'w');
    if (h >= 2) {
        ok = FWrite(h, buf, sizeof buf) == 0;
        FClose(h);
    }
    vr_cdback();
    return ok;
}

static const struct {
    struct boxhead h;
    struct txt t[NSCORES + 2];
    struct btn ok;
    u8 end;
} scores_box = {
    HEAD(150, 330, 176, 463),
    { { DBTXTSTR, 16, 20, "\x18High scores\x1b" },
      { DBTXTSTR, 24, 44, lines[0] },
      { DBTXTSTR, 24, 62, lines[1] },
      { DBTXTSTR, 24, 80, lines[2] },
      { DBTXTSTR, 24, 98, lines[3] },
      { DBTXTSTR, 24, 116, lines[4] },
      { DBTXTSTR, 16, 140, newline_txt } },
    { OK, 13, 152 },
    0
};

void show_scores(void)
{
    u8 i;
    char *p;
    for (i = 0; i < NSCORES; ++i) {
        /* "1.", the name, the score, the level, each at its column; the
           new one bold */
        p = lines[i];
        if (i == fresh)
            *p++ = 0x18;                /* (BOLDON) */
        *p++ = (char)('1' + i);
        *p++ = '.';
        p = gotox(p, SC_NAME);
        if (table[i].score) {
            p = gotox(stpcpy(p, table[i].name), SC_SCORE);
            p = gotox(num(p, table[i].score), SC_LEVEL);
            p = num(stpcpy(p, "level "), table[i].level);
        } else
            *p++ = '-';
        *p++ = 0x1B;                    /* (PLAINTEXT) */
        *p = 0;
    }
    if (fresh == 0xFF)
        strcpy(newline_txt, "Walls well built are remembered.");
    else
        strcpy(newline_txt, "Saved in /VERANDA/.SYSTEM.");
    hw_scores();
    box(&scores_box);
}

static char ask_txt[40];
static const struct {
    struct boxhead h;
    struct txt t[2];
    u8 gcmd, gx, gy, greg, gmax;
    struct btn ok;
    u8 end;
} name_box = {
    HEAD(170, 280, 176, 463),
    { { DBTXTSTR, 16, 20, ask_txt },
      { DBTXTSTR, 16, 42, "Your name:" } },
    DBGETSTRING, 90, 34, 0x0E, 10,
    { OK, 13, 76 },
    0
};

/* after a game: a place in the table? */
void scores_enter(void)
{
    u8 i, k;
    char *p;
    fresh = 0xFF;
    if (!score || score <= table[NSCORES - 1].score)
        return;
    p = ask_txt;
    strcpy(p, "\x18" "A new high score: ");   /* (bold: the box's question) */
    p = num(p + 19, score);
    strcpy(p, "!\x1b");
    r6 = (u16)name;
    sprites_hide();
    DoDlgBox((void *)&name_box);
    resync();
    for (k = 0; k < NSCORES && table[k].score >= score; ++k)
        ;
    for (i = NSCORES - 1; i > k; --i)
        table[i] = table[i - 1];
    memset(table[k].name, 0, sizeof table[k].name);
    strncpy(table[k].name, name[0] ? name : "SAILOR", 10);
    table[k].level = level;
    table[k].score = score;
    fresh = k;
    i = scores_save();                  /* (not inside hw_log2: in a release build it is empty) */
    hw_log2(i ? "SCORES SAVED" : "SCORES NOT SAVED", k, (u16)score);
    show_scores();
}

/*---------------------------------------------------------------- sound */

void sound_init(void)
{
    static u8 p[PREFS_SIZE];
    SysPrefs(PREFS_GET, p);
    sound_on = !(p[6] & PREFS_NOSOUND);
}

/*---------------------------------------------------------------- menus */

struct item { const char *text; u8 type; void *ptr; };
struct vmenu { u8 top, bottom; u16 left, right; u8 flags; struct item items[1]; };
#define VMENU(n) struct { u8 top, bottom; u16 left, right; u8 flags; struct item items[n]; }
#define V VERTICAL | UN_CONSTRAINED

static void m_about(void) { GotoFirstMenu(); box(&about_box); }
static void m_help(void) { GotoFirstMenu(); box(&help_box); }
static void m_new(void) { GotoFirstMenu(); do_new(); }
static void m_pause(void) { GotoFirstMenu(); do_pause(); }
static void m_scores(void) { GotoFirstMenu(); fresh = 0xFF; show_scores(); }
static void m_quit(void) { GotoFirstMenu(); do_quit(); }
static void m_turn(void) { GotoFirstMenu(); toggle_orient(); }
static void m_sound(void);

/* Veranda Look 3 (Sunroom): "\x1f" starts a group (the kernel draws a
   dotted line above the item), "Name\tKey" the key the game has, at the
   list's right */
static const char t_son[] = "Sound: on\tAlt S", t_soff[] = "Sound: off\tAlt S";
static VMENU(2) appMenu = { 15, 0, 0, 0, V | 2, {
    { "About Harbour Walls", MENU_ACTION, m_about },
    { "How to play", MENU_ACTION, m_help } } };
static VMENU(4) gameMenu = { 15, 0, 0, 0, V | 4, {
    { "New game\tAlt N", MENU_ACTION, m_new },
    { "Pause\tP", MENU_ACTION, m_pause },
    { "High scores\tAlt H", MENU_ACTION, m_scores },
    { "\x1fQuit\tAlt Q", MENU_ACTION, m_quit } } };
static VMENU(2) optMenu = { 15, 0, 0, 0, V | 2, {
    { t_son, MENU_ACTION, m_sound },
    { "\x1fTurn the wall\tSpace", MENU_ACTION, m_turn } } };
#define NMAIN 3
static VMENU(NMAIN) mainMenu = { 0, 14, 0, 0, HORIZONTAL | UN_CONSTRAINED | NMAIN, {
    { "Harbour Walls", SUB_MENU, &appMenu },
    { "Game", SUB_MENU, &gameMenu },
    { "Options", SUB_MENU, &optMenu } } };

/* an item's width: a leading $1F not counted, the TAB 24 pixels between
   the name and the key */
static u16 textW(const char *s)
{
    u16 w = 0;
    if (*s == 0x1F)
        ++s;
    for (; *s; ++s)
        w += *s == '\t' ? 24 : GetCharWidth(*s);
    return w;
}

/* a vertical menu's size from its items (as the shell: the kernel places
   it under its bar item, its width is the table's) */
static void sizeMenu(struct vmenu *m, u16 left)
{
    u8 i, n = m->flags & 31;
    u16 w, wmax = textW(t_soff);
    for (i = 0; i < n; ++i) {
        w = textW(m->items[i].text);
        if (w > wmax)
            wmax = w;
    }
    m->left = left;
    m->right = left + 8 + wmax + 8;     /* (the text 8 pixels in, 8 after the longest) */
    m->bottom = m->top + n * 16;         /* (the Veranda look: 16 lines an item) */
}

static void m_sound(void)
{
    GotoFirstMenu();
    toggle_sound();
}

void toggle_sound(void)
{
    sound_on ^= 1;
    optMenu.items[0].text = sound_on ? t_son : t_soff;
    if (!sound_on)
        sfx_off();
    else
        plat_sfx(SFX_PLACE);
    hw_log2("SOUND", sound_on, 0);
}

void menus_init(void)
{
    u8 i;
    u16 b = 0;
    UseFont(FONT_UI);                   /* (the menus' font: theme_menus, Veranda's V79) */
    if (!sound_on)
        optMenu.items[0].text = t_soff;
    for (i = 0; i < NMAIN; ++i) {
        sizeMenu((struct vmenu *)mainMenu.items[i].ptr, b);
        b += 8 + textW(mainMenu.items[i].text);
    }
    mainMenu.right = b;
    UseSystemFont();
    DoMenu((void *)&mainMenu, 0);
}
