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
#include "types.h"

/* #define VERBOSE_DEBUG */

#define GDB_MAJOR_VERSION 5

#define BRICK_DEBUG_PORT 6789
#define SIGINT_EXCEPTION 7

/*
 * H8/300 Register Emulation Setup for GDB
 *  + The last 3 registers are not actual h8/300 registers
 *    - Used for tracking virtual simulation statistics
 *    - Can be safely ignored (0 filled)
 *  + Layout
 *    |  #  | Index       | Description
 *    | 1-8 | 0 through 7 | Registers r0 through r7 (r7 is also SP / Stack Pointer)
 *    |  9  |  8 (CCR)    | Condition Code Register (technically only 1 byte)
 *    | 10  |  9 (PC)     | Program Counter
 *    | 11  | 10 (Cycles) | Instruction Cycle Counter (internal simulator)
 *    | 12  | 11 (Inst)   | Instruction Counter (internal simulator)
 *    | 13  | 12 (Tick)   | Clock Tick Counter (internal simulator)
 *  + GDB register implementations for the H8/300:
 *    - GDB 5 expects 13 x 32-bit (4-byte) registers
 *      ^ The real register value is in the high 16 bits, the rest is 0 padded.
 *    - GDB 6 expects 13 x 16-bit (2-byte) registers
 *
 * It makes no sense, but in the 'T' response message format, the register size
 *    is basically reversed--32-bit in GDB 6 and 16-bit in GDB 5.
 */
#define NUM_REGS 13
#define NUM_GP_REGS 8
#define NUM_BYTES_PER_REG sizeof(gdb_register_size_t)
#define NUM_REG_BYTES (NUM_REGS * NUM_BYTES_PER_REG)

#define REG_r0  0
#define REG_r1  1
#define REG_r2  2
#define REG_r3  3
#define REG_r4  4
#define REG_r5  5
#define REG_r6  6
#define REG_r7  7
#define REG_SP  REG_r7
#define REG_CCR 8
#define REG_PC  9

#if !defined(GDB_MAJOR_VERSION)
#  error "GDB_MAJOR_VERSION undefined for debugger.c"
#elif GDB_MAJOR_VERSION < 6
typedef uint32 gdb_register_size_t;
/* NOTE: Reverse register format! */
char *trap_response_format = "$T%02x%02x:%04x;%02x:%04x;\0";
#elif GDB_MAJOR_VERSION >= 6
typedef uint16 gdb_register_size_t;
/* NOTE: Reverse register format! */
char *trap_response_format = "$T%02x%02x:%08x;%02x:%08x;\0";
#else
#  error "GDB version mismatch for " GDB_MAJOR_VERSION " in debugger.c"
#endif

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
        /* Note that this overwrites the existing '$' with the '$' from the response format string */	
		sprintf(db_out_buffer, trap_response_format,
		    sigval, REG_PC, pc, REG_SP, (reg[REG_SP] << 8) | reg[REG_SP + 8]);
        db_out_len = strlen(db_out_buffer);
        break;

        /* Toggle the debug flag */
    case 'd' :
        remote_debug = !(remote_debug);
        break;

        /* Return the value of the CPU registers */
    case 'g' :
        /* Clear the register buffer before populating */
        memset(db_registers, 0, sizeof(db_registers));

        for (i = 0; i < NUM_GP_REGS; i++) {
            db_registers[ NUM_BYTES_PER_REG * i]      = reg[i];     /* High byte */
            db_registers[(NUM_BYTES_PER_REG * i) + 1] = reg[i + 8];  /* Low byte */
        }

        /* 'i' is now 8 from the loop */
#ifdef VERBOSE_DEBUG
        printf("Register counter i is %d (expecting 8); ", i);
#endif

        /* Populate the Condition Code Register (CCR) at index 8 */
		/* CCR is 8 bits, so high byte is 0 */
        db_registers[ NUM_BYTES_PER_REG * i]      = 0;
        db_registers[(NUM_BYTES_PER_REG * i) + 1] = ccr;
        i++;

        /* Populate the Program Counter (PC) at index 9 */
#ifdef VERBOSE_DEBUG
        printf("then %d (expecting 9); ", i);
#endif
        db_registers[ NUM_BYTES_PER_REG * i]      = pc >> 8;   /* High byte */
        db_registers[(NUM_BYTES_PER_REG * i) + 1] = pc & 0xff;  /* Low byte */
        i++;

        /* Populate the 'cycles' pseudo-register at index 10 (non-critical) */
#ifdef VERBOSE_DEBUG
        printf("then %d (expecting 10).\n", i);
#endif
        //db_registers[ NUM_BYTES_PER_REG * i]      = (cycles >> 8) & 0xff;
        //db_registers[(NUM_BYTES_PER_REG * i) + 1] = cycles & 0xff;

        mem2hex(db_registers, 
                db_out_buffer + db_out_len, NUM_REG_BYTES);
        db_out_len += 2 * NUM_REG_BYTES;
        break;

        /* Set the value of the CPU registers - return OK */
    case 'G' :
        hex2mem(packet, (char*) db_registers, NUM_REG_BYTES);
        db_out_buffer[db_out_len++] = 'O';
        db_out_buffer[db_out_len++] = 'K';
        break;

        /* set the value of a single CPU register - return OK */
    case 'P' :
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

        /* Try to read optional parameter, PC unchanged if no parm */
        db_singlestep = (*(packet - 1) == 's');
        db_trap = 0;

        /* Check for an optional address */
        if (hexToInt(&packet,&addr)) {
            pc = addr;
        }

        /* No additional reply info needed beyond the '+' ACK already sent */
        /* Drop the '$' already added to the output buffer and return to the main loop to let the CPU run */
        db_out_len = 0;

        return;

        /* Detach */
    case 'D':
        db_cont = 1;
        break;

        /* Set breakpoint */
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

        /* Remove breakpoint */
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

    }   /* End switch */
    
    /* Reply to the request */
	/* Unrecognized commands return an empty string here */
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
#ifdef VERBOSE_DEBUG
                printf("{Control}-C pressed in debugger: '%02x'\n", db_in_buffer[i]);
            }
            else if (db_in_buffer[i] == '+') {
                printf("Acknowledgment: Received: '%c'\n", db_in_buffer[i]);
            }
            else if (db_in_buffer[i] == '-') {
                printf("Acknowledgment: NEGATIVE: '%c'\n", db_in_buffer[i]);
            }
            else {
                printf("Preceding garbage: '%02x'\n", db_in_buffer[i]);
#endif
            }

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
            printf("Got db packet:  '%s'\n", db_in_buffer);
#endif
            db_out_buffer[db_out_len++] = '+';
            db_send_packet(); /* Send just the '+' and reset db_out_len to 0 */

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
    sigval = db_trap;

    if (gdbfd >= 0) {
		sprintf(db_out_buffer, trap_response_format,
		    sigval, REG_PC, pc, REG_SP, (reg[REG_SP] << 8) | reg[REG_SP + 8]);
        db_out_len = strlen(db_out_buffer);
        int checksum_start = 1;

#ifdef VERBOSE_DEBUG
        printf("Trap message (len: %d): %s\n", db_out_len, db_out_buffer);
#endif
		
        db_checksum_packet(checksum_start);
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
