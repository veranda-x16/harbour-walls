# Harbour Walls for Veranda
#   make            build/app/Harbour Walls.cvt, against a Veranda SDK (SDK=...)
#   make test       the test build (HW_TEST: logs for the gates, a fixed seed SEED)
#   make host       the host tests (cc)
#   make run        in the emulator (the SDK's vrun)
# OUT: where the results go (default build/app)
SDK      ?= build/sdk
OUT      ?= build/app
OPT      ?= -Oirs -Cl
export CC65_HOME ?= $(HOME)/.local/share/cc65
CC       := cl65 -t none --cpu 65C02 $(OPT) -I $(SDK)/include --asm-include-dir $(SDK)/include -I $(OUT)
SRC      := x16/main.c x16/ui.c x16/sfx.c x16/test.c x16/hw.s x16/fill.s x16/pic.s src/engine.c src/fill.c
DEFS     := -DFILL_ASM
APP      := $(OUT)/Harbour Walls.cvt
HOSTCC   ?= cc

all: $(OUT)/walls.cvt

$(OUT)/gfx.h: tools/mkgfx.py
	mkdir -p $(OUT)
	python3 -I -B tools/mkgfx.py $@

# the field's pictures in full colour (VERA tiles: Veranda's vr_pic, DECISIONS V105)
$(OUT)/pic.h: tools/mkpic.py
	mkdir -p $(OUT)
	python3 -I -B tools/mkpic.py $(OUT) $(SDK) > $(OUT)/mkpic.log

$(OUT)/walls.bin: $(SRC) src/engine.h x16/hw.h $(OUT)/gfx.h $(OUT)/pic.h
	mkdir -p $(OUT)/obj
	for f in $(OUT)/obj/*.o; do [ -f "$$f" ] && rm -f "$$f"; done; true
	for f in $(SRC); do $(CC) $(DEFS) $(CFLAGS) -c -o $(OUT)/obj/$$(basename $$f).o $$f || exit 1; done
	cl65 -t none -C $(SDK)/cfg/app-c.cfg -m $(OUT)/walls.map -o $@ $(OUT)/obj/*.o $(SDK)/lib/veranda.lib

$(OUT)/walls.cvt: $(OUT)/walls.bin x16/app.hdr x16/icon.pbm x16/tile.txt $(OUT)/pic.h
	python3 -I -B $(SDK)/tools/vpack.py --load 0x0801 -o "$(APP)" x16/app.hdr $(OUT)/walls.bin \
		$$(ls $(OUT)/pic.vram.[0-9]* | sort -t. -k3 -n)
	cp "$(APP)" $@

test:
	$(MAKE) OUT=$(OUT) CFLAGS="-DHW_TEST -DHW_SEED=$(or $(SEED),4660) $(TESTFLAGS)" all

host: build/host/tests
	build/host/tests
	build/host/tests8

build/host/tests: host/tests.c host/plat.c host/host.h src/engine.c src/fill.c src/engine.h
	mkdir -p build/host
	$(HOSTCC) -O2 -Wall -o $@ host/tests.c host/plat.c src/engine.c src/fill.c
	$(HOSTCC) -O2 -Wall -DFSTACK=8 -o build/host/tests8 host/tests.c host/plat.c src/engine.c src/fill.c

run: all
	$(SDK)/tools/vrun "$(APP)"

clean:
	rm -f $(OUT)/walls.bin $(OUT)/walls.map $(OUT)/walls.cvt "$(APP)" $(OUT)/gfx.h $(OUT)/pic.h $(OUT)/pic.vram.* $(OUT)/obj/*.o

.PHONY: all test host run clean
