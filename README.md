# Harbour Walls

An original wall-building game for **Veranda**, the GEOS-API desktop for
the Commander X16 (640x480, one paper colour per 8x8 card, VERA sprites,
the PSG). MIT licence, see `LICENSE`.

The harbour is rough: buoys drift across it and bounce off the quays. You
are the harbour master. Build sea walls to fence off stretches of water no
buoy can reach; that water calms into harbour land. Calm enough of the
harbour and the level is done; the next has more buoys, and faster ones.

## The rules

- **Walls grow from a point.** Click open sea: a wall starts on that card
  and grows **both ways at once**, one card at a time, across or down (the
  right button or Space turns the next wall; the pointer's badge shows
  which way). The two arms are shown in two colours (orange going left or
  up, purple going right or down).
- **A growing arm is fragile.** A buoy that touches an arm while it grows,
  or that an arm grows into, **breaks that arm**: it sinks back into the
  sea and you lose a life. The other arm carries on.
- **An arm that reaches stone sets** (the quay, another wall, harbour
  land) and becomes a sandstone sea wall. Every wall that sets is followed
  by a **capture**: a flood fill from every buoy marks the water they can
  still reach; the rest calms into green harbour land.
- **The goal:** each level has a target share of the harbour to calm: 60%
  on level 1, 5% more each level, 80% at most. Reach it and the level is
  cleared (a life back, up to 9).
- **Buoys** come in three classes, each its own colour: green (slow, 1
  pixel a frame), amber (1.5) and red (2). Level n has n + 1 buoys; every
  third level adds a fast one, every other one a medium one.
- **Crates** float up in open water now and then (yellow cards) and sink
  again after 15 seconds. Wall one in, calm the water round it, and you
  take what is in it: a **bolt** (quick-set concrete: the next three walls
  grow three times as fast), an **anchor** (every buoy held still for four
  seconds; held buoys turn grey), a **ring** (a life), a **star** (250
  points x the level). A wall built over a crate sinks it.
- **The tide.** Each level has a tide clock (90 seconds on level 1, 15
  more each level, 240 at most). The seconds left at the clear are a
  bonus. If the tide runs out, a **storm** blows up: every buoy speeds up.
- **Score:** each calmed card is worth the level number (twice that for a
  capture of 400 cards or more); a cleared level adds 5 x the tide left x
  the level, and 100 x the level for every per cent above the goal.
- **Lives:** 3 to start. The game ends with the last one; a score good
  enough goes into the high score table (five names), kept in Veranda's
  system data folder (`/VERANDA/.SYSTEM/Harbour Walls Scores`).

## Controls

| | |
|---|---|
| left click on the sea | start a wall |
| right button, Space | turn the next wall (across / down) |
| P, Esc | pause (the buoys are hidden while paused) |
| Alt+N | new game (N or a click after a game ends) |
| Alt+H | high scores |
| Alt+S | sound on or off |
| Alt+Q | quit to the desktop |

Menus (Veranda Look 2: the kernel's menu bar, `theme_menus()`):
**Harbour Walls** (About, How to play), **Game** (New game, Pause, High
scores, Quit), **Options** (Sound: on/off, Turn the wall). The sound
starts as the system's settings have it (Preferences' sounds off: off).

## The look

