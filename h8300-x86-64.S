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
 * $Id: h8300-i586.S 152 2005-08-13 15:24:05Z hoenicke $
 */

#if defined(__CYGWIN__) || defined(__OpenBSD__)
#define EXTERN(x) _##x
#define LOCAL(x) .L##x
#else
#define EXTERN(x) x
#define LOCAL(x) .L##x
#endif
	
	.text
	.align 2
.globl EXTERN(run_cpu_asm)
#if defined(__CYGWIN__) || defined(__OpenBSD__)
	.def EXTERN(run_cpu_asm); .scl 2; .type 32; .endef
#else
	.type EXTERN(run_cpu_asm),@function
#endif

EXTERN(run_cpu_asm):
	push	%rbp
	mov	%rsp, %rbp  /* needed by GDB */
	callq	EXTERN(mcount)
	push	%rbx
	push	%r14

	movzwl	EXTERN(pc)(%rip), %esi
	test	$1, %esi
	jnz	LOCAL(unaligned)

	movzbl	EXTERN(ccr)(%rip), %ebx
	mov	EXTERN(cycles)(%rip), %r9
	movq	EXTERN(next_timer_cycle)(%rip), %rdi
	orb	%bl, %bl
	cmovs	EXTERN(next_nmi_cycle)(%rip), %rdi
	subq	%rdi, %r9
	js	LOCAL(start)

LOCAL(clean_up):
	movw	%si, EXTERN(pc)(%rip)
	movb	%bl, EXTERN(ccr)(%rip)
	or	%bl, %bl
	movq	EXTERN(next_timer_cycle)(%rip), %rdi
	orb	%bl, %bl
	cmovs	EXTERN(next_nmi_cycle)(%rip), %rdi
	addq	%rdi, %r9
	movq	%r9, EXTERN(cycles)(%rip)
	pop	%r14
	pop	%rbx
	pop	%rbp
	ret
	
LOCAL(check_log):
	testb	$0x81, EXTERN(memtype)(%rsi)
	jnz	LOCAL(clean_up)

	movq	%r9, %r14
	movw	%si, EXTERN(pc)(%rip)
	movb	%bl, EXTERN(ccr)(%rip)

	movq	EXTERN(next_timer_cycle)(%rip), %rax
	orb	%bl, %bl
	cmovs	EXTERN(next_nmi_cycle)(%rip), %rax
	addq	%rax, %r9
	movq	%r9, EXTERN(cycles)(%rip)

	callq	EXTERN(dump_state)
	movq	%r14, %r9
	movb	EXTERN(ccr)(%rip), %bl
	movzwq	EXTERN(pc)(%rip), %rsi
	jmp	LOCAL(dispatch_opcode)

LOCAL(default_epilogue):
	addq	$2,%r9

LOCAL(default_epilogue_nocycles):
	xorl	%eax,%eax
	testb	$0x40, EXTERN(memtype)(%rsi)
	setz	%al
	addq	$2,%rsi
	leaq	(%r9,%rax,4),%r9
		
LOCAL(loop):
	orq	%r9, %r9
	jns	LOCAL(clean_up)
LOCAL(start):	
	testb	$0x83, EXTERN(memtype)(%rsi)
	jnz	LOCAL(check_log)
LOCAL(dispatch_opcode):
	xorl    %ecx,%ecx
	movb	EXTERN(memory)(%rsi), %cl
	xorl    %edx,%edx
	movb	EXTERN(memory)+1(%rsi), %dl
	movl	opctable(,%rcx,4),%eax
	incl	EXTERN(frame_asmopcstat)(,%rcx,4)
	jmpq	*%rax

#include "h8300-x86-64.inc"

LOCAL(illegalOpcode):
	movl	$4, EXTERN(db_trap)(%rip)
	jmp	LOCAL(clean_up)

LOCAL(illegaladdr):
LOCAL(unaligned):
	movl	$10, EXTERN(db_trap)(%rip)
	jmp	LOCAL(clean_up)

LOCAL(trap):
	movl	$5, EXTERN(db_trap)(%rip)
	jmp	LOCAL(clean_up)

	.section	.rodata
add2flags:
	.byte	0x00, 0x0a, 0x02, 0x08
