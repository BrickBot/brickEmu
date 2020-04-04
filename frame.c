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
 * $Id: frame.c 336 2006-04-01 19:27:09Z hoenicke $
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "types.h"
#include "frame.h"
#include "h8300.h"
#include "memory.h"
#include "symbols.h"
#include "hash.h"

#define PROFILE_CPU
#undef LOG_CALLS
#undef LOG_CALLS_IRQ
#undef LOG_TIMERS
#undef LOG_THREADS
#undef LOG_MEMORY

#ifdef LOG_CALLS
FILE *framelog;
#endif

unsigned int frame_opcstat[256];
unsigned int frame_asmopcstat[256];

typedef struct callee_info {
    unsigned long calls;
    unsigned long cycles;
} callee_info;

typedef struct profile_info {
    unsigned long local_cycles;
    unsigned long irq_cycles;
    unsigned long total_cycles;
    unsigned long max_local_cycles;
    unsigned long max_total_cycles;
    unsigned long calls;
    uint8 isIRQ;
    hash_type callees;
} profile_info;

typedef struct frame_info {
    int32 startcycle;
    int32 childcycles;
    int32 irqcycles;
    int32 threadcycles;
    uint16 pc;
    uint16 fp;
} frame_info;

typedef struct thread_info {
    uint16 savefp;
    uint16 minfp;
    int32  switchcycle;
    int    num_frames;
    int    size_frames;
    struct frame_info frames[1];
} thread_info;

#ifdef HASH_PROFILES
static hash_type profiles;
#else
static profile_info *profiles[65536];
#endif
static hash_type threads;
static thread_info *current_thread;

//#define VERBOSE_FRAME
#define MIN_DESC_LEVEL 3

#define READ_WORD(offset) ((memory[offset] << 8) | memory[(offset) + 1])

void frame_dump_stack(FILE *out, uint16 fp) {
    char buf[11];
    char *funcname;
    int lastpc = pc;
    int i;
    thread_info *thread;
    thread = fp == GET_REG16(7) ? current_thread : hash_get(&threads, fp);
    
    if (!thread)
	return;

    for (i = thread->num_frames; i >= 0; i--) {
	int fpc = thread->frames[i].pc & ~1;
	int fp = thread->frames[i].fp;
	funcname = symbols_get(fpc, 0);
	if (!funcname) {
	    snprintf(buf, 11, "0x%04x", fpc);
	    funcname=buf;
	}
	fprintf(out, "    %04x: %30s [%04x+%04x]\n", 
	       fp, funcname, fpc, lastpc - fpc);
	lastpc = (memory[fp] << 8 | memory[fp+1]);
    }
    return;
}

void frame_switch(uint16 oldframe, uint16 newframe) {
    if (oldframe == newframe)
	return;

#ifdef LOG_CALLS
    if (current_thread) {
	int lastpc = pc;
	int i;
	char buf[10];
	char *funcname;
	for (i = current_thread->num_frames; i >= 0; i--) {
	    int fpc = current_thread->frames[i].pc & ~1;
	    int fp = current_thread->frames[i].fp;
	    funcname = symbols_get(fpc, 0);
	    if (!funcname) {
		snprintf(buf, 10, "0x%04x", fpc);
		funcname=buf;
	    }
	    fprintf(framelog, "    %04x: %30s%s [%04x+%04x]\n", 
		    fp, funcname, 
		    current_thread->frames[i].pc & 1 ? "(I)" : "   ",
		    fpc, lastpc - fpc);
	    lastpc = (memory[fp] << 8 | memory[fp+1]);
	}
    }
#endif

#ifdef VERBOSE_FRAME
    printf("Frame switch: %04x --> %04x    cycles: %11ld\n", 
	   oldframe, newframe, cycles);
#endif
    if (current_thread) {
	current_thread->switchcycle = cycles;
	current_thread->savefp = oldframe;
	hash_move(&threads, 0, oldframe);
    }
    
    current_thread = hash_move(&threads, newframe, 0);
#ifdef LOG_CALLS
    fprintf(framelog, "Frame switch: %04x --> %04x\n", oldframe, newframe);
    if (current_thread) {
	int lastpc = pc;
	int i;
	char buf[10];
	char *funcname;
	for (i = current_thread->num_frames; i >= 0; i--) {
	    int fpc = current_thread->frames[i].pc & ~1;
	    int fp = current_thread->frames[i].fp;
	    funcname = symbols_get(fpc, 0);
	    if (!funcname) {
		snprintf(buf, 10, "0x%04x", fpc);
		funcname=buf;
	    }
	    fprintf(framelog, "    %04x: %30s%s [%04x+%04x]\n", 
		    fp, funcname, 
		    current_thread->frames[i].pc & 1 ? "(I)" : "   ",
		    fpc, lastpc - fpc);
	    lastpc = (memory[fp] << 8 | memory[fp+1]);
	}
    }
#endif
    if (!current_thread) {
	current_thread = hash_create(&threads, 0, 
				     sizeof(thread_info) + 
				     2* sizeof(frame_info));
	current_thread->minfp = newframe;
	current_thread->size_frames = 2;
	current_thread->num_frames = 0;
	current_thread->switchcycle = cycles;
	memset(&current_thread->frames[0], 0, sizeof(frame_info));
	current_thread->frames[0].startcycle = cycles;
	current_thread->frames[0].pc = pc;
    }
    current_thread->frames[current_thread->num_frames].threadcycles
	+= cycles - current_thread->switchcycle;
}