The field is in full colour: VERA's 4 bpp tiles on layer 0, under the
bitmap (Veranda's `vr_pic`, its DECISIONS V105). Each kind of card has a
32x32 picture of 4x4 tiles in its own palette row (`tools/mkpic.py`,
drawn there: a moving sea, sandstone walls, grass with flowers and a
path, granite setts for the quay, orange and purple planks for the
growing arms, four crates with their icons), and card (x, y) shows tile
(y & 3) * 4 + (x & 3) of its kind, so the field is textured and a run
of cards is a run of map entries (`x16/pic.s`; no bitmap drawn). The
sea's wave crests are three palette entries the interrupt turns round
every ten frames. The panel and the banners stay cards of Look 2's
paper colours (`sdk/lib/theme.h`) with text in the bitmap, so dark mode
works there as the palette's swap. The buoys are 16x16 VERA sprites at 4 bits a pixel; the
game sets palette entries 32-95 for them (one palette a class, and grey
for anchored buoys; the field's pictures use rows 6-12) and puts the whole palette back as it found it when
it quits. The pointer over the field is our own 4-bit picture: an arrow
with a badge showing the wall's direction. The application's tile is
`x16/tile.txt` (`tile = tile.txt` in `x16/app.hdr`, sky2).

## Building

Needs cc65 (`CC65_HOME`, default `~/.local/share/cc65`), cc, Python 3
(Pillow for the screenshots) and Veranda's SDK, built from a `git archive`
export of Veranda's main (never in Veranda's own tree; the colour field
needs an SDK with `vrpic.h`, Veranda's V105: the gate takes `VREF`):

    mkdir -p build/vsrc && (cd ~/Projects/geos && git archive main) | tar -x -C build/vsrc
    (cd build/vsrc && python3 tools/apigen.py api/veranda.api build/gen && python3 tools/kbuild.py)
    build/vsrc/tools/mksdk.sh $PWD/build/sdk
    make                 # build/app/Harbour Walls.cvt
    make host            # the host tests
    make run             # in the emulator

Copy `Harbour Walls.cvt` to the SD card's `/VERANDA/APPS` and double-click
it. It needs Veranda with API 4 (PlaySound's generation, Look 2's bar).

## The gate

`tools/gate.sh` builds the SDK (as above) and runs everything; see its
header for the details:

- **H** the host tests (`host/tests.c`, compiled with cc): the flood fill
  against a breadth-first search on 3,000 random fields with walls, land,
  crates and growing arms, every budget, and again with a stack of only 8
  seeds (overflows found again by a rescan); a capture and its share, a
  level cleared, an arm broken and a life lost, game over, crates and
  their powers, the storm, the same game from the same inputs.
- **A** the release CVT's header and its start.
- **P** `host/plan`: the same engine plays the seed's game ahead and
  chooses the walls (level 1 cleared in three walls that no buoy breaks,
  even if the click came 3 steps early or late; then walls in level 2 that
  buoys do break, until the game is over); `tools/mkscript.py` turns them
  into an `x16emu -mousescript` script.
- **S** the session under ROM R49 through the real PS/2 mouse path: every
  wall lands on the planned card at the planned step (the test build holds
  a click for the next step that is 8 mod 16, and the script waits for
  the step counter, so a click lands on a known step), captures and the
  calmed share, level 1 cleared, arms broken and lives lost, game over, a
  name typed into the high score table and the file written; Game > Pause,
  P, Game > High scores, the right button and Space on the way; the X16's
  whole game replayed by the host engine number for number (`host/replay`:
  the inputs at their steps; checksums every 64 steps, every capture,
  break and level, the whole state's checksum at the clear and the end;
  the X16 runs the fill in assembly, the host in C); the frame timings.
- **D** the same script again in a fresh folder: the same log, line for
  line.
- **Q** on an SD card image with `/VERANDA`: the session, then Game > Quit
  by the mouse: the palette's CRC at the end equals the one at the start,
  the shell starts, and its palette and sprites equal those of the shell
  started without the game; the high score is in `/VERANDA/.SYSTEM`.
- **V** screenshots (`tools/shot.py` composes VERA's picture: bitmap,
  colour map, sprites) at the moments of the session.

## Speed

60 steps a second, tied to the frame interrupt; a main loop pass runs
one step for each frame that went by (at most three). The per-frame work
is kept small and bounded: buoys check cards only when their leading edge
crosses into a new column or row; a growing arm draws one card; the
capture's flood fill (`x16/fill.s`, a scan-line fill with an explicit
stack) marks at most 400 cards a frame and the calming paints 4 rows a
frame (`paint_scan` in assembly finds the runs); the panel redraws one
field a frame. The test build measures each frame's work with the
emulator's cycle counter, and the gate fails a frame over 16.67 ms or a
frame missed.

## Inside

| Path | What |
|---|---|
| `src/engine.c`, `engine.h` | the rules: the grid, buoys, arms, captures, crates, the tide, levels, scores |
| `src/fill.c` | the flood fill in C (the host; the X16 has `x16/fill.s`) |
| `x16/main.c` | the screen, the sprites, the pointer, the panel, banners, input, the main loop |
| `x16/ui.c` | the menus, the boxes, the high scores, the sound's option |
| `x16/sfx.c` | the sound effects, PSG voices 0-1 from the interrupt (loud: V72) |
| `x16/hw.s`, `x16/fill.s` | the sprites' attributes; the fill and the calming's scan |
| `x16/test.c` | the test build's logs and timings |
| `tools/mkgfx.py` | the card patterns, the 8x8 font, the buoy, the pointers, the icon and tile |
| `host/` | the tests, the planner, the replay |
| `tools/` | the gate, the script maker, the screenshot composer, the runner |
