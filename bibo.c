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
 * $Id: bibo.c 336 2006-04-01 19:27:09Z hoenicke $
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
#include "frame.h"

#define MAX_PATHNAME_LEN 4096

#define TDATA_SP_SAVE  0
#define TDATA_TFLAGS   2
#define TDATA_PRIO     3
#define TDATA_NEXT     4
#define TDATA_PREV     6
#define TDATA_END      8

#define READ_BYTE(offset) (memory[offset])
#define WRITE_BYTE(offset, val) memory[offset] = (val)
#define READ_WORD(offset) ((memory[offset] << 8) | memory[(offset) + 1])
#define WRITE_WORD(offset, val) do { memory[offset] = (val) >> 8; \
                             memory[(offset) + 1] = (val) & 0xff; } while(0)
#define DEBUG(...)

static void bibo_free(uint16 mm_start, uint16 mm_first_free, uint16 addr) {
    uint16 next, prev;
    DEBUG("bibo_free: %04x\n", addr);
    if (addr == 0)
        return;

    addr -= 6;
    next = READ_WORD(addr + 2);
    if ((next & 1)) {
        /* merge with previous free block */
        next &= ~1;
        prev = READ_WORD(addr);
        DEBUG("merge_with_previous: %04x\n", prev);
        if (READ_WORD(next + 4) == 0) {
            /* merge with next free block and dequeue that */
            DEBUG("merge_with_next: %04x\n", next);
            uint16 nextnext = READ_WORD(next + 2);
            uint16 nextnextfree = READ_WORD(next + 6);
            WRITE_WORD(prev + 2, nextnext);
            WRITE_WORD(nextnext + 0, prev);
            WRITE_WORD(prev + 6, nextnextfree);
        } else {
            uint16 nextnext = READ_WORD(next + 2);
            WRITE_WORD(next + 2, nextnext | 1);
            WRITE_WORD(prev + 2, next);
            WRITE_WORD(next + 0, prev);
        }
    } else {
        uint16 prevnext;
        /* enqueue block to free list */
        WRITE_WORD(addr + 4, 0);
        prev = mm_first_free;
        prevnext = READ_WORD(prev);
        while (prevnext && prevnext < addr) {
            prev = prevnext + 6;
            prevnext = READ_WORD(prev);
        }
        WRITE_WORD(prev, addr);
        if (prevnext == next) {
            /* merge with next free block */
            DEBUG("merge_with_next: %04x", next);
            uint16 nextnext = READ_WORD(next + 2);
            uint16 nextnextfree = READ_WORD(next + 6);
            WRITE_WORD(addr + 2, nextnext);
            WRITE_WORD(addr + 6, nextnextfree);
        } else {
            /* New free block */
            uint16 nextnext = READ_WORD(next + 2) | 1;
            WRITE_WORD(next + 2, nextnext);
            WRITE_WORD(next + 6, prevnext);
        }
    }
}

static uint16 bibo_malloc(uint16 mm_start, uint16 mm_first_free,
                           uint16 size) {
    uint16 pptr, ptr;
    pptr = mm_first_free;
    size = (size + 1) & ~1;
    
    while ((ptr = READ_WORD(pptr))) {
        uint16 blocksize = READ_WORD(ptr+2) - ptr;
        if (blocksize >= size + 4) {
            WRITE_WORD(ptr + 4, 0xfffe); /* set owner */
            if (blocksize >= size + 12) {
                /* split block */
                WRITE_WORD(ptr + size + 4 + 4, 0); /* free next block */
                WRITE_WORD(ptr + size + 4 + 2, ptr + blocksize);
                WRITE_WORD(ptr + blocksize, ptr + size + 4);
                WRITE_WORD(ptr + size + 4 + 6, READ_WORD(ptr + 6));
                WRITE_WORD(ptr + 2, ptr + size + 4);
                WRITE_WORD(pptr, ptr + size + 4);
            } else {
                WRITE_WORD(pptr, READ_WORD(ptr + 6));
            }
            return ptr + 6;
        }
        pptr = ptr + 6;
    }
    return 0;
}

void bibo_dump_timers(FILE *out) {
    char* funcname, buf[10];
    fprintf(out, "********* Timers: *******\n");
    uint16 timer = READ_WORD(symbols_getaddr("_timer_queue"));
    uint16 delay = READ_WORD(symbols_getaddr("_first_delay"));
    while (timer) {
        int pc = READ_WORD(timer+2);
        funcname = symbols_get(pc, 0);
        if (!funcname) {
            snprintf(buf, 10, "0x%04x", pc);
            funcname=buf;
        }
        fprintf(out, "   %5d : %s(%04x)\n",
                delay, funcname, READ_WORD(timer+4));
        timer = READ_WORD(timer+6);
        delay = READ_WORD(timer);
    }
    fprintf(out, "*************************\n");
}