/**
 * Add a new frame to the current thread.  Resize the stack structure
 * if necessary.
 */
static void frame_grow_num_frames() {
    current_thread->num_frames++;

    if (current_thread->num_frames >= current_thread->size_frames) {
	size_t newsize;

	current_thread->size_frames += 10;
	newsize = sizeof(thread_info)
	    + current_thread->size_frames * sizeof(frame_info);
	current_thread = hash_realloc(&threads, 0, newsize);
    }
}

/**
 * Insert a new frame before the lowest frame.  This is called from a
 * return statement when the function had no caller, because the stack
 * was artificially initialized by execi method of brickOS or similar.
 */
static void frame_insert_subframe(uint16 fp) {
    /* This function is only called when there is only the bottom frame.  No
     * need to regrow current_thread.
     */
    current_thread->num_frames = 1;
    current_thread->frames[1] = current_thread->frames[0];
    memset(&current_thread->frames[0], 0, sizeof(frame_info));
    /* assume that the function starts at the return address. */
    current_thread->frames[0].pc = memory[fp] << 8 | memory[fp+1];
    current_thread->frames[0].startcycle = cycles;
}

static void frame_create_callee(profile_info *prof, uint16 pc) {
}

static void frame_add_callee(profile_info *prof, uint16 pc, unsigned long cycles) {
    callee_info * callee = hash_get(&prof->callees, pc);
    if (!callee) {
	callee = hash_create(&prof->callees, pc, sizeof(callee_info));
	callee->cycles = 0;
	callee->calls = 0;
    }
    callee->calls++;
    callee->cycles += cycles;
}

/**
 * Handle a call instruction and add profile information for the
 * called and calling function.
 */
