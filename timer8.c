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
 * $Id: timer8.c 238 2006-02-23 19:18:39Z hoenicke $
 */

#include <string.h>

#include <netinet/in.h> /* for htonx/ntohx */

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"


#define CYCLES_PER_TICK 1


#define TCR_CMIEB 0x80
#define TCR_CMIEA 0x40
#define TCR_OVIE  0x20
#define TCR_CCLR1 0x10
#define TCR_CCLR0 0x08
#define TCR_CKS2  0x04
#define TCR_CKS1  0x02
#define TCR_CKS0  0x01

#define TCSR_CMFB 0x80
#define TCSR_CMFA 0x40
#define TCSR_OVF  0x20
#define TCSR_OS3  0x08
#define TCSR_OS2  0x04
#define TCSR_OS1  0x02
#define TCSR_OS0  0x01

#define TOCR_OCRS  0x10
#define TOCR_OEA   0x08
#define TOCR_OEB   0x04
#define TOCR_OLVLA 0x02
#define TOCR_OLVLB 0x01

static uint8  tcnt[2], tcr[2], tcsr[2], tcora[2], tcorb[2];
static uint8  out[2] = {0,0};
static uint8  stcr;

static uint32 last_cycles[2];
static uint32 my_next_cycle[2];

static uint8 freq[16] = { 31, 3, 6, 10, 31, 31, 31, 31, 
			  31, 1, 5,  8, 31, 31, 31, 31 };

typedef struct {
    uint32 last_cycles[2];
    uint32 my_next_cycle[2];
    uint8  tcnt[2], tcr[2], tcsr[2], tcora[2], tcorb[2];
    uint8  out[2];
    uint8  stcr;
} t8_save_type;

extern void sound_update(int bit, uint32 new_incr);

static int t8_save(void *buffer, int maxlen) {
    int i;
    t8_save_type *data = buffer;

    for (i = 0; i < 2; i++) {
	data->last_cycles[i] = htonl(last_cycles[i]);
	data->my_next_cycle[i] = htonl(my_next_cycle[i]);
	data->tcnt[i]  = tcnt[i];
	data->tcr[i]   = tcr[i];
	data->tcsr[i]  = tcsr[i];
	data->tcora[i] = tcora[i];
	data->tcorb[i] = tcorb[i];
	data->out[i]   = out[i];
    }
    data->stcr = stcr;
    return sizeof(t8_save_type);
}

static void t8_load(void *buffer, int len) {
    int i;
    t8_save_type *data = buffer;
    for (i = 0; i < 2; i++) {
	last_cycles[i] = htonl(data->last_cycles[i]);
	my_next_cycle[i] = htonl(data->my_next_cycle[i]);
	tcnt[i]  = data->tcnt[i];
	tcr[i]   = data->tcr[i];
	tcsr[i]  = data->tcsr[i];
	tcora[i] = data->tcora[i];
	tcorb[i] = data->tcorb[i];
	out[i]   = data->out[i];
    }
    stcr = data->stcr;
}

static void t8_reset() {
    tcnt[0] = tcnt[1] = 0;
    tcr[0]  = tcr[1]  = 0;
    tcsr[0] = tcsr[1] = 0x10;
    tcora[0] = tcora[1] = tcorb[0] = tcorb[1] = 0xff;
    stcr = 0xf8;
    last_cycles[0] = last_cycles[0] = cycles;
}

static void t8_check_next_cycle() {
    int shift, nr;

    if (((tcsr[0] & tcr[0]) | (tcsr[1] & tcr[1])) & 0xe0) {
	/* interrupt is pending */
	next_timer_cycle = cycles;
	return;
    }
    
    for (nr = 0; nr < 1; nr++) {
	int nextev;
	int32 next = 0x100 << 10;
	shift = freq[(tcr[nr] & 7) | ((stcr & (1+nr)) << (3-nr))];
	if (shift == 31)
	    continue;
	
	nextev = (0x100 - tcnt[nr]) << shift;
	if (next > nextev)
	    next = nextev;
	if ((tcsr[nr] & TCR_CMIEA) && tcnt[nr] < tcora[nr]) {
	    nextev = (tcora[nr] + 1 - tcnt[nr]) << shift;
	    if (next > nextev)
		next = nextev;
	}
	if ((tcsr[nr] & TCR_CMIEB)) {
	    nextev = (tcorb[nr] + 1 - tcnt[nr]) << shift;
	    if (next > nextev)
		next = nextev;
	}
	my_next_cycle[nr] = last_cycles[nr] + next;

	
	//printf("Waiting tm%d for %d  (%d/%d) -> (%d/%d)\n", nr, next, 
	//   tcnt[0], tcnt[1], tcora[0], tcora[1]);

	if (next < (int32) (next_timer_cycle - last_cycles[nr])) {
	    next_timer_cycle = last_cycles[nr] + next;
	}
    }
}

