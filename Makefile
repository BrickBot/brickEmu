## ========================================================================================================
## IMPORTANT NOTE: This Makefile setup is designed to work both standalone and integrated with brickOS-bibo
##   - Makefile:     For building stand-alone, effectively providing what would provided by Makefle
##                     or Makefile.common from brickOS-bibo
##   - Makefile.sub: Facilitates building as part of brickOS-bibo
## ========================================================================================================

# Development Packages:
# * Required: GNU GCC, h8300 toolchain
# * Optional: ALSA, SDL
# Runtime Packages Required: TK,TCL Libs, and ALSA or SDL
# * openSUSE: tk

# Using $(abspath x) here so that symlinks are NOT replaced
MAKEFILE_PATH  := $(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR   := $(abspath $(dir $(MAKEFILE_PATH)))
MAKEFILE_NAME  := $(notdir $(MAKEFILE_PATH))

CC=gcc
CFLAGS=-std=gnu89 -O2 -pg -g -Wall -Wmissing-prototypes
CROSSTOOLPREFIX=h8300-hitachi-coff-
CROSSTOOLSUFFIX=
LIBS=
PROFILE=

# If defined, EMUSUBDIR _MUST_ include the trailing directory slash
EMUSUBDIR=

ROM_SOURCES=rom.s rom-lcd.s rom.ld

DIST_ASM_SOURCES=h8300-i586.S h8300-x86-64.S h8300-sparc.S

DIST = README Makefile \
	$(SOURCES) $(DIST_ASM_SOURCES) $(HEADERS) \
	sound_alsa.c sound_sdl.c sound_none.c \
	$(ROM_SOURCES) \
	h8300.pl h8300-i586.pl h8300-x86-64.pl h8300-sparc.pl \
	ir-server.c GUI.tcl remote \
	firmdl.diff dll-src.diff Makefile.diff \
	brickemu.dxy rom.bin


OBJS = $(subst .c,.o,$(EMU_SOURCE_PATHS)) $(subst .S,.o,$(EMU_ASM_SOURCE_PATHS))  \
	$(subst .c,.o,$(EMU_SOUND_SOURCE_PATHS))


all: emu ir-server rom


include $(MAKEFILE_DIR)/Makefile.sub


clean: emu-clean

realclean: clean emu-realclean
	rm -f ir-server


brickemu.tar.gz: $(DIST)
	mkdir brickemu
	cp $^ brickemu
	tar -cvzf brickemu.tar.gz brickemu
	rm -rf brickemu


prototype.h: $(SOURCES)
	perl mkproto.pl $(SOURCES) > prototype.h

ir-server: ir-server.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@


rom: rom.bin

rom.o: rom.s rom-lcd.s
	$(CROSSTOOLPREFIX)as$(CROSSTOOLSUFFIX) -o $@ $<

rom.coff: rom.o rom.ld
	$(CROSSTOOLPREFIX)ld$(CROSSTOOLSUFFIX) -T rom.ld -relax rom.o -nostdlib -o $@

rom.bin: rom.coff
	$(CROSSTOOLPREFIX)objcopy$(CROSSTOOLSUFFIX) -I coff-h8300 -O binary $< $@


.PHONY: all clean realclean emu-clean emu-realclean emu-install emu-uninstall rom
