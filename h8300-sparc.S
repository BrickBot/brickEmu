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
 * $Id: h8300-sparc.S 57 2004-03-12 10:03:46Z hoenicke $
 */

#define EXTERN(x) x
	
	.file	"h8300-sparc.S"
	
	.section	".text"
	.align 4
	.globl EXTERN(run_cpu_asm)
	.type EXTERN(run_cpu_asm),#function

#define NEXT_XX_CYCLE  %i4
#define MEMTYPE %i5
#define CCR     %l0
#define PC      %l1
#define CYCLES  %l2
#define OPCTABLE %l3
#define OPCSTAT  %l4
#define MEMORY   %l5
#define REG      %l6
#define HI2CCR   %l7

EXTERN(run_cpu_asm):
	!#PROLOGUE# 0
	save	%sp, -96, %sp		! create new stack frame and register
	!#PROLOGUE# 1

	sethi	%hi(EXTERN(pc)), %i1
	sethi	%hi(EXTERN(cycles)), %i2
	sethi	%hi(EXTERN(ccr)), %i3
	sethi	%hi(EXTERN(memtype)), MEMTYPE
	sethi	%hi(EXTERN(memory)), MEMORY
	sethi	%hi(EXTERN(reg)), REG
	sethi	%hi(EXTERN(hi2ccr)+2), HI2CCR
	sethi	%hi(EXTERN(opctable)), OPCTABLE
	sethi	%hi(EXTERN(frame_asmopcstat)), OPCSTAT

	or	MEMTYPE, %lo(EXTERN(memtype)), MEMTYPE
	or	MEMORY, %lo(EXTERN(memory)), MEMORY
	or	REG, %lo(EXTERN(reg)), REG
	or	HI2CCR, %lo(EXTERN(hi2ccr)+2), HI2CCR
	or	OPCTABLE, %lo(EXTERN(opctable)), OPCTABLE
	or	OPCSTAT, %lo(EXTERN(frame_asmopcstat)), OPCSTAT
	
	lduh	[%i1+%lo(EXTERN(pc))],PC

	sethi	%hi(EXTERN(next_nmi_cycle)), NEXT_XX_CYCLE
	ldub	[%i3+%lo(EXTERN(ccr))],CCR
	btst	0x80,CCR
	bne,a	1f
	or	NEXT_XX_CYCLE, %lo(EXTERN(next_nmi_cycle)), NEXT_XX_CYCLE
	sethi	%hi(EXTERN(next_timer_cycle)), NEXT_XX_CYCLE
	or	NEXT_XX_CYCLE, %lo(EXTERN(next_timer_cycle)), NEXT_XX_CYCLE
1:	

	ld	[%i2+%lo(EXTERN(cycles))],CYCLES
	ld	[NEXT_XX_CYCLE], NEXT_XX_CYCLE
	
	andcc	PC, 1, %g0
	bne	unaligned
	subcc	CYCLES, NEXT_XX_CYCLE, CYCLES
	
	bneg,a  start
	ldub	[MEMTYPE+PC], %o2

clean_up:
	add	CYCLES, NEXT_XX_CYCLE, CYCLES
	st	CYCLES, [%i2+%lo(EXTERN(cycles))]
	sth	PC, [%i1+%lo(EXTERN(pc))]
	stb	CCR, [%i3+%lo(EXTERN(ccr))]
	ret
	restore
	
check_log:
	btst	0x81,%o2
	bne	clean_up

	sth	PC, [%i1+%lo(EXTERN(pc))]
	stb	CCR, [%i3+%lo(EXTERN(ccr))]
	call	EXTERN(dump_state) 
	nop
	b	dispatch_opcode
	lduh	[MEMORY+PC], %o0

default_epilogue:
	add	CYCLES, 2, CYCLES

default_epilogue_nocycles:
	ldub	[MEMTYPE+PC], %o0
	andcc	%o0, 0x40, %g0
	bne	loop
	add	PC, 2, PC
	
	add	CYCLES, 4, CYCLES
loop:
	addcc	CYCLES, 0, %g0
	bpos	clean_up
	ldub	[MEMTYPE+PC], %o2

start:
	btst	0x83, %o2
	bne	check_log
	lduh	[MEMORY+PC], %o0
	
dispatch_opcode:
	srl	%o0, 8, %o1
	sll	%o1, 2, %o7
	ld	[OPCSTAT+%o7], %o2
	add	%o2, 1, %o2
	st	%o2, [OPCSTAT+%o7]
	ld	[OPCTABLE+%o7], %o2
	jmp	%o2
	and	%o0, 255, %o0
	
illegalOpcode:
	mov	4, %o0

setTrap:
	sethi	%hi(EXTERN(db_trap)),%o1
	b	clean_up
	st	%o0, [%o1+%lo(EXTERN(db_trap))]

#include "h8300-sparc.inc"

	.section	".text"
	.align 4
illegaladdr:	
unaligned:
	b	setTrap
	mov	10, %o0

trap:
	b	setTrap
	mov	5, %o0