void frame_begin(uint16 fp, int in_irq) {
    frame_info *frame, *pframe;
    profile_info *pprof;

    /* If we there is no current thread, just return.  The current thread
     * will be created by the opcode that initializes the stack pointer. 
     * So there should be always a current thread at the moment we call
     * a function.
     */
    if (!current_thread)
	return;

    if (fp < current_thread->minfp)
	current_thread->minfp = fp;

    /* Add a new frame to the current thread.  Resize the stack structure
     * if necessary.
     */
    frame_grow_num_frames();
    
    /* Initialize the new frame.
     */
    frame = &current_thread->frames[current_thread->num_frames];
    frame->startcycle = cycles;
    frame->childcycles = 0;
    frame->irqcycles = 0;
    frame->threadcycles = 0;
    frame->pc = pc | (in_irq ? 1 : 0);
    frame->fp = fp;

#ifdef LOG_CALLS
#ifndef LOG_CALLS_IRQ
    int i, rec_in_irq = 0;
    /* Check if this frame was directly or indirectly called by interrupt. */
    for (i = current_thread->num_frames; i >= 0; i--) {
	if (current_thread->frames[i].pc & 1) {
	    rec_in_irq = 1;
	    break;
	}
    }
    /* Log the call to framelog, unless called from interrupt. */
    if (!rec_in_irq)
#endif
    {
	char* funcname, buf[10];
	funcname = symbols_get(pc, 0);
	if (!funcname) {
	    snprintf(buf, 10, "0x%04x", pc);
	    funcname=buf;
	}
	fprintf(framelog, "%*s%s(%04x,%04x,%04x,%04x) fp: %04x klock: %d inirq: %d\n",
		current_thread->num_frames * 3, "", funcname,
		pc < 0x8000 ? GET_REG16(6) : GET_REG16(0), 
		pc < 0x8000 ? READ_WORD(GET_REG16(7)+2) : GET_REG16(1),
		pc < 0x8000 ? READ_WORD(GET_REG16(7)+4) : GET_REG16(2),
		pc < 0x8000 ? READ_WORD(GET_REG16(7)+6) : GET_REG16(3),
		GET_REG16(7),
		memory[symbols_getaddr("_kernel_lock")], in_irq);
    }
#endif

    /* get parent frame and add a new callee to its profile information. */
    pframe = frame - 1;
#ifdef HASH_PROFILES
    pprof = hash_get(&profiles, pframe->pc & ~1);
#else
    pprof = profiles[pframe->pc & ~1];
#endif
    if (!pprof) {
#ifdef HASH_PROFILES
       pprof = hash_create(&profiles, pframe->pc & ~1, sizeof(profile_info));
#else
       pprof = profiles[pframe->pc & ~1] = malloc(sizeof(profile_info));
#endif
       memset(pprof, 0, sizeof(profile_info));
       hash_init(&pprof->callees, 5);
    }
}

