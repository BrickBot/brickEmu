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
 * $Id: h8300-i586.S 311 2006-03-07 11:15:42Z hoenicke $
 */

#if defined(__CYGWIN__) || defined(__OpenBSD__)
#define EXTERN(x) _##x
#define LOCAL(x) .L##x
#else
#define EXTERN(x) x
#define LOCAL(x) .L##x
#endif
	
	.file	"h8300-i586.S"
	
	.text
	.align 2
.globl EXTERN(run_cpu_asm)
#if defined(__CYGWIN__) || defined(__OpenBSD__)
	.def EXTERN(run_cpu_asm); .scl 2; .type 32; .endef
#else
	.type EXTERN(run_cpu_asm),@function
#endif

EXTERN(run_cpu_asm):
	pushl	%ebp
	movl	%esp, %ebp  /* needed by GDB */
	pushl	%ebx
	call	EXTERN(mcount)
	pushl	%esi
	pushl	%edi

	movzwl	EXTERN(pc), %esi
	testl	$1,%esi
	jnz	LOCAL(unaligned)

	movzbl	EXTERN(ccr), %ebx
	movl	EXTERN(cycles), %ebp
	movl	EXTERN(next_timer_cycle), %edi
	orb	%bl, %bl
	cmovsl	EXTERN(next_nmi_cycle), %edi
	subl	%edi, %ebp
	js	LOCAL(start)

	.align 16
LOCAL(clean_up):
	movw	%si, EXTERN(pc)
	movb	%bl, EXTERN(ccr)
	movl	EXTERN(next_timer_cycle), %edi
	or	%bl, %bl
	cmovsl	EXTERN(next_nmi_cycle), %edi
	addl	%edi, %ebp
	movl	%ebp, EXTERN(cycles)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

	.align 16
LOCAL(check_log):
	testb	$0x81, EXTERN(memtype)(%esi)
	jnz	LOCAL(clean_up)

	movw	%si, EXTERN(pc)
	movb	%bl, EXTERN(ccr)
	movl	%ebp, %edi

	movl	EXTERN(next_timer_cycle), %eax
	or	%bl, %bl
	cmovsl	EXTERN(next_nmi_cycle), %eax
	addl	%eax, %ebp
	movl	%ebp, EXTERN(cycles)

	leal	12(%esp), %ebp
	call	EXTERN(dump_state)
	movl	%edi, %ebp
	movb	EXTERN(ccr), %bl
	jmp	LOCAL(dispatch_opcode)

	.align 16
LOCAL(default_epilogue):
	addl	$2,%ebp

LOCAL(default_epilogue_nocycles):
	xorl	%eax,%eax
	testb	$0x40, EXTERN(memtype)(%esi)
	setz	%al
	addl	$2,%esi
	leal	(%ebp,%eax,4),%ebp
		
LOCAL(loop):
	orl	%ebp,%ebp
	jns	LOCAL(clean_up)
LOCAL(start):	
	testb	$0x83, EXTERN(memtype)(%esi)
	jnz	LOCAL(check_log)
LOCAL(dispatch_opcode):
	xorl    %ecx,%ecx
	movb	EXTERN(memory)(%esi), %cl
	xorl    %edx,%edx
	movb	EXTERN(memory)+1(%esi), %dl
	movl	opctable(,%ecx,4),%eax
	incl	EXTERN(frame_asmopcstat)(,%ecx,4)
	jmp	*%eax

LOCAL(illegalOpcode):
	movl	$4, EXTERN(db_trap)
	jmp	LOCAL(clean_up)

#include "h8300-i586.inc"

LOCAL(illegaladdr):
LOCAL(unaligned):
	movl	$10, EXTERN(db_trap)
	jmp	LOCAL(clean_up)

LOCAL(trap):
	movl	$5, EXTERN(db_trap)
	jmp	LOCAL(clean_up)

	.section	.rodata
add2flags:
	.byte	0x00, 0x0a, 0x02, 0x08
