#!/usr/bin/env python3
"""A mouse script (x16emu -mousescript) from the planner's walls.

usage: mkscript.py <plan.txt> <walls.map> <out.txt> [--stop LABEL] [--quit]

The script waits for the game to be ready for a wall (hw_ready), puts the
pointer on the wall's card, turns the wall when it must (the right button,
or Space typed as a key), waits for the game's step / 16 (hw_sync) and
clicks: the click lands at the planned step (plan.txt: the step 16 v +
the delay). On the way: a pause (P typed, P again), the high scores box
through the menu (Game > High scores, Return), and after the game is
over: a name for the table (typed, Return), the table (Return), then
Game > Quit by the mouse, if --quit.

--stop LABEL ends the script there instead (for screenshots): grow (a
wall half grown), capture (the first capture done), menu (the Game menu
open), paused, scores (the table open from the menu), clear (the level
cleared), break (a wall broken), over (the game over), newscore (the
table after the game), end (the GAME OVER banner, before quitting).
"""
import re
import sys

MENU_GAME = (128, 6)            # the bar's "Game" (Veranda Look 2: the kernel's geometry)
ITEM_Y = {'new': 22, 'pause': 38, 'scores': 54, 'quit': 70}
ITEM_X = 125


def main():
    plan, mapf, out = sys.argv[1:4]
    stop = sys.argv[sys.argv.index('--stop') + 1] if '--stop' in sys.argv else None
    quit_ = '--quit' in sys.argv
    m = open(mapf).read()

    def addr(n):
        r = re.search(r'\b_%s\s+00([0-9A-F]{4})' % n, m)
        if not r:
            raise SystemExit('mkscript: no %s in the map' % n)
        return r.group(1)
    SYNC, READY, STATE = addr('hw_sync'), addr('hw_ready'), addr('hw_state')
    delay = None
    walls = []
    for line in open(plan):
        w = line.split()
        if w and w[0] == 'P':
            walls.append((int(w[1]), int(w[2]), int(w[3]), int(w[4]), w[5]))
        if w and w[0] == 'delay':
            delay = int(w[1])
    if delay is None:
        delay = 3
    o = ['# Harbour Walls: written by tools/mkscript.py from the planner\'s walls',
         '0 id 0']
    st = {'f': 0, 'x': 0, 'y': 0, 'v': 1, 'toggles': 0}

    def at(n, cmd):
        st['f'] += n
        o.append('%d %s' % (st['f'], cmd))

    def wait(a, v, mask=None):
        o.append('wait %s %X%s' % (a, v, '' if mask is None else ' %X' % mask))
        st['f'] = 0

    def home():
        at(4, 'move -2000 -2000')
        st['x'] = st['y'] = 0

    def goto(x, y):
        dx, dy = x - st['x'], y - st['y']
        if dx or dy:
            at(4, 'move %d %d' % (dx, dy))
        st['x'], st['y'] = x, y

    def click(n=4):
        at(n, 'down')
        at(4, 'up')

    def menu(item):
        home()
        goto(*MENU_GAME)
        click(6)
        at(20, 'move 0 0')
        # (Veranda's V137: an open menu leaves the pointer where it is, on
        # the bar's title; it holds the pointer: no homing)
        goto(ITEM_X, ITEM_Y[item])
        click(6)

    def done(label):
        if stop == label:
            at(30, 'exit')
            open(out, 'w').write('\n'.join(o) + '\n')
            sys.exit(0)

    wait(READY, 1)
    home()
    level2 = False
    for i, (t, x, y, v, kind) in enumerate(walls):
        wait(READY, 1)
        if kind == 'break' and not level2:
            level2 = True
        if i == 1:
            # paused by Game > Pause: the game stands still while the high
            # scores box comes from the menu; P typed goes on
            menu('pause')
            at(30, 'move 0 0')
            done('paused')
            menu('scores')
            at(60, 'move 0 0')
            done('scores')
            at(10, 'type \\n')
            at(60, 'type p')
            at(20, 'move 0 0')
        home()
        goto(x * 8 + 4, (y + 3) * 8 + 4)
        if v != st['v']:
            if st['toggles'] % 2 == 0:
                at(4, 'down 2')
                at(4, 'up 2')
            else:
                at(4, 'type z z')     # a space between two keys that do nothing
            st['toggles'] += 1
            st['v'] = v
        at(30, 'move 0 0')              # (the packets delivered before the wait)
        vs = ((t - 8) >> 4) & 0xFF     # the click is held for step 16 v + 8
        wait(SYNC, vs)
        click(1)
        if i == 0:
            at(30, 'move 0 0')
            done('grow')
            wait(READY, 1)
            at(4, 'move 0 0')
            done('capture')
            if stop == 'menu':
                home()
                goto(*MENU_GAME)
                click(6)
                done('menu')
        if kind == 'break':
            at(40, 'move 0 0')
            done('break')
        if i == len(walls) - 1:
            break
        if walls[i + 1][4] == 'break' and kind == 'capture':
            wait(STATE, 1)
            at(40, 'move 0 0')
            done('clear')
    wait(STATE, 2)
    at(30, 'move 0 0')
    done('over')
    at(200, 'type HARBOUR\\n')
    at(90, 'move 0 0')
    done('newscore')
    at(30, 'type \\n')
    at(60, 'move 0 0')
    done('end')
    if quit_:
        menu('quit')
        at(400, 'exit')
    else:
        at(60, 'exit')
    open(out, 'w').write('\n'.join(o) + '\n')


if __name__ == '__main__':
    main()
