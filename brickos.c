/* Emulator for LEGO RCX Brick, Copyright (C) 2003 Jochen Hoenicke.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; see the file COPYING.LESSER.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: brickos.c 261 2006-02-24 16:28:30Z hoenicke $
 */

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h> /* for ntohs/ntohl */

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"
#include "lx.h"
#include "symbols.h"
#include "coff.h"

#define MAX_PATHNAME_LEN 4096

#define TDATA_SP_SAVE  0
#define TDATA_TSTATE   2
#define TDATA_TFLAGS   3
#define TDATA_PCHAIN   4
#define TDATA_NEXT     6
#define TDATA_PREV     8
#define TDATA_PARENT   10
#define TDATA_STACK_BASE 12
#define TDATA_WAKEUP   12
#define TDATA_WAKEUP_DATA 14

#define PCHAIN_PRIORITY 0
#define PCHAIN_NEXT     2
#define PCHAIN_PREV     4
#define PCHAIN_CTID     6

#define READ_BYTE(offset) (memory[offset])
#define WRITE_BYTE(offset, val) memory[offset] = (val)
#define READ_WORD(offset) ((memory[offset] << 8) | memory[(offset) + 1])
#define WRITE_WORD(offset, val) do { memory[offset] = (val) >> 8; \
                             memory[(offset) + 1] = (val) & 0xff; } while(0)

static void brickos_free(uint16 mm_start, uint16 mm_first_free,
			 uint16 addr) {
    uint16 ptr = mm_start;
    uint16 nptr;
    if (!mm_start)
	return;

    while (ptr >= mm_start) {
	uint16 len  = READ_WORD(ptr+2);
	nptr = ptr + 4 + 2*len;
	if (ptr+4 == addr) {
	    WRITE_WORD(ptr, 0);
	    if (READ_WORD(mm_first_free) < ptr)
		WRITE_WORD(mm_first_free, ptr);

	    while (READ_WORD(nptr) == 0) {
		/* merge adjacent free blocks */
		if (nptr == READ_WORD(mm_first_free))
		    WRITE_WORD(mm_first_free, ptr);

		len += 2 + READ_WORD(nptr+2);
		WRITE_WORD(ptr + 2, len);
		nptr = ptr + 4 + 2 * len;
	    }
	    return;
	}
	ptr = nptr;
    }
    return;
}

static uint16 brickos_malloc(uint16 mm_start, uint16 mm_first_free,
			     uint16 size) {
    uint16 ptr = mm_start;
    uint16 nptr;
    if (!mm_start)
	return 0;

    size = (size + 1) >> 1;
    while (ptr >= mm_start) {
	uint16 type = READ_WORD(ptr);
	uint16 len  = READ_WORD(ptr+2);
	nptr = ptr + 4 + 2*len;
	while (type == 0 && READ_WORD(nptr) == 0) {
	    /* merge adjacent free blocks */
	    if (nptr == READ_WORD(mm_first_free))
		WRITE_WORD(mm_first_free, ptr);

	    len += 2 + READ_WORD(nptr+2);
	    WRITE_WORD(ptr + 2, len);
	    nptr = ptr + 4 + 2 * len;
	}

	if (type == 0 && len > size) {
	    if (len > size + 8) {
		/* split block */
		WRITE_WORD(ptr + 2, size);
		nptr = ptr + 4 + 2 * size;
		WRITE_WORD(nptr, 0);
		WRITE_WORD(nptr + 2, len - size - 2);
	    }
	    WRITE_WORD(ptr, 0xfffe);
	    if (ptr == READ_WORD(mm_first_free)) {
		while (READ_WORD(nptr) != 0 && nptr >= mm_start) {
		    nptr = nptr + 4 + 2 * READ_WORD(nptr + 2);
		}
		WRITE_WORD(mm_first_free, nptr);
	    }
	    return ptr + 4;
	}
	ptr = nptr;
    }
    return 0;
}

static void brickos_dump_memory() {
    uint16 mm_start = symbols_getaddr("_mm_start");
    uint16 ptr;

    if (!mm_start)
	return;

    ptr = mm_start;
    printf("****** BrickOS Memory Dump ******\n");

    while (ptr >= mm_start) {
	uint16 type = READ_WORD(ptr);
	uint16 len  = READ_WORD(ptr+2);
	char buffer[20], *owner;
	if (type == 0xffff) {
	    owner = "RESERVED";
	} else if (type == 0) {
	    owner = "FREE";
	} else if (type == 1) {
	    owner = "OWNED";
	} else if (type == 0xfffe) {
	    owner = "BEAMED_PROGRAM";
	} else {
	    snprintf(buffer, 20, "THR %04x", type);
	    owner = buffer;
	}
	printf("Block %04x len %5d, owner %s\n", ptr + 4, 2 * len, owner);
	ptr += 4 + 2*len;
    }
    printf("*********************************\n");
}

