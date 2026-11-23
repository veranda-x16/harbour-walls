/* Harbour Walls for Veranda: the platform's parts */
#ifndef HW_H
#define HW_H

/* main.c */
extern u8 paused;
void do_new(void);
void do_pause(void);
void do_quit(void);
void toggle_orient(void);
void sprites_hide(void);
void resync(void);
u16 pal_crc(void);

/* hw.s: the buoys' sprite attributes from bx, by, bkind (anchor: the
   anchored palette) */
void sprites_put(void);

/* ui.c: the menus, the boxes, the high scores, the sound's option */
void menus_init(void);
void show_scores(void);
void scores_load(void);
void scores_enter(void);
void toggle_sound(void);
void sound_init(void);
extern u8 sound_on;

/* sfx.c: the sounds, played from the interrupt on PSG voices 0 and 1 */
void sfx_irq(void);
void sfx_off(void);

/* test.c (HW_TEST builds): logs, checks and timings for the gates */
#ifdef HW_TEST
void hw_start(void);
void hw_log2(const char *what, u16 a, u16 b);
void hw_in(u8 x, u8 y, u8 v);
void hw_step(void);
void hw_frame_begin(u8 n);
void hw_frame_end(u8 skip);
void hw_level(void);
void hw_clear(void);
void hw_over(void);
void hw_quit(void);
void hw_scores(void);
void hw_mark(u8 i);
#else
#define hw_mark(i)
#define hw_start()
#define hw_log2(w, a, b)
#define hw_in(x, y, v)
#define hw_step()
#define hw_frame_begin(n)
#define hw_frame_end(s)
#define hw_level()
#define hw_clear()
#define hw_over()
#define hw_quit()
#define hw_scores()
#endif

#endif