void bibo_dump_memory(FILE *out) {
    uint16 mm_start = symbols_getaddr("_mm_start");
    uint16 nextfree = symbols_getaddr("_mm_first_free");
    uint16 ptr;

    if (!mm_start)
        return;

    ptr = mm_start - 2;
    fprintf(out, "****** Rcxrt Memory Dump ******\n");

    while (ptr >= mm_start - 2) {
        uint16 next = READ_WORD(ptr+2) & ~1;
        uint16 type = READ_WORD(ptr+4);
        char buffer[20], *owner;
        if (type == 0 && READ_WORD(nextfree) != ptr)
            fprintf(out, "Free list broken %04x != %04x\n", 
                   READ_WORD(nextfree), ptr);
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
        if (type == 0)
            nextfree = ptr + 6;
        fprintf(out, "Block %04x len %5d, owner %s\n", 
               ptr + 6, (uint16) (next - ptr - 4), owner);
        if (type != 0 && next && (READ_WORD(next+2) & 1))
            fprintf(out, "Memory broken: next block thinks this is free\n");
        if (type == 0 && next && 
            (!(READ_WORD(next+2) & 1) || READ_WORD(next) != ptr))
            fprintf(out, "Memory free list broken: %04x != %04x:%d\n", 
                   ptr, READ_WORD(next), READ_WORD(next+2) & 1);
        ptr = next;
    }
    if (READ_WORD(nextfree) != 0)
        fprintf(out, "Free list broken %04x not found\n", READ_WORD(nextfree));
    fprintf(out, "*********************************\n");
}

void bibo_dump_threads(FILE *out) {
    static const char tstates[] = { 'K', 'Z', 'W', 'R', '?' };
    uint16 tid;
    uint16 td_idle = symbols_getaddr("_td_idle");
    uint16 waiters = symbols_getaddr("_waiters");
    uint16 ctid = READ_WORD(symbols_getaddr("_ctid"));
    uint16 last;
    
    if (!td_idle || !waiters)
        return;

    fprintf(out, "****** Rcxrt Thread Dump ******\n");
    fprintf(out, "Running (current %04x):\n", ctid);
    tid = td_idle;
    do {
        last = tid;
        tid = READ_WORD(tid + TDATA_NEXT);

        uint8 flags = READ_BYTE(tid + TDATA_TFLAGS);
        uint8 state = flags & 7 ;
        uint16 stackbase = tid + TDATA_END;
        uint16 sp = tid == ctid ? GET_REG16(7) : 
            READ_WORD(tid + TDATA_SP_SAVE);
        if (tid == td_idle)
            stackbase = 0xfe00;
        if (state >= sizeof(tstates))
            state = sizeof(tstates)-1;
        if (READ_WORD(tid + TDATA_PREV) != last) {
            fprintf(out, "    QUEUE_ERROR tid.prev = %04x != %04x\n",
                   READ_WORD(tid + TDATA_PREV), last);
        }
        fprintf(out, "  Thread %04x: prio %2d state %c, flags %c%c%c"
               " stack(@%04x %04x free)%s\n",
               tid, READ_BYTE(tid + TDATA_PRIO), tstates[state],
               (flags & 0x80) ? 'S':'-',
               (flags & 0x40) ? 'I':'-',
               (flags & 0x20) ? 'K':'-',
               sp, sp - stackbase,
               tid == ctid ? " CURRENT" : tid == td_idle ? " IDLE": "");
        frame_dump_stack(out, sp);
        if (tid == 0) {
            fprintf(out, "    QUEUE_ERROR tid.next == 0\n");
            break;
        }
    } while (tid != td_idle);
    fprintf(out, "Waiting:\n");
    last = waiters;
    tid = READ_WORD(waiters + TDATA_NEXT);
    while (tid != waiters) {
        uint8 flags = READ_BYTE(tid + TDATA_TFLAGS);
        uint8 state = flags & 7 ;
        uint16 stackbase = tid + TDATA_END;
        uint16 sp = READ_WORD(tid + TDATA_SP_SAVE);
        if (state >= sizeof(tstates))
            state = sizeof(tstates)-1;
        if (READ_WORD(tid + TDATA_PREV) != last) {
            fprintf(out, "    QUEUE_ERROR tid.prev = %04x != %04x\n",
                   READ_WORD(tid + TDATA_PREV), last);
        }
        fprintf(out, "  Thread %04x: prio %2d state %c, flags %c%c%c"
               " stack(@%04x %04x free)%s\n",
               tid, READ_BYTE(tid + TDATA_PRIO), tstates[state],
               (flags & 0x80) ? 'S':'-',
               (flags & 0x40) ? 'I':'-',
               (flags & 0x20) ? 'K':'-',
               sp, sp - stackbase, "");
        frame_dump_stack(out, sp);
        if (tid == 0) {
            fprintf(out, "    QUEUE_ERROR tid.next == 0\n");
            break;
        }
        last = tid;
        tid = READ_WORD(tid + TDATA_NEXT);
    }
    if (READ_WORD(waiters + TDATA_PREV) != last) {
        fprintf(out, "    QUEUE_ERROR tid.prev = %04x != %04x\n",
               READ_WORD(waiters + TDATA_PREV), last);
    }
    
    fprintf(out, "*********************************\n");
}