static void brickos_dump_threads() {
    static const char tstates[] = { 'K', 'Z', 'W', 'S', 'R', '?' };
    uint16 prio, stid, tid;
    uint16 priority_head = symbols_getaddr("_priority_head");
    uint16 ctid = symbols_getaddr("_ctid");
    if (!priority_head)
	return;

    printf("****** BrickOS Thread Dump ******\n");
    prio = READ_WORD(priority_head);
    while (prio) {
	printf("Priority Queue %04x:  Prio %3d\n", prio,
	       READ_BYTE(prio + PCHAIN_PRIORITY));
	stid = READ_WORD(prio + PCHAIN_CTID);
	tid = READ_WORD(stid + TDATA_NEXT);
	while (1) {
	    uint8 state = READ_BYTE(tid + TDATA_TSTATE);
	    uint8 flags = READ_BYTE(tid + TDATA_TFLAGS);
	    uint16 stackbase = READ_WORD(tid + TDATA_STACK_BASE);
	    if (state >= sizeof(tstates))
		state = sizeof(tstates)-1;
	    printf("  Thread %04x: parent %04x state %c, flags %c%c%c%c"
		   " stack(@%04x %04x free)%s\n",
		   tid, READ_WORD(tid + TDATA_PARENT), tstates[state],
		   (flags & 0x80) ? 'S':'-',
		   (flags & 0x04) ? 'I':'-',
		   (flags & 0x02) ? 'U':'-',
		   (flags & 0x01) ? 'K':'-',
		   stackbase, READ_WORD(tid + TDATA_SP_SAVE) - stackbase, 
		   tid == ctid ? " CURRENT" : "");
	    if (tid == stid)
		break;
	    tid = READ_WORD(tid + TDATA_NEXT);
	}
	prio = READ_WORD(prio + PCHAIN_NEXT);
    }

    printf("*********************************\n");
}

static void brickos_dump_programs() {
    int i;
    uint16 programs = symbols_getaddr("_programs");

    if (!programs)
	return;

    printf("****** BrickOS Program Dump ******\n");
    for (i = 0; i < 8; i++) {
	uint16 prog = programs + i * 22;
	if (READ_WORD(prog + 8) > 0) {
	    uint16 text = READ_WORD(prog +  0);
	    uint16 data = READ_WORD(prog +  2);
	    uint16 bss  = READ_WORD(prog +  4);
	    uint16 text_size  = READ_WORD(prog +  8);
	    uint16 data_size  = READ_WORD(prog + 10);
	    uint16 bss_size   = READ_WORD(prog + 12);
	    uint16 stack_size = READ_WORD(prog + 14);
	    uint16 start      = READ_WORD(prog + 16);
	    uint16 prio       = READ_BYTE(prog + 18);

	    printf("Program %d: size %04x (%04x text+data) stack_size %04x downloaded %04x\n"
		   "   text %04x-%04x data %04x-%04x bss %04x-%04x start %04x, pio %d\n",
		   i, text_size + 2*data_size + bss_size, 
		   text_size + data_size, stack_size, READ_WORD(prog + 20),
		   text, text + text_size,
		   data, data + data_size,
		   bss, bss + bss_size,
		   start, prio);
	}
    }
    printf("**********************************\n");
}

