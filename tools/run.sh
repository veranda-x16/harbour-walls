#!/bin/bash
# run.sh CVT DIR SECONDS [MOUSESCRIPT]: the CVT as AUTOSTART.cvt in DIR (a
# host folder), headless and warped under ROM R49, at most SECONDS; the
# console in DIR/console.txt, VRAM (and RAM) in DIR/dump.bin at the end
# (EMU, ROM, SDK from the environment; EXTRA: more files to put there)
set -e
cvt=$1; d=$2; secs=$3; ms=$4
mkdir -p "$d"
rm -f "$d"/dump*.bin "$d"/console.txt
cp "$cvt" "$d/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$d/"
for f in $EXTRA; do cp "$f" "$d/"; done
args=()
if [ -n "$ms" ]; then cp "$ms" "$d/mouse.txt"; args=(-mousescript mouse.txt); fi
(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT "$secs" "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump RV "${args[@]}" > console.txt 2>&1) || true