void frame_end(uint16 fp, int in_irq) {
    unsigned long local, total;
    frame_info * frame;
    frame_info * pframe;
    profile_info *prof, *pprof;

    if (!current_thread)
	return;

    /* If this is the bottom most frame insert a new frame below */
    if (current_thread->num_frames == 0)
	frame_insert_subframe(fp);

    frame = &current_thread->frames[current_thread->num_frames];
    /* Check if frame pointers match */
    while (frame->fp && frame->fp < fp) {
	/* This means that the function on top of the stack had no return.
	 * We just remove it as if it never had been called and reassign
	 * its cycles to the caller.
	 */
	(frame-1)->irqcycles += frame->irqcycles;
	(frame-1)->threadcycles += frame->threadcycles;

	if (--current_thread->num_frames == 0)
	    frame_insert_subframe(fp);
	frame = &current_thread->frames[current_thread->num_frames];
    }

#ifdef LOG_CALLS
#ifndef LOG_CALLS_IRQ
    int i, rec_in_irq = 0;
    for (i = current_thread->num_frames; i >= 0; i--)
	if (current_thread->frames[i].pc & 1)
	    rec_in_irq = 1;
    if (!rec_in_irq)
#endif
    {
	fprintf(framelog, "%*s<- %04x\n",
		current_thread->num_frames * 3, "", 
		pc < 0x8000 ? GET_REG16(6) : GET_REG16(0));
    }
#endif

    current_thread->num_frames--;
    /* Compute number of cycles spent in child frame and its children */
    total = cycles - frame->startcycle - frame->threadcycles
	- frame->irqcycles;
    local = total - frame->childcycles;

    /* If fp is 0 this means that the return address has been put on stack
     * manually.  We don't add the cycles of the "child" function in that
     * case.
     */
    if (frame->fp != 0) {
	/* get parent frame */
	pframe = frame - 1;
	
	/* Move irq cycles and cycles spent in different threads to parent
	 * frame */
	pframe->irqcycles += frame->irqcycles;
	pframe->threadcycles += frame->threadcycles;
	
	/* Add the total number of cycles to child cycles of parent */
	if ((frame->pc & 1)) {
	    /* The function that finished was an interrupt handler */
	    pframe->irqcycles += total;
	} else {
	    pframe->childcycles += total;
	}


#ifdef HASH_PROFILES
	pprof = hash_get(&profiles, pframe->pc & ~1);
#else
	pprof = profiles[pframe->pc & ~1];
#endif
	/* since fp was okay, we know that this function was called regularly
	 * and we have a profile entry for its caller.
	 */
	frame_add_callee(pprof, frame->pc, total);
    }

#ifdef LOG_CALLS
    /* log data structures if a method finished that manipulates them */
#ifdef LOG_TIMERS
    if (frame->pc == symbols_getaddr("_add_timer")
	|| frame->pc == symbols_getaddr("_remove_timer")
	|| frame->pc == symbols_getaddr("_run_timers")) {
	extern void bibo_dump_timers(FILE* file);
	bibo_dump_timers(framelog);
    }
#endif

#ifdef LOG_MEMORY
    if (frame->pc == symbols_getaddr("_mm_init")) {
	extern void bibo_dump_memory(FILE*);
	bibo_dump_memory(framelog);
    }
    if (frame->pc == symbols_getaddr("_malloc")) {
	extern void bibo_dump_memory(FILE*);
	bibo_dump_memory(framelog);
    }
    if (frame->pc == symbols_getaddr("_free")) {
	extern void bibo_dump_memory(FILE*);
	bibo_dump_memory(framelog);
    }
#endif

#ifdef LOG_THREADS
    if (frame->pc == symbols_getaddr("_execi")
	|| frame->pc == symbols_getaddr("_wait")
	|| frame->pc == symbols_getaddr("_make_running")
	|| frame->pc == symbols_getaddr("_shutdown_tasks")
	|| frame->pc == symbols_getaddr("_wakeup")
	|| frame->pc == symbols_getaddr("_wakeup_single")) {
	extern void bibo_dump_threads(FILE*);
	bibo_dump_threads(framelog);
    }
#endif
#endif

    /* add profile info for child frame */
#ifdef HASH_PROFILES
    prof = hash_get(&profiles, frame->pc & ~1);
#else
    prof = profiles[frame->pc & ~1];
#endif
    if (!prof) {
#ifdef HASH_PROFILES
	prof = hash_create(&profiles, frame->pc & ~1, sizeof(profile_info));
#else
	prof = profiles[frame->pc & ~1] = malloc(sizeof(profile_info));
#endif
	memset(prof, 0, sizeof(profile_info));
	hash_init(&prof->callees, 5);
    }
	
    prof->irq_cycles   += frame->irqcycles;
    prof->local_cycles += local;
    prof->total_cycles += total;
    prof->calls++;
    prof->isIRQ |= (frame->pc & 1);
    if (total > prof->max_total_cycles)
	prof->max_total_cycles = total;
    if (local > prof->max_local_cycles)
	prof->max_local_cycles = local;
}

static FILE *proffile;

static void frame_dump_callee(unsigned int key, void *param) {
    char buf[11];
    char *funcname;
    callee_info *callee = param;

    funcname =  symbols_get(key & ~1, 0);

    if (!funcname) {
	snprintf(buf, 11, "0x%04x", key & ~1);
	funcname=buf;
    }


    fprintf(proffile, "    %2s%30s: %6ld calls %11ld cycles\n",
	    key & 1 ? "I:" : "  ",
	    funcname, callee->calls, callee->cycles);
}    

