CC=gcc
CFLAGS=-O2 -pg -g -Wall -Wmissing-prototypes
#TOOLPREFIX=/usr/local/brickos/bin/h8300-hitachi-hms-
TOOLPREFIX=/usr/local/rcx/bin/h8300-hitachi-hms-
LIBS=
PROFILE=

ifeq ($(shell test -d /usr/include/alsa && echo 1),1)
LIBS += -L/usr/lib -lasound
SOUND_SOURCES=sound_alsa.c
else
ifneq ($(shell which sdl-config 2>/dev/null),)
# use default SDL config arguments
CFLAGS += $(shell sdl-config --cflags)
LIBS += $(shell sdl-config --libs)
# use custom SDL config arguments (e.g. for building in Cygwin)
#CFLAGS += -I/usr/local/include/SDL
#LIBS += -L/usr/local/lib -lSDL
SOUND_SOURCES=sound_sdl.c
else
SOUND_SOURCES=sound_none.c
endif
endif

SOURCES=main.c h8300.c peripherals.c memory.c lcd.c timer16.c timer8.c \
	buttons.c waitstate.c frame.c serial.c debugger.c adsensors.c \
	watchdog.c firmware.c coff.c srec.c socket.c motor.c symbols.c \
	lx.c hash.c savefile.c printf.c brickos.c bibo.c

HEADERS=types.h h8300.h peripherals.h memory.h lx.h symbols.h hash.h \
	frame.h debugger.h socket.h coff.h

DIST_ASM_SOURCES=h8300-i586.S h8300-x86-64.S h8300-sparc.S
ROM_SOURCES=rom.S rom-lcd.S rom.ld

DIST   =README Makefile \
	$(SOURCES) $(DIST_ASM_SOURCES) $(HEADERS) \
	sound_alsa.c sound_sdl.c sound_none.c \
	$(ROM_SOURCES) \
	h8300.pl h8300-i586.pl h8300-x86-64.pl h8300-sparc.pl \
	ir-server.c GUI.tcl remote \
	firmdl.diff dll-src.diff Makefile.diff \
	brickemu.dxy rom.bin

OBJS = $(subst .c,.o,$(SOURCES)) $(subst .S,.o,$(ASM_SOURCES))  \
	$(subst .c,.o,$(SOUND_SOURCES))

MACHINE:=$(shell uname -m)
ifeq ($(MACHINE),x86_64)
 ASM_SOURCES=h8300-x86-64.S
else
 ifneq ($(filter i%86,$(MACHINE)),)
  ASM_SOURCES=h8300-i586.S
 else
  ifneq ($(filter sun4%,$(MACHINE)),)
   ASM_SOURCES=h8300-sparc.S
   LIBS += -lsocket
  endif
 endif
endif
ifneq ($(ASM_SOURCES),)
CFLAGS += -DHAVE_RUN_CPU_ASM
endif

all: emu ir-server rom.bin

clean:	
	rm -f *.o *.inc emu ir-server
	rm -rf html latex

h83%.inc: h83%.pl
	perl $^ > $@

h8300.o: h8300.inc h8300.h
h8300-i586.o: h8300-i586.inc
h8300-x86-64.o: h8300-x86-64.inc
h8300-sparc.o: h8300-sparc.inc
lcd.o: h8300.h


prototype.h: $(SOURCES)
	perl mkproto.pl $(SOURCES) > prototype.h

ir-server: ir-server.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

emu: CFLAGS += $(PROFILE)
emu: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -lz -o $@

brickemu.tar.gz: $(DIST)
	mkdir brickemu
	cp $^ brickemu
	tar -cvzf brickemu.tar.gz brickemu
	rm -rf brickemu

rom.o: rom.S rom-lcd.S
	$(TOOLPREFIX)gcc -c -o $@ $<

rom.coff: rom.o rom.ld
	$(TOOLPREFIX)ld -T rom.ld -relax rom.o -nostdlib -o $@

rom.bin: rom.coff
	$(TOOLPREFIX)objcopy -O binary $< $@

