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
 * $Id: watchdog.c 83 2004-05-13 11:52:49Z hoenicke $
 */

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"

#include <netinet/in.h> /* for htonx/ntohx */

//#define DEBUG_WDOG
//#define VERBOSE_WDOG

#define TCSR_OVF   0x80
#define TCSR_WTIT  0x40
#define TCSR_TME   0x20
#define TCSR_RST   0x08
#define TCSR_CKS2  0x04
#define TCSR_CKS1  0x02
#define TCSR_CKS0  0x01

static uint8  tcsr, readtcsr, tcnt, pw;
static int32 last_cycles;

static uint8 freq[8] = { 1, 5, 6, 7, 8, 9, 11, 12 };

/* Hook to add ftoa/b listeners */
#define SET_FTOA(v) do {} while(0)
#define SET_FTOB(v) do {} while(0)

typedef struct {
    int32 last_cycles;
    uint8  tcsr, readtcsr, tcnt, pw;
} wdog_save_type;

static int wdog_save(void *buffer, int maxlen) {
    wdog_save_type *data = buffer;
    data->last_cycles = htonl(last_cycles);
    data->tcsr = tcsr;
    data->readtcsr = readtcsr;
    data->tcnt = tcnt;
    data->pw   = pw;
    return sizeof(wdog_save_type);
}

static void wdog_load(void *buffer, int len) {
    wdog_save_type *data = buffer;
    last_cycles = htonl(data->last_cycles);
    tcsr = data->tcsr;
    readtcsr = data->readtcsr;
    tcnt = data->tcnt;
    pw   = data->pw;
}

static void wdog_reset() {
    tcnt = pw = 0;
    readtcsr = tcsr = 0x10;
    last_cycles = cycles;
}

static void wdog_check_next_cycle() {
    if (tcsr & TCSR_TME) {
        int32 next_cycle = last_cycles + ((0x100 - tcnt) << freq[tcsr & 7]);

        if ((next_cycle - cycles) < (next_nmi_cycle - cycles))
            next_nmi_cycle = next_cycle;
        if ((next_cycle - cycles) < (next_timer_cycle - cycles))
            next_timer_cycle = next_cycle;
    }
}

static void wdog_update_time() {
    if (tcsr & TCSR_TME) {
        int incr = (cycles - last_cycles) >> freq[tcsr & 7];
        if (incr < 0) {
            wdog_check_next_cycle();
            return;
        }
        last_cycles += incr << freq[tcsr & 7];

        //printf("Update time %d for %d\n", nr, incr);
        
        if (tcnt + incr > 0xff) {
            tcsr |= TCSR_OVF;
            if (tcsr & TCSR_WTIT) {
                /* calculate cycle where interrupt was fired and add 518 */
                /* 518 is from the processor spec */
                last_cycles -= ((tcnt + incr - 0x100) << freq[tcsr & 7]);
                last_cycles += 518;
                incr = 0;
                tcnt = 0;
            }
        }
#ifdef DEBUG_WDOG
        printf("wdog_update_time tcnt: %04x incr: %04x tcsr: %02x\n", 
               tcnt, incr, tcsr);
#endif

        tcnt += incr;
        wdog_check_next_cycle();
    }
}

static int wdog_check_irq() {
    if (tcsr & TCSR_OVF) {
        tcsr &= ~TCSR_OVF;
        if (tcsr & TCSR_RST) {
            next_nmi_cycle = last_cycles;
            return 0;
        } else {
            return 3;
        }
    }
    return 255;
}

static void set_pw(uint8 val) {
    pw = val;
}

static void set_val(uint8 val) {
    if (pw == 0xa5) {
        wdog_update_time();
        val |= 0x10;
        val |= (0x80 & ~readtcsr);
        val &= (0x7f | tcsr);
#ifdef VERBOSE_WDOG
        printf("watchdog.c: set_TCSR(%02x)\n", val);
#endif
        tcsr = val;
        last_cycles = cycles;
    } else if (pw == 0x5a) {
        wdog_update_time();
        tcnt = val;
        last_cycles = cycles;
    } else
        return;
    pw = 0;
    wdog_check_next_cycle();
}
static uint8 get_TCSR(void) {
    return readtcsr = tcsr;
}
static uint8 get_TCNT(void) {
    return tcnt;
}


static peripheral_ops watchdog = {
    id: 'W',
    reset: wdog_reset,
    update_time: wdog_update_time,
    check_irq: wdog_check_irq,
    save_data: wdog_save,
    load_data: wdog_load
};

void wdog_init() {
    port[0xA8-0x88].set = set_pw;
    port[0xA8-0x88].get = get_TCSR;
    port[0xA9-0x88].set = set_val;
    port[0xA9-0x88].get = get_TCNT;

    register_peripheral(watchdog);
    wdog_reset();
}