static void bibo_dump_programs(FILE *out) {
    int i;
    uint16 programs = symbols_getaddr("_programs");

    if (!programs)
        return;

    fprintf(out, "****** Rcxrt Program Dump ******\n");
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

            fprintf(out, "Program %d: size %04x (%04x text+data) stack_size %04x downloaded %04x\n"
                   "   text %04x-%04x data %04x-%04x bss %04x-%04x start %04x, pio %d\n",
                   i, text_size + 2*data_size + bss_size, 
                   text_size + data_size, stack_size, READ_WORD(prog + 20),
                   text, text + text_size,
                   data, data + data_size,
                   bss, bss + bss_size,
                   start, prio);
        }
    }
    fprintf(out, "**********************************\n");
}

static void bibo_load_program(int fd) {
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
        fprintf(stderr, "BrickEmu: Program load error: Not enough symbolic information about firmware\n");
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
        bibo_free(mm_start, mm_first_free, READ_WORD(prog + 0));

    text = bibo_malloc(mm_start, mm_first_free,
                          header.text_size + 
                          2 * header.data_size
                          + header.bss_size);
    if (text == 0) {
        fprintf (stderr, "Not enough space\n");
        return;
    }

    if (isCoff && text != header.base) {
        fprintf (stderr, "Coff linked to wrong address\n");
        bibo_free(mm_start, mm_first_free, text);
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
    } else {
        lx_read(file, &header, text);
    }
    memset(memory + text + header.text_size + header.data_size,
           0, header.bss_size);
    memcpy(memory + text + 
           header.text_size  + header.data_size + header.bss_size,
           memory + text + header.text_size, 
           header.data_size);
    WRITE_WORD(prog + 20, header.text_size + header.data_size);
}

static void bibo_sethostaddr(int fd) {
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

static void bibo_checkos(int fd) {
    char buf[5];

    sprintf(buf, "OO%d\n", symbols_getaddr("_mm_start") ? 1 : 0);
    write(fd, buf, strlen(buf));
}

static void bibo_newprogram_addr(int fd) {
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
            bibo_free(mm_start, mm_first_free, READ_WORD(prog + 0));
            WRITE_WORD(prog + 8, 0);
        }
        addr = bibo_malloc(mm_start, mm_first_free, size);
        if (addr) 
            bibo_free(mm_start, mm_first_free, addr);
    }
    sprintf(buf, "OA%x\n", addr);
    write(fd, buf, strlen(buf));
}

extern void brickos_read_fd(int fd);

static void bibo_read_fd(int fd) {
    char buf[3];

    /* check if bibo is loaded, otherwise chain brickos */
    if (symbols_getaddr("_td_idle") == 0)
      brickos_read_fd(fd);

    /* read in next byte: id */
    read(fd, buf, 1);

    switch (buf[0]) {
    case 'M':
    case 'm':
        bibo_dump_memory(stdout);
        fflush(stdout);
        break;
    case 'P':
    case 'p':
        bibo_dump_programs(stdout);
        fflush(stdout);
        break;
    case 'O':
    case 'o':
        bibo_checkos(fd);
        break;
    case 'A':
    case 'a':
        bibo_newprogram_addr(fd);
        break;
            
    case 'N':
    case 'n':
        bibo_sethostaddr(fd);
        break;
        
    case 'T':
    case 't':
        bibo_dump_threads(stdout);
        fflush(stdout);
        break;
        
    case 'C':
    case 'c':
        bibo_dump_timers(stdout);
        fflush(stdout);
        break;
        
    case 'L':
    case 'l':
        bibo_load_program(fd);
        break;
    }
}

static int bibo_check_irq() {
    return 255;
}


static peripheral_ops bibo = {
    id: 'O',
    read_fd: bibo_read_fd,
    check_irq: bibo_check_irq
};

void bibo_init(void) {
    register_peripheral(bibo);
}
