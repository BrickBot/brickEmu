# Development Packages:
# * Required: GNU GCC, h8300 toolchain
# * Optional: ALSA, SDL
# Runtime Packages Required: TK, ALSA or SDL
# * openSUSE: tk
CC=gcc
CFLAGS=-std=gnu89 -O2 -pg -g -Wall -Wmissing-prototypes
TOOLPREFIX=h8300-hitachi-coff-
LIBS=
PROFILE=

# If defined, EMUSUBDIR _MUST_ include the trailing directory slash
EMUSUBDIR=

ifeq ($(SOUND),mute)
  EMU_SOUND_SOURCE_FILES=sound_none.c
else ifeq ($(shell test -d /usr/include/alsa && echo 1),1)
  LIBS += -L/usr/lib -lasound
  EMU_SOUND_SOURCE_FILES=sound_alsa.c
else ifneq ($(shell which sdl-config 2>/dev/null),)
  # use default SDL config arguments
  CFLAGS += $(shell sdl-config --cflags)
  LIBS += $(shell sdl-config --libs)
  # use custom SDL config arguments (e.g. for building in Cygwin)
  #CFLAGS += -I/usr/local/include/SDL
  #LIBS += -L/usr/local/lib -lSDL
  EMU_SOUND_SOURCE_FILES=sound_sdl.c
else
  EMU_SOUND_SOURCE_FILES=sound_none.c
endif
EMU_SOUND_SOURCE_PATHS=$(EMU_SOUND_SOURCE_FILES:%=$(EMUSUBDIR)%)

EMU_HEADER_FILES=types.h h8300.h peripherals.h memory.h lx.h symbols.h hash.h \
	frame.h debugger.h socket.h coff.h
EMU_HEADER_PATHS=$(EMU_HEADER_FILES:%=$(EMUSUBDIR)%)

EMU_SOURCE_FILES=main.c h8300.c peripherals.c memory.c lcd.c timer16.c timer8.c \
	buttons.c waitstate.c frame.c serial.c debugger.c adsensors.c \
	watchdog.c firmware.c coff.c srec.c socket.c motor.c symbols.c \
	lx.c hash.c savefile.c printf.c brickos.c bibo.c
EMU_SOURCE_PATHS=$(EMU_SOURCE_FILES:%=$(EMUSUBDIR)%)

DIST_ASM_SOURCES=h8300-i586.S h8300-x86-64.S h8300-sparc.S
ROM_SOURCES=rom.s rom-lcd.s rom.ld

DIST = README Makefile \
	$(SOURCES) $(DIST_ASM_SOURCES) $(HEADERS) \
	sound_alsa.c sound_sdl.c sound_none.c \
	$(ROM_SOURCES) \
	h8300.pl h8300-i586.pl h8300-x86-64.pl h8300-sparc.pl \
	ir-server.c GUI.tcl remote \
	firmdl.diff dll-src.diff Makefile.diff \
	brickemu.dxy rom.bin

## NOTE: Assembly sources do not work with current compiler tools,
##       so disabling EMU_ASM_SOURCE_FILES for now by commenting out the line below.
#MACHINE:=$(shell uname -m)
ifeq ($(MACHINE),x86_64)
  EMU_ASM_SOURCE_FILES=h8300-x86-64.S
else
  ifneq ($(filter i%86,$(MACHINE)),)
    EMU_ASM_SOURCE_FILES=h8300-i586.S
  else
    ifneq ($(filter sun4%,$(MACHINE)),)
     EMU_ASM_SOURCE_FILES=h8300-sparc.S
     LIBS += -lsocket
    endif
  endif
endif
ifneq ($(EMU_ASM_SOURCE_FILES),)
  CFLAGS += -DHAVE_RUN_CPU_ASM
endif

EMU_ASM_SOURCE_PATHS=$(EMU_ASM_SOURCE_FILES:%=$(EMUSUBDIR)%)


OBJS = $(subst .c,.o,$(EMU_SOURCE_PATHS)) $(subst .S,.o,$(EMU_ASM_SOURCE_PATHS))  \
	$(subst .c,.o,$(EMU_SOUND_SOURCE_PATHS))


all: emu ir-server rom

clean: emu-clean

realclean: clean emu-realclean
	rm -f ir-server


emu: CFLAGS += $(PROFILE)
emu: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -lz -o $@

emu-clean:	
	rm -f *.o *.inc
	rm -rf html latex

emu-realclean: emu-clean
	rm -f emu

emu-install:

emu-uninstall:


h83%.inc: h83%.pl
	perl $^ > $@

h8300.o: h8300.inc h8300.h
h8300-i586.o: h8300-i586.inc
h8300-x86-64.o: h8300-x86-64.inc
h8300-sparc.o: h8300-sparc.inc
lcd.o: h8300.h


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
	$(TOOLPREFIX)as -o $@ $<

rom.coff: rom.o rom.ld
	$(TOOLPREFIX)ld -T rom.ld -relax rom.o -nostdlib -o $@

rom.bin: rom.coff
	$(TOOLPREFIX)objcopy -I coff-h8300 -O binary $< $@
