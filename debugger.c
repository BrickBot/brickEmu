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
 * $Id: debugger.c 112 2005-08-01 15:37:55Z hoenicke $
 */

#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"
#include "frame.h"
#include "socket.h"
//#define VERBOSE_DEBUG

#define BRICK_DEBUG_PORT 6789
#define SIGINT_EXCEPTION 7

/*
 * Comment copied from within db_handle_packet() method:
 *
 * GDB expects 13 x 32 bit registers. The real register value is in
 * the high 16 bits, the rest is 0 padded. The top 3 registers are
 * not used by h8/300, and ignored (0 filled).
 */
#define NUM_REGS 13
#define NUM_REG_BYTES (NUM_REGS*sizeof(int32))

int   debuggerfd;
int   monitorport;

static uint8 db_registers[NUM_REG_BYTES];
static int   serverfd, gdbfd;

static char db_in_buffer[2048];
static char db_out_buffer[2048];
static int db_len = 0, db_out_len = 0, db_cont;

static int remote_debug;

static int bptype2mask[6] = {
    MEMTYPE_BREAKPOINT, 
    MEMTYPE_BREAKPOINT, 
    MEMTYPE_WRITETRAP, 
    MEMTYPE_READTRAP,
    MEMTYPE_BREAKPOINT | MEMTYPE_WRITETRAP | MEMTYPE_READTRAP,
    MEMTYPE_BREAKPOINT, 
};
    
static const char hexchars[]="0123456789abcdef";
static int hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
    if ((ch >= '0') && (ch <= '9')) return (ch-'0');
    if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
    return (-1);
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
static char* mem2hex(uint8 *mem, char *buf, int count) {
  int i;
  unsigned char ch;
  
  for (i=0;i<count;i++) {
      ch = *mem++;
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch % 16];
  }
  return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
/*
 * This code also checks the data being written, and if it is gdb's
 * idea of a breakpoint (an h8/500 instruction, I think), it
 * translates it into 'bra #040ef' instruction.
 */
static char* hex2mem(char *buf, uint8 *mem, int count) {
  int i;
  unsigned char ch;

  if (count == 2) {
      char tbuff[5]; /* Drats . No memcmp, strncmp */
      memcpy(tbuff, buf, 4);
      tbuff[4] = 0;
      if (strcmp(tbuff, "5730") == 0) {
        memcpy(buf, "5f0c", 4);
      }
  }
  for (i=0;i<count;i++) {
    ch = hex(*buf++) << 4;
    ch = ch + hex(*buf++);
    *mem++ = ch;
  }
  return mem;
}


static void db_send_packet() {
    struct sigaction sigact;
    struct sigaction oldsigact;

    if (gdbfd == -1) {
        db_out_len = 0;
        return;
    }

    sigact.sa_handler = SIG_IGN;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sigact, &oldsigact);
    
#ifdef VERBOSE_DEBUG
    db_out_buffer[db_out_len] = 0;
    printf("Send db packet: '%s'\n", db_out_buffer);
#endif

    if (write(gdbfd, db_out_buffer, db_out_len) < 0) {
        printf ("Can't write packet :(\n");
        close(gdbfd);
        gdbfd = -1;
        debuggerfd = serverfd;
    }
    db_out_len = 0;

    sigaction(SIGPIPE, &oldsigact, NULL);
}