static void brickos_load_program(int fd) {
    FILE *file;
    char filename[MAX_PATHNAME_LEN+1];
    uint16 prog;
    uint16 text;
    char buf[1];
    int isCoff = 0;
    int len;
    lx_header header;
    uint16 programs = symbols_getaddr("_programs");
    uint16 mm_start = symbols_getaddr("_mm_start");
    uint16 mm_first_free = symbols_getaddr("_mm_first_free");

    read(fd, buf, 1);
    prog = programs + 22 * (buf[0] - '0');

    len = 0;
    do {
	len += read(fd, filename + len, 1);
    } while (filename[len-1] != '\n' && filename[len-1] != '\r');
    filename[len-1] = 0;

    if (mm_start == 0 || programs == 0) {
	fprintf(stderr, "Not enough symbolic information about firmware\n");
	return;
    }

    /* Open file */
    if ((file = fopen(filename, "r")) == NULL) {
	fprintf(stderr, "%s: failed to open\n", filename);
	return;
    }

    if (coff_init(file)) {
	coff_aouthdr aouthdr;

	fseek(file, sizeof(coff_header), SEEK_SET);
	fread(&aouthdr, sizeof(aouthdr), 1, file);
	header.text_size = ntohl(aouthdr.tsize);
	header.data_size = ntohl(aouthdr.dsize);
	header.bss_size = ntohl(aouthdr.bsize);
	header.base = ntohl(aouthdr.text_start);
	header.offset = ntohl(aouthdr.entry) - header.base;
#define DEFAULT_STACK_SIZE    1024
	header.stack_size = DEFAULT_STACK_SIZE;
	isCoff = 1;
    } else if (!lx_init(file, &header)) {
	fprintf (stderr, "Can't detect file format\n");
	return;
    }

    if (READ_WORD(prog + 8) > 0)
	brickos_free(mm_start, mm_first_free, READ_WORD(prog + 0));

    text = brickos_malloc(mm_start, mm_first_free,
			  header.text_size + 
			  2 * header.data_size
			  + header.bss_size);
    if (text == 0) {
	fprintf (stderr, "Not enough space\n");
	return;
    }

    if (isCoff && text != header.base) {
	fprintf (stderr, "Coff linked to wrong address\n");
	brickos_free(mm_start, mm_first_free, text);
	return;
    }
    
    WRITE_WORD(prog + 0, text);
    WRITE_WORD(prog + 2, text + header.text_size);
    WRITE_WORD(prog + 4, text + header.text_size + header.data_size);
    WRITE_WORD(prog + 6, 
	     text + header.text_size + header.data_size + header.bss_size);
    WRITE_WORD(prog + 8, header.text_size);
    WRITE_WORD(prog + 10, header.data_size);
    WRITE_WORD(prog + 12, header.bss_size);
    WRITE_WORD(prog + 14, header.stack_size);
    WRITE_WORD(prog + 16, header.offset);
    WRITE_BYTE(prog + 18, 10 /*DEFAULT_PRIORITY*/);
    if (isCoff) {
	coff_read(file, text);
	coff_symbols(file, text);
    } else
	lx_read(file, &header, text);
    memset(memory + text + header.text_size + header.data_size,
	   0, header.bss_size);
    memcpy(memory + text + 
	   header.text_size  + header.data_size + header.bss_size,
	   memory + text + header.text_size, 
	   header.data_size);
    WRITE_WORD(prog + 20, header.text_size + header.data_size);
}

static void brickos_sethostaddr(int fd) {
    uint16 lnp_hostaddr;
    char buf[5];
    int addr;
    read(fd, buf, 1);

    lnp_hostaddr = symbols_getaddr("_lnp_hostaddr");

    if (buf[0] == '?') {
	if (lnp_hostaddr)
	    sprintf(buf, "ON%x\n", memory[lnp_hostaddr] >> 4);
	else
	    strcpy(buf, "ON?\n");
	write(fd, buf, strlen(buf));
    } else if (!lnp_hostaddr) {
	fprintf(stderr, "Not enough symbolic information about firmware\n");
    } else {
	addr = buf[0] > 'a' ? (buf[0] - 'a' + 10) :
	    buf[0] > 'A' ? (buf[0] - 'A' + 10) : (buf[0] - '0');

	memory[lnp_hostaddr] = addr << 4;
    }
}

static void brickos_checkos(int fd) {
    char buf[5];

    sprintf(buf, "OO%d\n", symbols_getaddr("_mm_start") ? 1 : 0);
    write(fd, buf, strlen(buf));
}

static void brickos_newprogram_addr(int fd) {
    char buf[20];
    uint16 programs = symbols_getaddr("_programs");
    uint16 mm_start = symbols_getaddr("_mm_start");
    uint16 mm_first_free = symbols_getaddr("_mm_first_free");
    uint16 addr = 0, size;
    uint16 prog;
    int len;

    len = 0;
    do {
	len += read(fd, buf + len, 1);
    } while (len < sizeof(buf) && buf[len-1] != '\n' && buf[len-1] != '\r');
    buf[len-1] = 0;
    prog = programs + 22 * (buf[0] - '0');
    size = atoi(buf+1);
    if (programs && mm_start) {
	if (READ_WORD(prog + 8) > 0) {
	    /* We don't really want to delete the program before
	     * we load the new one, but we need to return the correct
	     * address as if it had been deleted.
	     */
	    brickos_free(mm_start, mm_first_free, READ_WORD(prog + 0));
	    WRITE_WORD(prog + 8, 0);
	}
	addr = brickos_malloc(mm_start, mm_first_free, size);
	if (addr) 
	    brickos_free(mm_start, mm_first_free, addr);
    }
    sprintf(buf, "OA%x\n", addr);
    write(fd, buf, strlen(buf));
}

void brickos_read_fd(int fd) {
    char buf[3];

    /* read in next byte: id */
    read(fd, buf, 1);

    switch (buf[0]) {
    case 'M':
    case 'm':
	brickos_dump_memory();
	break;
    case 'P':
    case 'p':
	brickos_dump_programs();
	break;
    case 'O':
    case 'o':
	brickos_checkos(fd);
	break;
    case 'A':
    case 'a':
	brickos_newprogram_addr(fd);
	break;
	    
    case 'N':
    case 'n':
	brickos_sethostaddr(fd);
	break;
	
    case 'T':
    case 't':
	brickos_dump_threads();
	break;
	
    case 'L':
    case 'l':
	brickos_load_program(fd);
	break;
    }
}
