brickEmu  ![brickEmu CI](https://github.com/BrickBot/brickEmu/workflows/brickEmu%20CI/badge.svg)
========
An emulator for LEGO MindStorms RCX bricks

Copyright © 2003-2006 Jochen Hoenicke

With updates by Matthew Sheets

Original Website – https://jochen-hoenicke.de/rcx/brickemu.html

This is an emulator for a LEGO RCX brick.  It emulates processor and
peripherals, so it runs the original ROM code, firmware and
programs.  In principal it should work with any firmware, but I only
tested brickOS.  It also has some special features that only works when
brickOS is installed.

It is still in an early state, so forgive the bad documentation.


Quick install instructions
--------------------------

You need the ROM image from your RCX brick.  Put this image into a
file named rom.srec in SREC format.  You can get a program that
extracts the image from the [getROM project](https://github.com/BrickBot/getROM).

```shell
cd brickemu
make
export CROSSTOOLPREFIX=<path to h8300 cross-compiler toolchain (e.g. /usr/bin/h8300-hms- )>
export BRICKOS_DIR=<path to brickOS home>
export BRICKOS_LIBDIR=<path to brickOS lib dir>  (if not ${BRICKOS_DIR}/lib/brickos)
./ir-server
wish GUI.tcl -firm <path/to/brickOS.coff>
```

You can set BRICKEMU_DIR if you want to start emu from another
directory.

There are two possibilities to download firmware and programs.  The
easy way is to choose "Firmware..." and "Load Program..." from the
menu.  Make sure that the rcx is turned on before loading firmware,
otherwise it will just fail silently.

With "Firmware..." you can load either coff or srec files.  I
recommend to use coff files as these come with symbolic information,
which is used by brickemu to determine the location of memory, program
and thread data structures.  The "Load Program..." and the various
"Dump" menu entries will only work if you loaded a brickOS firmware
from the coff file.  You can simply "make brickOS.coff" in the
brickos/boot directory.


IR support
----------

The emulated bricks support IR communications and if you start
several instances they can send each other messages.  You can also
inject messages yourself by connecting to port 50637. All data is sent
as raw bytes and repeated to every one who is connected to this port
(including the sender).  This emulates the broadcasting nature of
infra red.

You can also download the program and firmware via IR with the
standard brickOS utitilies .  You need to patch the utilities so they
can also write to a network socket instead of the serial port.  The
patches are included in the archive (see download section).
* NOTE: The BrickBot brickOS-bibo and NQC projects have already been patched.

As an alternative to patching, is it possible that `socat` (Linux) or
`com0com` (Windows) can be setup to provide a pseudo-terminal interface
to ir-server.  (Note that, due to shared dependencies, ir-server has
been moved to the [brickOS project](https://github.com/BrickBot/brickOS-bibo).)

Then you just download the firmware and program as you do it on your
real RCX, only the tty needs to be changed.

```shell
cd brickos
util/firmdl --tty=ncd:localhost:50637 boot/brickOS.srec
util/dll --tty=ncd:localhost:50637 demo/helloworld.lx
```

This will download the firmware via TCP/IP and emulated infrared
controller (this is what ir-server was good for). Since the emulator
runs in real time this will take a while.


To debug:
---------

Download and compile gdb with --target=h8300-hitachi-hms

When the emulator detects illegal memory access it will wait for a
debugger to connect.  You can also force entering debugging mode by
pressing Ctrl + Backslash in the xterm where you started "emu" or
you can start the emulator with "./emu -d".  When it waits for the
debugger you can start it in another window like this:

```shell
cd brickos/boot
make brickOS.coff
h8300-hitachi-hms-gdb brickOS.coff
(gdb) target remote localhost:6789
```


TODO:
-----

- better sensor input/motor output
- fix all bugs :)
- load/save functionality
- support for other Operating Systems (anyone tried this with cygwin?)
- support for other Firmware
- better debugging support especially for the downloaded files.
