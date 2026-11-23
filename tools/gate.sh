#!/bin/bash
# Harbour Walls' gate: the host tests, then the game in the emulator under
# ROM R49, played by the real mouse (x16emu -mousescript: PS/2 packets
# through the ROM's mouse driver and Veranda's input path).
#
#   tools/gate.sh                 SEED=n: another game (default 4660)
#
# Needs: Veranda (VERANDA=~/Projects/geos: its main, or VREF, exported by git archive
# into build/vsrc and built there, never in Veranda's own tree (VTREE=<a
# working tree>: copied instead, uncommitted changes too); the SDK
# installed from it into build/sdk), cc65 (CC65_HOME), cc, Python 3 with
# Pillow, ~/Projects/x16-emulator-mouse's x16emu (-mousescript) and R49's
# ROM.
#
# H  the host tests (cc, the same engine C): the flood fill against a
#    breadth-first search on 3,000 random fields (and with a stack of 8
#    seeds, overflows rescanned); captures, the calmed share, a level
#    cleared, an arm broken and a life lost, game over, crates, the tide,
#    the same game from the same inputs
# A  the application: the CVT's header (API 4, 640x480, the tile), it
#    starts through the loader (AUTOSTART)
# P  the plan: host/plan plays the seed's game ahead with the engine and
#    chooses the walls: level 1 cleared in three walls no buoy breaks,
#    level 2 lost to walls buoys do break
# S  the session (the test build, a fixed seed): the planned walls placed
#    by the mouse at the planned steps, captures and the percentage, level
#    1 cleared, arms broken and lives lost, game over; on the way Game >
#    Pause, Game > High scores, P typed, the wall turned by the right
#    button and by Space; a name typed for the high score table, saved;
#    the X16's game replayed by the host engine number for number; the
#    frame timings (the emulator's cycle counter)
# D  determinism: the same script again, a fresh folder: the same game,
#    log line for log line
# Q  quit: on an SD card image with /VERANDA, the session again and then
#    Game > Quit by the mouse: the palette as it was at the start (CRCs),
#    the shell starts, its palette and sprites the same as a shell started
#    without the game; the high score in /VERANDA/.SYSTEM
# V  screenshots at the moments of the session (VERA's picture with the
#    sprites: tools/shot.py), and the release build's first screen
set -e -o pipefail
cd "$(dirname "$0")/.."
TOP=$(pwd)
VERANDA=${VERANDA:-$HOME/Projects/geos}
VREF=${VREF:-main}             # (the colour field needs Veranda's vr_pic: DECISIONS V105)
EMU=${EMU:-$HOME/Projects/x16-emulator-mouse/build/x16emu}
ROM=${ROM:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
SEED=${SEED:-4660}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$HOME/.local/bin:$CC65_HOME/bin:$PATH
PY="python3 -I -B"
OUT=build/gate
SDK=$TOP/build/sdk
export EMU ROM SDK

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*" >&2; exit 1; }

[ -x "$EMU" ] || fail "no x16emu at $EMU"
{ "$EMU" -help 2>&1 || true; } | grep -q -- -mousescript || fail "$EMU has no -mousescript (~/Projects/x16-emulator-mouse)"
[ -f "$ROM" ] || fail "no ROM at $ROM (R49)"
mkdir -p "$OUT"
for d in "$OUT"/*/; do [ -d "$d" ] && rm -r -- "$d"; done; true
rm -f "$OUT"/*.png "$OUT"/*.txt

#---------------------------------------------------------------- the SDK
echo "== Veranda's SDK"
if [ "${HW_KEEP_SDK:-0}" = 0 ] || [ ! -f "$SDK/bin/VERANDA.PRG" ]; then
	[ -d build/vsrc ] && rm -r -- build/vsrc
	mkdir -p build/vsrc
	if [ -n "$VTREE" ]; then    # (VTREE=<a Veranda working tree>: copied as it is, uncommitted changes too)
		tar -C "$VTREE" --exclude=./build --exclude=./.git --exclude=./.claude -cf - . | tar -x -C build/vsrc
	else
		(cd "$VERANDA" && git archive "$VREF") | tar -x -C build/vsrc
	fi
	(cd build/vsrc && $PY tools/apigen.py api/veranda.api build/gen >/dev/null && $PY tools/kbuild.py >/dev/null) \
		|| fail "Veranda does not build (in build/vsrc)"
	build/vsrc/tools/mksdk.sh "$SDK" >/dev/null || fail "the SDK does not install"
fi
[ -f "$SDK/bin/VERANDA.PRG" ] || fail "the SDK has no VERANDA.PRG"
ok "Veranda ${VTREE:+the working tree $VTREE at }$(cd "${VTREE:-$VERANDA}" && git rev-parse --short "${VTREE:+HEAD}${VTREE:-$VREF}"), exported to build/vsrc; the SDK in build/sdk"

#---------------------------------------------------------------- H
echo "== H: the host tests"
H=$OUT/host
mkdir -p "$H"
cc -O2 -Wall -o "$H/tests" host/tests.c host/plat.c src/engine.c src/fill.c || fail "the host tests do not build"
cc -O2 -w -DFSTACK=8 -o "$H/tests8" host/tests.c host/plat.c src/engine.c src/fill.c || fail "the host tests (stack 8)"
cc -O2 -Wall -o "$H/plan" host/plan.c host/plat.c src/engine.c src/fill.c || fail "the planner"
cc -O2 -Wall -o "$H/replay" host/replay.c host/plat.c src/engine.c src/fill.c || fail "the replay"
"$H/tests" | tee "$H/tests.log" | sed 's/^/      /'
grep -q "^FAIL" "$H/tests.log" && fail "H: the host tests"
"$H/tests8" | head -1 | sed 's/^/      /'
"$H/tests8" >/dev/null || fail "H: the flood fill with a stack of 8"
ok "H: $(tail -1 "$H/tests.log" | sed 's/^ok *//') and the fill with a stack of 8 seeds"

#---------------------------------------------------------------- A
echo "== A: the application"
make -s OUT="$TOP/$OUT/app" SDK="$SDK" >"$OUT/app.log" 2>&1 || { cat "$OUT/app.log"; fail "the release build"; }
A="$OUT/app/walls.cvt"
field() { $PY "$SDK/tools/vpack.py" --list "$1" | sed -n "s/^$2 *//p"; }
[ "$(field "$A" 'geos type')" = 6 ] && [ "$(field "$A" class)" = 'HarbourWallsV1.0' ] || fail "A: the CVT's type or class"
[ "$(field "$A" load)" = '$0801' ] && [ "$(field "$A" veranda)" = True ] && [ "$(field "$A" mode)" = 640x480 ] \
	&& [ "$(field "$A" api)" = 4 ] || fail "A: the CVT's load address, VRND, mode or API"
field "$A" tile | grep -q '^sky2' || fail "A: no tile"
end=$(field "$A" end)
ok "A: an application, class HarbourWalls V1.0, \$0801-$end, API 4, 640x480, the tile ($(field "$A" tile))"
printf '0 id 0\n420 exit\n' > "$OUT/start.txt"
tools/run.sh "$A" "$OUT/app/run" 30 "$OUT/start.txt"
grep -aq "HARBOUR WALLS RUNNING" "$OUT/app/run/console.txt" || fail "A: it does not start"
$PY tools/shot.py "$SDK/tools" "$OUT/app/run/dump.bin" "$OUT/start.png"
ok "A: the release build starts through the loader (start.png)"

#---------------------------------------------------------------- P
echo "== P: the plan (seed $SEED)"
"$H/plan" "$SEED" > "$OUT/plan.txt" || { cat "$OUT/plan.txt"; fail "P: the planner did not end the game"; }
sed 's/^/      /' "$OUT/plan.txt"
nc=$(grep -c '^P .* capture' "$OUT/plan.txt"); nb=$(grep -c '^P .* break' "$OUT/plan.txt")
[ "$nc" -ge 3 ] && [ "$nb" -ge 1 ] || fail "P: $nc capturing walls, $nb breaking ones"
ok "P: $nc walls to clear level 1, $nb that buoys break in level 2"

# the test build (HW_TEST: logs, the fixed seed)
T=$OUT/test
make -s test OUT="$TOP/$T" SDK="$SDK" SEED="$SEED" >"$OUT/test.log" 2>&1 || { cat "$OUT/test.log"; fail "the test build"; }
script() { $PY tools/mkscript.py "$OUT/plan.txt" "$T/walls.map" "$1" "${@:2}" || fail "mkscript $*"; }
events() { grep -a -o '\(IN\|HASH\|CALM\|BREAK\|CLEAR\|FULL\|OVER\|LEVEL\|NEW\|PAUSE\|ORIENT\|SCORES\) .*' "$1"; }

#---------------------------------------------------------------- S
echo "== S: the session, the real mouse, R49"
S=$OUT/session
script "$OUT/session.txt"
tools/run.sh "$T/walls.cvt" "$S" 200 "$OUT/session.txt"
C=$S/console.txt
grep -aq "the mouse script's exit" "$C" || { tail -5 "$C"; fail "S: the session did not reach its end"; }
# the walls where and when the plan has them
grep '^P ' "$OUT/plan.txt" | awk '{print "IN", $2, $3, $4, $5}' > "$S/want.txt"
grep -a '^IN ' "$C" > "$S/got.txt" || true
cmp -s "$S/want.txt" "$S/got.txt" || { diff "$S/want.txt" "$S/got.txt"; fail "S: the walls placed are not the plan's"; }
ok "S: $(wc -l < "$S/got.txt") walls placed by the mouse (PS/2 packets through R49's driver), each at its planned card, orientation and step"
cap=$(grep -a '^CALM ' "$C" | head -2 | tail -1)
[ -n "$cap" ] || fail "S: no capture"
grep -aq "^CLEAR LEVEL 1 " "$C" && grep -aq "^LEVEL 2 BUOYS 3 TARGET 65" "$C" || fail "S: level 1 not cleared"
ok "S: captures: $(grep -a '^CALM ' "$C" | awk '{print $3}' | tr '\n' ' ')(the calmed share after each wall); $(grep -a '^CLEAR ' "$C"); level 2: 3 buoys, goal 65%"
nbr=$(grep -ac '^BREAK ' "$C")
[ "$nbr" -ge 1 ] || fail "S: no wall broken"
ok "S: $nbr arms broken by buoys, a life lost each: $(grep -a '^BREAK ' "$C" | awk '{print $4}' | tr '\n' ' ')lives left"
grep -aq "^OVER SCORE " "$C" || fail "S: no game over"
over=$(grep -a '^OVER SCORE ' "$C" | awk '{print $3}')
ok "S: game over, score $over"
grep -aq "^PAUSE 0001" "$C" && grep -aq "^PAUSE 0000" "$C" || fail "S: the pause"
[ "$(grep -ac '^SCORES SHOWN' "$C")" -ge 2 ] || fail "S: the high scores box"
[ "$(grep -ac '^ORIENT ' "$C")" -ge 2 ] || fail "S: the wall was not turned"
ok "S: Game > Pause and P, Game > High scores, the wall turned $(grep -ac '^ORIENT ' "$C") times (the right button, Space)"
grep -aq "SCORES SAVED 0000" "$C" || fail "S: the high score was not saved"
$PY - "$S/Harbour Walls Scores" "$over" <<-'EOF' || fail "S: the high score file"
	import struct, sys
	d = open(sys.argv[1], 'rb').read()
	assert d[:4] == b'HWS1' and len(d) == 4 + 5 * 16, 'the file'
	name = d[4:15].split(b'\0')[0].decode()
	level, score = d[15], struct.unpack_from('<I', d, 16)[0]
	print('      the table: 1. %s %d (level %d)' % (name, score, level))
	assert name == 'HARBOUR' and score == int(sys.argv[2]) and level == 2
	EOF
ok "S: the high score typed in and saved (\"Harbour Walls Scores\", the system data folder: the root of a host folder)"
r=$("$H/replay" "$C") || { echo "$r"; fail "S: the host's replay differs"; }
ok "S: ${r#ok    }"
prof=$(grep -a '^PROF ' "$C" | tail -1)
max=$(echo "$prof" | sed 's/.* MAX \([0-9]*\).*/\1/'); avg=$(echo "$prof" | sed 's/.* AVG \([0-9]*\).*/\1/')
cmx=$(echo "$prof" | sed 's/.* CAPMAX \([0-9]*\).*/\1/'); gmx=$(echo "$prof" | sed 's/.* GROWMAX \([0-9]*\).*/\1/')
nfr=$(echo "$prof" | sed 's/.* FRAMES \([0-9]*\).*/\1/')
over_=$(echo "$prof" | sed 's/.* OVER \([0-9]*\).*/\1/'); missed=$(echo "$prof" | sed 's/.* MISSED \([0-9]*\).*/\1/')
TIMING=$(awk -v n="$nfr" -v m="$max" -v a="$avg" -v c="$cmx" -v g="$gmx" 'BEGIN{printf "%d frames of play: a frame %.2f ms on average, %.2f ms at most (a capture %.2f, a wall growing %.2f) of 16.67", n, a/8000, m/8000, c/8000, g/8000}')
[ "$over_" = 0 ] && [ "$missed" = 0 ] && [ "$max" -lt 133333 ] || fail "S: frame timing: $prof"
ok "S: 60 fps: $TIMING; no frame over, none missed"
echo "$TIMING" > "$OUT/timing.txt"

#---------------------------------------------------------------- D
echo "== D: determinism"
D=$OUT/again
tools/run.sh "$T/walls.cvt" "$D" 200 "$OUT/session.txt"
events "$C" > "$S/events.txt"; events "$D/console.txt" > "$D/events.txt"
cmp -s "$S/events.txt" "$D/events.txt" || { diff "$S/events.txt" "$D/events.txt" | head; fail "D: the second run differs"; }
ok "D: the same script again: the same game, $(wc -l < "$D/events.txt") log lines equal (walls, checksums, captures, breaks, the end)"

#---------------------------------------------------------------- Q
echo "== Q: quit to the shell, the palette back"
Q=$OUT/quit
mkdir -p "$Q/card/VERANDA" "$Q/base/VERANDA"
[ -d "$Q/shell" ] && rm -r -- "$Q/shell"
cp -r build/vsrc/shell "$Q/shell"
make -s -C "$Q/shell" SDK="$SDK" >"$Q/shell.log" 2>&1 || { tail "$Q/shell.log"; fail "Q: the shell does not build"; }
script "$OUT/quit.txt" --quit
cp "$T/walls.cvt" "$Q/card/AUTOSTART.cvt"
for c in card base; do
	cp "$SDK/bin/VERANDA.SYS" "$Q/$c/"
	cp "$Q/shell/SHELL.cvt" "$Q/$c/VERANDA/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$Q/$c.img" "$Q/$c" >/dev/null || fail "Q: vcard make"
done
cp "$SDK/bin/VERANDA.PRG" "$Q/"
(cd "$Q" && SDL_VIDEODRIVER=dummy timeout -s INT 200 "$EMU" -rom "$ROM" -sdcard card.img -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump RV -mousescript ../quit.txt > console.txt 2>&1) || true
grep -aq "the mouse script's exit" "$Q/console.txt" || { tail -5 "$Q/console.txt"; fail "Q: the run did not reach its end"; }
mv "$Q/dump.bin" "$Q/quit.bin"
p0=$(grep -a '^PAL0 ' "$Q/console.txt" | awk '{print $2}'); p1=$(grep -a '^PAL1 ' "$Q/console.txt" | awk '{print $2}')
[ -n "$p0" ] && [ "$p0" = "$p1" ] || fail "Q: the palette's CRC at the start $p0, after the game $p1"
grep -aq '^QUIT' "$Q/console.txt" || fail "Q: Game > Quit did not quit"
grep -aq "SHELL STARTED" "$Q/console.txt" || fail "Q: the shell did not start after the game"
grep -a "PANIC" "$Q/console.txt" && fail "Q: a crash"
printf '0 id 0\n600 exit\n' > "$Q/base.txt"
(cd "$Q" && SDL_VIDEODRIVER=dummy timeout -s INT 60 "$EMU" -rom "$ROM" -sdcard base.img -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump RV -mousescript base.txt > base.console.txt 2>&1) || true
grep -aq "SHELL STARTED" "$Q/base.console.txt" || fail "Q: the shell alone did not start"
mv "$Q/dump.bin" "$Q/base.bin"
$PY - "$Q/quit.bin" "$Q/base.bin" <<-'EOF' || fail "Q: the shell's palette or sprites after the game"
	import sys
	V = 132878
	a, b = [open(p, 'rb').read()[-V:] for p in sys.argv[1:3]]
	pa, pb = a[131072 + 256:][:512], b[131072 + 256:][:512]
	bad = [i for i in range(256) if pa[2 * i:2 * i + 2] != pb[2 * i:2 * i + 2]]
	assert not bad, 'palette entries differ: %s' % bad[:20]
	sa = a[-1024:]
	on = [n for n in range(8, 20) if (sa[8 * n + 6] >> 2) & 3]
	assert not on, 'sprites still on: %s' % on
	print('      the shell after the game: all 256 palette entries as the shell alone has them; sprites 8-19 off')
	EOF
$PY "$SDK/tools/vcard.py" get "$Q/card.img" "VERANDA/.SYSTEM/Harbour Walls Scores" "$Q/scores.bin" >/dev/null \
	|| fail "Q: no high score file in /VERANDA/.SYSTEM"
$PY tools/shot.py "$SDK/tools" "$Q/quit.bin" "$OUT/quit-shell.png"
ok "Q: Game > Quit by the mouse: the palette CRC $p0 at the start and after (entries 32-95 were the buoys'); the shell started, its palette the same as without the game; the high score in /VERANDA/.SYSTEM (quit-shell.png)"

#---------------------------------------------------------------- V
echo "== V: screenshots"
for l in grow capture menu paused scores clear break over newscore end; do
	script "$OUT/stop-$l.txt" --stop "$l"
	tools/run.sh "$T/walls.cvt" "$OUT/shot-$l" 200 "$OUT/stop-$l.txt"
	grep -aq "the mouse script's exit" "$OUT/shot-$l/console.txt" || fail "V: $l: the run did not reach its end"
	$PY tools/shot.py "$SDK/tools" "$OUT/shot-$l/dump.bin" "$OUT/$l.png"
done
ok "V: grow.png capture.png menu.png paused.png scores.png clear.png break.png over.png newscore.png end.png"
echo "pictures: $TOP/$OUT/*.png"
echo "timing:   $TIMING"
echo "GATE harbour-walls: PASS"
