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
 * $Id: h8300.c 320 2006-03-17 13:35:34Z hoenicke $
 */

#include <stdio.h>
#include "h8300.h"
#include "memory.h"
#include "peripherals.h"
#include "frame.h"
#include "debugger.h"

#undef DEBUG_CPU_ASM
#undef DEBUG_CPU

#define   TRAP_EXCEPTION 5
#define ILLOPC_EXCEPTION 4
#define SIGINT_EXCEPTION 7

#define BP_EXEC  0x0101
#define BP_READ  0x0808
#define BP_WRITE 0x0404

uint8  reg[16];
uint16 pc;
uint8  ccr;

long cycles, next_timer_cycle, next_nmi_cycle;
int irq_disabled_one;
volatile int db_trap;
int db_singlestep;
static uint16 db_singlestep_pc;
static uint8 db_singlestep_memtype;

#ifdef HAVE_RUN_CPU_ASM
extern void run_cpu_asm(void);
#endif

#define GET_OPCODE \
    if ((*(uint16*) (memtype+pc)) & BP_EXEC) \
        goto trap; \
    opc = GET_WORD_CYCLES(pc); \
    pc += 2

#define READ_BYTE(addr) \
    GET_BYTE_CYCLES(addr); \
    if ((*(uint8*) (memtype+(addr))) & BP_READ) \
        goto trap

#define READ_WORD(addr) \
    GET_WORD_CYCLES(addr); \
    if ((*(uint16*) (memtype+(addr))) & BP_READ) \
        goto trap

#define WRITE_BYTE(addr, val) \
    if ((*(uint8*) (memtype+(addr))) & BP_WRITE) \
        goto trap; \
    SET_BYTE_CYCLES(addr, val)

#define WRITE_WORD(addr, val) \
    if ((*(uint16*) (memtype+(addr))) & BP_WRITE) \
        goto trap; \
    SET_WORD_CYCLES(addr, val)


void dump_state(void) {
    int i;
    for (i = 0; i < 8; i++) {
	printf("  R%d: %02x%02x (%3d:%3d == %5d)\n",
	       i, reg[i], reg[i+8], reg[i], reg[i+8], GET_REG16(i));
    }
    printf ("  PC: %04x   ccr: %c%c%c%c%c%c%c%c  cycles: %10ld->%10ld\n",
	    pc, 
	    (ccr & 0x80 ? 'I':'.'),
	    (ccr & 0x40 ? '-':'.'),
	    (ccr & 0x20 ? 'H':'.'),
	    (ccr & 0x10 ? '-':'.'),
	    (ccr & 0x08 ? 'N':'.'),
	    (ccr & 0x04 ? 'Z':'.'),
	    (ccr & 0x02 ? 'V':'.'),
	    (ccr & 0x01 ? 'C':'.'),
	    cycles,
	    ccr & 0x80 ? next_nmi_cycle : next_timer_cycle);
    printf ("trap: %02x\n", db_trap);
    printf ("%04x:  %02x%02x %02x%02x   [%02x%02x %02x%02x]\n", pc, 
	    memory[pc], memory[pc+1], memory[pc+2], memory[pc+3],
	    memtype[pc], memtype[pc+1], memtype[pc+2], memtype[pc+3]);

    printf("Stack Trace:\n");
    frame_dump_stack(stdout, GET_REG16(7));
}

static void implement(char *opcode) {
    pc -= 2;
    printf("0x%04x: Implement Opcode %04x (%s)\n", pc, GET_WORD(pc), opcode);
    db_trap = ILLOPC_EXCEPTION;
}

extern unsigned int frame_opcstat[256];

#ifdef DEBUG_CPU_ASM
#include <string.h>
#include <stdlib.h>