static void t8_incr_tcnt(int nr, unsigned int incr, int shift) {
    int first, second, fshift, cnt;

    if (tcnt[nr] <= tcora[nr] && tcnt[nr] + incr > tcora[nr])
	tcsr[nr] |= TCSR_CMFA;
    if (tcnt[nr] <= tcorb[nr] && tcnt[nr] + incr > tcorb[nr])
	tcsr[nr] |= TCSR_CMFB;

    if (tcora[nr] <= tcorb[nr]) {
	first = tcora[nr];
	fshift = 0;
	second = tcorb[nr];
    } else {
	first = tcorb[nr];
	fshift = 2;
	second = tcora[nr];
    }
	    
    cnt = tcnt[nr];
    if (cnt <= first && cnt + incr > first) {
	if (nr == 0)
	    sound_update(out[nr], (first - cnt + 1) << shift);
	switch ((tcsr[nr] >> fshift) & 0x3) {
	case 1:
	    out[nr] = 0;
	    break;
	case 2:
	    out[nr] = 1;
	    break;
	case 3:
	    out[nr] = !out[nr];
	    break;
	}
	incr -= first - cnt + 1;
	cnt = first;
    }

    if (cnt <= second && cnt + incr > second) {
	if (nr == 0)
	    sound_update(out[nr], (second - cnt + 1) << shift);
	switch ((tcsr[nr] >> (2-fshift)) & 0x3) {
	case 1:
	    out[nr] = 0;
	    break;
	case 2:
	    out[nr] = 1;
	    break;
	case 3:
	    out[nr] = !out[nr];
	    break;
	}
	incr -= second - cnt + 1;
	cnt = second;
    }

    if (incr && nr == 0)
	sound_update(out[nr], incr << shift);
}

static void t8_update_time() {
    int nr;
#ifdef DEBUG_TIMER
    printf("t8_update_time cnt: %02x/%02x incr: %04x/%04x tcsr: %02x/%02x", 
	   tcnt0, tcnt1, incr0, incr1, tcsr0, tcsr1);
#endif

    for (nr= 0; nr < 1; nr++) {
	int incr, shift;
	shift = freq[(tcr[nr] & 7) | ((stcr & (1+nr)) << (3-nr))];

	if (shift == 31) {
	    if (nr == 0)
		sound_update(out[nr], cycles - last_cycles[nr]);
	    last_cycles[nr] = cycles;
	    continue;
	}

 	if ((int32)(cycles - my_next_cycle[nr]) < 0)
 	    continue;

	incr = (cycles - last_cycles[nr]) >> shift;
	last_cycles[nr] += incr << shift;
	    
	//printf("Update time %d for %d\n", nr, incr);

	while (incr) {
	    if ((tcr[nr] & (TCR_CCLR1|TCR_CCLR0)) == TCR_CCLR0) {
		if (tcnt[nr] <= tcora[nr] && tcnt[nr] + incr > tcora[nr]) {
		    t8_incr_tcnt(nr, tcora[nr] + 1 - tcnt[nr], shift);
		    incr -= tcora[nr] + 1 - tcnt[nr];
		    tcnt[nr] = 0;
		    continue;
		}
	    } else if ((tcr[nr] & (TCR_CCLR1|TCR_CCLR0)) == TCR_CCLR1) {
		if (tcnt[nr] <= tcorb[nr] && tcnt[nr] + incr > tcorb[nr]) {
		    t8_incr_tcnt(nr, tcorb[nr] + 1 - tcnt[nr], shift);
		    incr -= tcorb[nr] + 1 - tcnt[nr];
		    tcnt[nr] = 0;
		    continue;
		}
	    }

	    if (tcnt[nr] + incr > 0xff) {
		tcsr[nr] |= TCSR_OVF;
		t8_incr_tcnt(nr, 0x100 - tcnt[nr], shift);
		incr -= 0x100 - tcnt[nr];
		tcnt[nr] = 0;
		continue;
	    }
	    
	    t8_incr_tcnt(nr, incr, shift);
	    tcnt[nr] += incr;
	    break;
	}	    
    }

#ifdef DEBUG_TIMER
    printf(" --> frc: %04x tcsr: %02x\n", frc, tcsr);
#endif
    t8_check_next_cycle();
}

static void t8_set_last_cycles(int nr) {
    if (nr == 0)
	sound_update(out[nr], cycles - last_cycles[nr]);
    last_cycles[nr] = cycles;
}


static int t8_check_irq() {
    int i;
    int irqs = (tcsr[0] & tcr[0] & 0xe0) | ((tcsr[1] & tcr[1] & 0xe0) >> 3);
    for (i = 0; i < 6; i++) {
	if (irqs & (1 << (7-i)))
	    return 19 + i;
    }
    return 255;
}