static void db_checksum_packet(int start) {
    unsigned char checksum = 0;
    int i;
    
    for (i = start; i < db_out_len; i++)
        checksum += db_out_buffer[i];
    
    db_out_buffer[db_out_len++] = '#';
    db_out_buffer[db_out_len++] = hexchars[checksum >> 4];
    db_out_buffer[db_out_len++] = hexchars[checksum & 0xf];
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int hexToInt(char **ptr, int *intValue) {
  int numChars = 0;
  int hexValue;
  
  *intValue = 0;
  while (**ptr) {
    hexValue = hex(**ptr);
    if (hexValue >=0) {
      *intValue = (*intValue <<4) | hexValue;
      numChars ++;
    }
    else
      break;
    
    (*ptr)++;
  }
  return (numChars);
}

static int sigval;

static void db_handle_packet(char* packet) {
    int start;
    int type, addr, length, i;

    db_out_buffer[db_out_len++] = '$';
    start = db_out_len;
    switch (*packet++) {
    case '?' :   
        db_out_buffer[db_out_len++] = 'S';
        db_out_buffer[db_out_len++] =  hexchars[sigval >> 4];
        db_out_buffer[db_out_len++] =  hexchars[sigval & 0xf];
        db_out_buffer[db_out_len++] =  0;
        break;
    case 'd' : 
        remote_debug = !(remote_debug);  /* toggle debug flag */
        break;
    case 'g' : /* return the value of the CPU registers */
        /*
         * GDB expects 13 x 32 bit registers. The real register value is in
         * the high 16 bits, the rest is 0 padded. The top 3 registers are
         * not used by h8/300, and ignored (0 filled).
         */
        
        memset(db_registers, 0, sizeof(db_registers));
        for (i = 0; i < 8; i++) {
            db_registers[4*i]   = reg[i];
            db_registers[4*i+1] = reg[i+8];
        }
        db_registers[33]  = ccr;
        db_registers[36]  = pc >> 8;
        db_registers[37]  = pc & 0xff;
        db_registers[40]  = (cycles >> 8) & 0xff;
        db_registers[41]  = cycles & 0xff;

        mem2hex(db_registers, 
                db_out_buffer + db_out_len, NUM_REG_BYTES);
        db_out_len += 2 * NUM_REG_BYTES;
        break;
    case 'G' : /* set the value of the CPU registers - return OK */
        hex2mem(packet, (char*) db_registers, NUM_REG_BYTES);
        db_out_buffer[db_out_len++] = 'O';
        db_out_buffer[db_out_len++] = 'K';
        break;
    case 'P' : /* set the value of a single CPU register - return OK */
        if (hexToInt (&packet, &addr) && *packet++ == '='
            && addr >= 0 && addr < NUM_REGS) {
            uint8 value[4];
            hex2mem (packet, value, 4);
            if (addr >= 0 && addr < 8) {
                reg[addr  ] = value[0];
                reg[addr+8] = value[1];
            } else if (addr == 8) {
                ccr = value[1];
            } else if (addr == 9) {
                pc = (value[0]<<8)+value[1];
            }
            db_out_buffer[db_out_len++] = 'O';
            db_out_buffer[db_out_len++] = 'K';
        } else {
            db_out_buffer[db_out_len++] = 'E';
            db_out_buffer[db_out_len++] = '0';
            db_out_buffer[db_out_len++] = '1';
            break;
        }
        
        /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
    case 'm' :

        if (hexToInt(&packet,&addr)
            && *(packet++) == ','
            && hexToInt(&packet,&length)) {

            if (addr >= 0 && addr + length <= 0xff88) {
                mem2hex(&memory[addr], 
                        db_out_buffer + db_out_len, length);
                db_out_len += 2 * length;
            } else {
                db_out_buffer[db_out_len++] = 'E';
                db_out_buffer[db_out_len++] = '0';
                db_out_buffer[db_out_len++] = '3';
            }
        } else {
            db_out_buffer[db_out_len++] = 'E';
            db_out_buffer[db_out_len++] = '0';
            db_out_buffer[db_out_len++] = '1';
        }
        break;
        
        /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
    case 'M' :
        if (hexToInt(&packet,&addr) 
            && *(packet++) == ','
            && hexToInt(&packet,&length)
            && *(packet++) == ':') {
            
            if (addr >= 0 && addr + length <= 0xff88) {
                /* we allow writing to ROM, but why not :) */
                hex2mem(packet, &memory[addr], length);
                db_out_buffer[db_out_len++] = 'O';
                db_out_buffer[db_out_len++] = 'K';
            } else {
                db_out_buffer[db_out_len++] = 'E';
                db_out_buffer[db_out_len++] = '0';
                db_out_buffer[db_out_len++] = '3';
            }
            
        } else {
            db_out_buffer[db_out_len++] = 'E';
            db_out_buffer[db_out_len++] = '0';
            db_out_buffer[db_out_len++] = '2';
        }
        break;
        
        /* cAA..AA    Continue at address AA..AA(optional) */
        /* sAA..AA   Step one instruction from AA..AA(optional) */
    case 's' :
    case 'c' :
        /* try to read optional parameter, pc unchanged if no parm */
        db_singlestep = *(packet - 1) == 's';
        db_trap = 0;
        if (hexToInt(&packet,&addr)) {
            pc = addr;
        }
        db_out_len = start - 1;
        db_send_packet();
        return;

        /* detach */
    case 'D':
        db_cont = 1;
        break;

    case 'Z' :

        if (hexToInt(&packet,&type)
            && *(packet++) == ','
            && hexToInt(&packet,&addr)
            && *(packet++) == ','
            && hexToInt(&packet,&length)) {

            if (addr >= 0 && addr + length <= 0xff88
                && type >= 0 && type < 6) {
                int mask = bptype2mask[type];
                for (i = addr; i < addr + length; i++)
                    memtype[i] |= mask;
                db_out_buffer[db_out_len++] = 'O';
                db_out_buffer[db_out_len++] = 'K';
            } else {
                db_out_buffer[db_out_len++] = 'E';
                db_out_buffer[db_out_len++] = '0';
                db_out_buffer[db_out_len++] = '3';
            }
        } else {
            db_out_buffer[db_out_len++] = 'E';
            db_out_buffer[db_out_len++] = '0';
            db_out_buffer[db_out_len++] = '1';
        }
        break;

    case 'z' :

        if (hexToInt(&packet,&type)
            && *(packet++) == ','
            && hexToInt(&packet,&addr)
            && *(packet++) == ','
            && hexToInt(&packet,&length)) {

            if (addr >= 0 && addr + length <= 0xff88
                && type >= 0 && type < 6) {
                int mask = bptype2mask[type];
                for (i = addr; i < addr + length; i++)
                    memtype[i] &= ~mask;
                db_out_buffer[db_out_len++] = 'O';
                db_out_buffer[db_out_len++] = 'K';
            } else {
                db_out_buffer[db_out_len++] = 'E';
                db_out_buffer[db_out_len++] = '0';
                db_out_buffer[db_out_len++] = '3';
            }
        } else {
            db_out_buffer[db_out_len++] = 'E';
            db_out_buffer[db_out_len++] = '0';
            db_out_buffer[db_out_len++] = '1';
        }
        break;
        
        /* kill the program; ignore it */
    case 'k' :
        break;

    } /* switch */
    
    /* reply to the request */
    db_checksum_packet(start);
    db_send_packet();
}

static void db_parse_packet() {
    uint8 checksum;
    int i = 0;

    db_in_buffer[db_len] = 0;
    while (i < db_len) {
        while  (i < db_len && db_in_buffer[i] != '$') {
            /* skip preceding garbage */
            if (db_in_buffer[i] == '\003') {
                /* Ctrl-C pressed in debugger */
                db_trap = SIGINT_EXCEPTION;
            }
#ifdef VERBOSE_DEBUG
            else
                printf("preceding garbage: '%02x'\n", db_in_buffer[i]);
#endif
            i++;
        }
        if (i >= db_len) {
            db_len = 0;
            return;
        }
        
        memcpy(db_in_buffer, db_in_buffer+i, db_len - i);
        db_len -= i;
        
        i = 1;
        checksum = 0;
        while (i < db_len - 2 && db_in_buffer[i] != '#') {
            checksum += db_in_buffer[i++];
        }
        if (i == db_len - 2) {
            return;
        }
        
        db_in_buffer[i+3] = 0;
        
        if (checksum != (hex(db_in_buffer[i+1]) << 4 | hex(db_in_buffer[i+2]))) {
            printf("Illegal db packet: '%s'\n", db_in_buffer);
            db_out_buffer[db_out_len++] = '-';
            db_send_packet();
        } else {
#ifdef VERBOSE_DEBUG
            printf("Got db packet: '%s'\n", db_in_buffer);
#endif
            db_out_buffer[db_out_len++] = '+';
            db_in_buffer[i] = 0;

            /* if a sequence char is present, reply the sequence ID */
            if (db_in_buffer[3] == ':') {
                db_out_buffer[db_out_len++] = db_in_buffer[1];
                db_out_buffer[db_out_len++] = db_in_buffer[2];
                db_in_buffer[0] = '#';
                db_handle_packet(db_in_buffer+4);
            } else {
                db_handle_packet(db_in_buffer+1);
            }
            
        }
        i+=3;
        memcpy(db_in_buffer, db_in_buffer+i, db_len - i);
        db_len -= i;
    }
}

void db_handletrap() {
    int start;
    sigval = db_trap;

    if (gdbfd >= 0) {
        db_out_buffer[db_out_len++] = '$';
        start = db_out_len;
        db_out_buffer[db_out_len++] = 'S';
        db_out_buffer[db_out_len++] = hexchars[sigval >> 4];
        db_out_buffer[db_out_len++] = hexchars[sigval % 16];
        db_out_buffer[db_out_len++] = 0;
        db_checksum_packet(start);
        db_send_packet();
    } else {
        printf("Fault %d at %04x ... \n", db_trap, pc);
        dump_state();
    }
}

void db_handlefd() {
    int len;

#ifndef HMSMON_THREAD
    if (gdbfd < 0) {
        struct sockaddr addr;
        socklen_t addr_len = sizeof(addr);

        gdbfd = accept(serverfd, &addr, &addr_len);
        debuggerfd = gdbfd;
        printf("GDB connected!\n");
    }
#endif

    
    if ((len = read(gdbfd, db_in_buffer + db_len, 
                    sizeof(db_in_buffer) - db_len - 1)) == 0) {
        printf("debugger closed connection.\n");
        close(gdbfd);
        gdbfd = -1;
        debuggerfd = serverfd;
        /* try to rerun */
        db_trap = 0;
        return;
    }
    db_len += len;
    db_parse_packet();
}

static void sigint_handler(int sig) {
    db_trap = SIGINT_EXCEPTION;
}

#ifdef DB_THREAD
int db_thread(void *args) {
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    while (1) {
        memset(&addr, 0, sizeof(addr));
        gdbfd = accept(serverfd, &addr, &addr_len);
        printf("GDB connected!\n");

        db_continueing = 0;
        db_trap = SIGINT_EXCEPTION;

        while (gdbfd >= 0) {
            sleep(1);
        }
        printf("GDB connection closed.\n");
    }
}
#endif

void db_init() {
    struct sigaction sigact;

    gdbfd = -1;
    serverfd = create_anon_socket(&monitorport);
if (serverfd < 0) {
printf("Can't start debugging server\n");
                abort();
                return;
            }
        printf("Debugging-Server started on port %d.\n", monitorport);
    debuggerfd = serverfd;

#ifdef DB_THREAD
    clone(db_thread, malloc(16*1024)+16*1024 - sizeof(int32), 
          CLONE_FS | CLONE_FILES | CLONE_VM,
          NULL);
#endif

    sigact.sa_handler = sigint_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGQUIT, &sigact, NULL);
}