static void debug_set_byte(uint16 old_pc, uint16 addr, uint8 value) {
    if (memory[addr] != value) { 
	printf("Unexpected memory diff at %04x (%02x) %04x %02x != %02x\n", 
	       old_pc, memory[old_pc], addr, value, memory[addr]); 
	return;
    }
}
static void debug_set_word(uint16 old_pc, uint16 addr, uint16 value) {
    int memval = (memory[addr] << 8) | memory[addr+1];
    if (memval != value) {
	printf("Unexpected memory diff at %04x (%02x) %04x %04x != %04x\n", 
	       old_pc, memory[old_pc], addr, value, memval); 
	return;
    }
}
static void debug_cpu_asm(void) {
    long old_next_timer_cycle = next_timer_cycle;
    long old_next_nmi_cycle = next_nmi_cycle;
    
    uint8 old_reg[16];
    uint16 old_pc;
    uint8  old_ccr;
    long old_cycles;
    uint8 new_reg[16];
    uint16 new_pc;
    uint8  new_ccr;
    long new_cycles;
    uint8 opcval;
    unsigned int opc;

    while ((long) (cycles - (ccr & 0x80 ? old_next_nmi_cycle
			     : old_next_timer_cycle)) < 0) {
	memcpy(old_reg, reg, 16);
	old_pc = pc;
	old_ccr = ccr;
	old_cycles = cycles;
	
	/* run for at most one opcode */
	next_timer_cycle = next_nmi_cycle = cycles + 1;
	run_cpu_asm();
	
	/* abort, if asm could not handle opcode */
	if (cycles == old_cycles)
	    break;

	/* store new state */
	memcpy(new_reg, reg, 16);
	new_pc = pc;
	new_ccr = ccr;
	new_cycles = cycles;
	
	/* restore old state */
	memcpy(reg, old_reg, 16);
	pc = old_pc;
	ccr = old_ccr;
	cycles = old_cycles;
	
	GET_OPCODE;
	opcval = opc & 0xff;
#ifdef DEBUG_CPU
	printf ("Exec %04x: %04x\n", pc-2, opc);
#endif
	frame_opcstat[opc>>8]++;
	
	switch(opc >> 8) {
#define GET_BYTE(addr) memory[(uint16)(addr)]
#define GET_WORD(addr) ((memory[(uint16)(addr)] << 8) | memory[(uint16) (addr) + 1])
#define SET_BYTE(addr, val) debug_set_byte(old_pc, addr,val)
#define SET_WORD(addr, val) debug_set_word(old_pc, addr,val)
#define frame_begin(stack, irq)
#define frame_end(stack, irq)
#define frame_switch(oldstack, newstack)
#define MAKE_LABEL(label) while(0)
#include "h8300.inc"
#undef MAKE_LABEL
#undef GET_BYTE
#undef GET_WORD
#undef SET_BYTE
#undef SET_WORD
#undef frame_begin
#undef frame_end
#undef frame_switch
#define GET_BYTE(addr) \
    (!(memtype[(uint16)(addr)] & MEMTYPE_DIV) ? \
       memory[(uint16)(addr)] : get_byte_div(addr))

#define GET_WORD(addr) \
    (!((addr) & 1) && !(memtype[(uint16)(addr)] & MEMTYPE_DIV) ? \
      (memory[(uint16)(addr)] << 8) | memory[(uint16) (addr) + 1] \
     : get_word_div(addr))
	default:
	illOpc:
	    db_trap = ILLOPC_EXCEPTION;
	trap:
	fault:
	    printf ("Unexpected fault at %04x (%02x)\n", 
		    old_pc, memory[old_pc]);
	    abort();
	}
	if (new_pc != pc || new_ccr != ccr || new_cycles != cycles
	    || memcmp(reg, new_reg, 16)) {
	    printf ("Unexpected difference at %04x (%02x)\n", 
		    old_pc, memory[old_pc]);
	    printf ("Reference State\n");
	    dump_state();
	    memcpy(reg, new_reg, 16);
	    pc = new_pc;
	    ccr = new_ccr;
	    cycles = new_cycles;
	    printf ("Asm State\n");
	    dump_state();
	    memcpy(reg, old_reg, 16);
	    pc = old_pc;
	    ccr = old_ccr;
	    cycles = old_cycles;
	    printf ("Old State\n");
	    dump_state();
	    abort();
	}
    }
    next_timer_cycle = old_next_timer_cycle;
    next_nmi_cycle = old_next_nmi_cycle;
    return;
}

#define run_cpu_asm debug_cpu_asm
#endif

void run_cpu(void) {
    uint16 oldpc = pc;
    uint8 opcval;
    unsigned int opc;

    db_singlestep_pc = 0xffff;
    do_reset();

    while (1) {
	if (pc & 1)
	    db_trap = 10;
	if (db_trap || db_singlestep) {
	    if (db_singlestep) {
		oldpc = pc;
		db_trap = TRAP_EXCEPTION;
	    }
	    if (0) {
	trap:
		if (db_singlestep_pc == oldpc) {
		    db_singlestep_pc = 0xffff;
		    db_singlestep = 1;
		    memtype[oldpc] = db_singlestep_memtype;
		}
		db_trap = TRAP_EXCEPTION;
        fault:
		pc = oldpc;
	    }
	handletrap:
	    periph_handletrap();
	}
	    

	if (irq_disabled_one) {
	    irq_disabled_one = 0;

	} else {

#ifdef HAVE_RUN_CPU_ASM
	    if (!db_singlestep) {
		run_cpu_asm();
		if (db_trap)
		    goto handletrap;
	    }
#endif
	    if ((cycles - (ccr & 0x80 ? next_nmi_cycle
			   : next_timer_cycle)) >= 0) {
		if (!db_singlestep) {
		    check_irq();

		} else {

		    /* singlestep handling is a bit more difficult */
		    db_singlestep_pc = pc;
		    check_irq();
		    if (db_singlestep_pc != pc) {
			/* interrupt occured: step over it */
			db_singlestep_memtype = memtype[db_singlestep_pc];
			memtype[db_singlestep_pc] |= MEMTYPE_BREAKPOINT;
			db_singlestep = 0;
		    }
		}
		continue;
	    }
	}
	    
	if (memtype[pc] & 0x03)
	    dump_state();

	oldpc = pc;
	GET_OPCODE;
	opcval = opc & 0xff;
#ifdef DEBUG_CPU
	printf ("Exec %04x: %04x\n", pc-2, opc);
#endif
	frame_opcstat[opc>>8]++;

	switch(opc >> 8) {
#define MAKE_LABEL(label) __asm__ ("\n.L" label ":\n")
#include "h8300.inc"
	default:
	illOpc:
	    db_trap = ILLOPC_EXCEPTION;
	    goto fault;
	}
    }
}