static void set_TCR0(uint8 val) {
    my_next_cycle[0] = cycles;
    t8_update_time();
    t8_set_last_cycles(0);
#ifdef VERBOSE_TIMER8
    if (tcr[0] != val)
        printf("%10ld: %04x: set TCR0 to %02x\n", cycles,pc, val);
#endif
    tcr[0] = val;
    t8_check_next_cycle();
}
static uint8 get_TCR0(void) {
    return tcr[0];
}
static void set_TCSR0(uint8 val) {
    my_next_cycle[0] = cycles;
    t8_update_time();
    val &= (tcsr[0]|0x0f);
    val |= 0x10;
#ifdef VERBOSE_TIMER8
    printf("set TCSR0 to %d\n", val);
#endif
    tcsr[0] = val;
    t8_check_next_cycle();
}
static uint8 get_TCSR0(void) {
    my_next_cycle[0] = cycles;
    t8_update_time();
    return tcsr[0];
}
static void set_TCNT0(uint8 val) {
    my_next_cycle[0] = cycles;
    t8_update_time();
#ifdef VERBOSE_TIMER8
    printf("set TCNT0 to %d\n", val);
#endif
    tcnt[0] = val;
    t8_check_next_cycle();
}
static uint8 get_TCNT0(void) {
    return tcnt[0];
}
static void set_TCORA0(uint8 val) {
    my_next_cycle[0] = cycles;
    t8_update_time();
#ifdef VERBOSE_TIMER8
    printf("set TCORA0 to %d\n", val);
#endif
    tcora[0] = val;
    t8_check_next_cycle();
}
static uint8 get_TCORA0(void) {
    return tcora[0];
}
static void set_TCORB0(uint8 val) {
    my_next_cycle[0] = cycles;
    t8_update_time();
    tcorb[0] = val;
    t8_check_next_cycle();
}
static uint8 get_TCORB0(void) {
    return tcorb[0];
}



static void set_TCR1(uint8 val) {
    my_next_cycle[1] = cycles;
    t8_update_time();
    t8_set_last_cycles(1);
    tcr[1] = val;
    t8_check_next_cycle();
}
static uint8 get_TCR1(void) {
    return tcr[1];
}
static void set_TCSR1(uint8 val) {
    my_next_cycle[1] = cycles;
    t8_update_time();
    val &= val & tcsr[1];
    val |= 0x10;
    tcsr[1] = val;
    t8_check_next_cycle();
}
static uint8 get_TCSR1(void) {
    my_next_cycle[1] = cycles;
    t8_update_time();
    return tcsr[1];
}
static void set_TCNT1(uint8 val) {
    my_next_cycle[1] = cycles;
    t8_update_time();
    tcnt[1] = val;
    t8_check_next_cycle();
}
static uint8 get_TCNT1(void) {
    return tcnt[1];
}
static void set_TCORA1(uint8 val) {
    my_next_cycle[1] = cycles;
    t8_update_time();
    tcora[1] = val;
    t8_check_next_cycle();
}
static uint8 get_TCORA1(void) {
    return tcora[1];
}
static void set_TCORB1(uint8 val) {
    my_next_cycle[1] = cycles;
    t8_update_time();
    tcorb[1] = val;
    t8_check_next_cycle();
}
static uint8 get_TCORB1(void) {
    return tcorb[1];
}


static void set_STCR(uint8 val) {
    if (!((stcr ^ val) & 0x3))
	return;
    my_next_cycle[0] = cycles;
    my_next_cycle[1] = cycles;
    t8_update_time();
    t8_set_last_cycles(0);
    t8_set_last_cycles(1);
    stcr = val | 0xf8;
#ifdef VERBOSE_TIMER8
    printf("%10ld: %04x: set STCR to %d\n", cycles,pc, stcr);
#endif
    t8_check_next_cycle();
}
static uint8 get_STCR(void) {
    return stcr;
}


peripheral_ops timer8 = {
    id: '8',
    reset: t8_reset,
    update_time: t8_update_time,
    check_irq: t8_check_irq,
    save_data: t8_save,
    load_data: t8_load
};

void t8_init() {
    port[0xc8-0x88].set = set_TCR0;
    port[0xc8-0x88].get = get_TCR0;
    port[0xc9-0x88].set = set_TCSR0;
    port[0xc9-0x88].get = get_TCSR0;
    port[0xca-0x88].set = set_TCORA0;
    port[0xca-0x88].get = get_TCORA0;
    port[0xcb-0x88].set = set_TCORB0;
    port[0xcb-0x88].get = get_TCORB0;
    port[0xcc-0x88].set = set_TCNT0;
    port[0xcc-0x88].get = get_TCNT0;
    port[0xd0-0x88].set = set_TCR1;
    port[0xd0-0x88].get = get_TCR1;
    port[0xd1-0x88].set = set_TCSR1;
    port[0xd1-0x88].get = get_TCSR1;
    port[0xd2-0x88].set = set_TCORA1;
    port[0xd2-0x88].get = get_TCORA1;
    port[0xd3-0x88].set = set_TCORB1;
    port[0xd3-0x88].get = get_TCORB1;
    port[0xd4-0x88].set = set_TCNT1;
    port[0xd4-0x88].get = get_TCNT1;
    port[0xc3-0x88].set = set_STCR;
    port[0xc3-0x88].get = get_STCR;

    register_peripheral(timer8);
}