static void frame_close_thread(unsigned int key, void *param) {
    thread_info *thread = param;
    unsigned long switchcycle, local, total;
    frame_info *frame, *pframe;
    profile_info *prof, *pprof;
    int i;

    switchcycle = thread == current_thread ? cycles : thread->switchcycle;
    for (i = thread->num_frames; i >= 0; i--) {
	frame = &thread->frames[i];

	/* Compute number of cycles spent in child frame and its children */
	total = switchcycle - frame->startcycle - frame->threadcycles
	    - frame->irqcycles;
	local = total - frame->childcycles;

	if (i > 0) {
	    pframe = frame - 1;
	    /* Move irq cycles and cycles spent in different threads to parent
	     * frame */
	    pframe->irqcycles += frame->irqcycles;
	    pframe->threadcycles += frame->threadcycles;
	    
	    /* Add the total number of cycles to child cycles of parent */
	    if ((frame->pc & 1)) {
		/* The function that finished was an interrupt handler */
		pframe->irqcycles += total;
	    } else {
		pframe->childcycles += total;
	    }

#ifdef HASH_PROFILES
	    pprof = hash_get(&profiles, pframe->pc & ~1);
#else
	    pprof = profiles[pframe->pc & ~1];
#endif
	    frame_add_callee(pprof, frame->pc, total);
	}

	/* add profile info for child frame */
#ifdef HASH_PROFILES
	prof = hash_get(&profiles, frame->pc & ~1);
#else
	prof = profiles[frame->pc & ~1];
#endif
	if (!prof) {
#ifdef HASH_PROFILES
	    prof = hash_create(&profiles, frame->pc & ~1, sizeof(profile_info));
#else
	    prof = profiles[frame->pc & ~1] = malloc(sizeof(profile_info));
#endif
	    memset(prof, 0, sizeof(profile_info));
	    hash_init(&prof->callees, 5);
	}
	
	prof->irq_cycles   += frame->irqcycles;
	prof->local_cycles += local;
	prof->total_cycles += total;
	prof->calls++;
	prof->isIRQ |= (frame->pc & 1);
	if (total > prof->max_total_cycles)
	    prof->max_total_cycles = total;
	if (local > prof->max_local_cycles)
	    prof->max_local_cycles = local;
    }
}

static void frame_dump_function(unsigned int key, void *param) {
    char buf[10];
    char *funcname;
    profile_info *prof = param;

	
    funcname =  symbols_get(key, 0);

    if (!funcname) {
	snprintf(buf, 10, "0x%04x", key);
	funcname=buf;
    }
    
    if (prof->calls) {
	fprintf(proffile, "%2s%30s: %11ld %11lu %11lu %11lu %11lu %11lu %11lu %11lu\n",
		prof->isIRQ ? "I:" : "  ",
		funcname, prof->calls, prof->local_cycles, 
		prof->total_cycles,
		prof->local_cycles / prof->calls, 
		prof->total_cycles / prof->calls,
		prof->max_local_cycles, prof->max_total_cycles,
		prof->irq_cycles);
    } else {
	fprintf(proffile, "%2s%30s: never finished\n", 
		prof->isIRQ ? "I:" : "  ", funcname);
    }
    hash_enumerate(&prof->callees, frame_dump_callee);
    fprintf(proffile, "\n");
}

void frame_dump_profile() {
    proffile = fopen("profile.txt", "w");

    fprintf(proffile, " PC :  function name                   count       cycles      +child    cyc/call      +child     max cyc      +child   irqcycles\n");
    fprintf(proffile, "=================================================================================================================================\n");

    hash_enumerate(&threads, frame_close_thread);
#ifdef HASH_PROFILES
    hash_enumerate(&profiles, frame_dump_function);
#else
    {
	int i;
	for (i = 0; i< 65536; i++) {
	    if (profiles[i])
		frame_dump_function(i, profiles[i]);
	}
    }
#endif

#ifdef PROFILE_CPU
    {
	int i;
	fprintf(proffile, "\n\n");
	for (i = 0; i < 256; i++) {
	    fprintf(proffile, "Opcode %02x: %10u/%10u\n", i, 
		    frame_asmopcstat[i], frame_opcstat[i]);
	}
    }
#endif
    fclose(proffile);

#ifdef LOG_CALLS
    fclose(framelog);
#endif
}

void frame_init(void) {
#ifdef HASH_PROFILES
    hash_init(&profiles, 101);
#else
    memset(profiles, 0, sizeof(profiles));
#endif
    hash_init(&threads, 5);
    memset(frame_opcstat, 0, sizeof(frame_opcstat));
    memset(frame_asmopcstat, 0, sizeof(frame_asmopcstat));

#ifdef LOG_CALLS
    framelog = fopen("frames.txt", "w");
#endif
    current_thread = NULL;
}
