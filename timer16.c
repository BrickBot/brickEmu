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
 * $Id: timer16.c 107 2005-02-04 20:00:50Z hoenicke $
 */

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"

#include <netinet/in.h> /* for htonx/ntohx */

//#define VERBOSE_TIMER
//#define DEBUG_TIMER

#define TIER_ADDR 0xff90
#define TCSR_ADDR 0xff91
#define FRCH_ADDR 0xff92
#define FRCL_ADDR 0xff93
#define OCRH_ADDR 0xff94
#define OCRL_ADDR 0xff95
#define TCR_ADDR  0xff96
#define TOCR_ADDR 0xff97
#define ICRAH_ADDR 0xff98
#define ICRAL_ADDR 0xff99

#define TCSR_ICFA  0x80
#define TCSR_ICFB  0x40
#define TCSR_ICFC  0x20
#define TCSR_ICFD  0x10
#define TCSR_OCFA  0x08
#define TCSR_OCFB  0x04
#define TCSR_OVF   0x02
#define TCSR_CCLRA 0x01

#define TOCR_OCRS  0x10
#define TOCR_OEA   0x08
#define TOCR_OEB   0x04
#define TOCR_OLVLA 0x02
#define TOCR_OLVLB 0x01

static uint16 frc, ocra, ocrb;
static uint8  tcsr, tier, tcr, tocr;
static uint8  temp;
static cycle_count_t my_last_cycles;

static uint8 freq[4] = { 1, 3, 5, /*XXXX check value*/ 1 };

/* Hook to add ftoa/b listeners */
#define SET_FTOA(v) do {} while(0)
#define SET_FTOB(v) do {} while(0)

typedef struct {
    cycle_count_t my_last_cycles;
    uint16 frc, ocra, ocrb;
    uint8  tcsr, tier, tcr, tocr;
    uint8  temp;
} t16_save_type;

static int t16_save(void *buffer, int maxlen) {
    t16_save_type *data = buffer;
    data->my_last_cycles = htonl(my_last_cycles);
    data->frc = htons(frc);
    data->ocra = htons(ocra);
    data->ocrb = htons(ocrb);
    data->tcsr = tcsr;
    data->tier = tier;
    data->tcr  = tcr;
    data->tocr = tocr;
    return sizeof(t16_save_type);
}

static void t16_load(void *buffer, int len) {
    t16_save_type *data = buffer;
    my_last_cycles = ntohl(data->my_last_cycles);
    frc = ntohs(data->frc);
    ocra = ntohs(data->ocra);
    ocrb = ntohs(data->ocrb);
    tcsr = data->tcsr;
    tier = data->tier;
    tcr  = data->tcr;
    tocr = data->tocr;
}

static void t16_reset() {
    frc = 0;
    ocra = 0xffff;
    ocrb = 0xffff;
    tier = 1;
    tcsr = 0;
    tcr  = 0;
    tocr = 0xe0;
    SET_FTOA(0);
    SET_FTOB(0);
    my_last_cycles = cycles;
}

static void t16_check_next_cycle() {
    int32 next_cycle;
    int next = 0x10000 - frc;

    if ((tier & tcsr & 0xfe)) {
        /* interrupt is pending */
        next_timer_cycle = cycles;
        return;
    }

#ifdef DEBUG_TIMER
    printf("%" CYCLE_COUNT_F ": t16_check_next_cycle: %02x %02x %04x %04x", cycles, tier, tcsr, frc, ocra);
#endif
    if ((tier & TCSR_OCFA)) {
        int nexta = ((uint16) (ocra + 1 - frc));
        if (nexta < next)
            next = nexta;
    }
    if ((tier & TCSR_OCFB)) {
        int nextb = ((uint16) (ocrb + 1 - frc));
        if (nextb < next)
            next = nextb;
    }
    next_cycle = (int32)((next << freq[tcr & 3]) + my_last_cycles - cycles);
#ifdef DEBUG_TIMER
    printf(" --> %d  (%d)\n", next_cycle, (int32)(next_timer_cycle - cycles));
#endif
    if (next_cycle < (int32) (next_timer_cycle - cycles)) {
        next_timer_cycle = add_to_cycle('N', cycles, next_cycle);
    }
}

static void t16_update_time() {
    int32  increment = ((int32)(cycles - my_last_cycles)) >> freq[tcr & 3];
    uint32 newfrc = frc + increment;
#ifdef DEBUG_TIMER
    printf("t16_update_time frc: %04x incr: %04x tcsr: %02x", frc, increment, tcsr);
#endif

 repeat_cycle:
    if (frc <= ocra && newfrc > ocra) {
        tcsr |= TCSR_OCFA;
        if (tocr & TOCR_OEA) {
            SET_FTOA( (tocr & TOCR_OLVLA) ? 1 : 0 );
        }
        if (tcsr & TCSR_CCLRA) {
            if (frc <= ocrb && ocrb <= ocra)
                tcsr |= TCSR_OCFB;
            newfrc -= ocra + 1;
            frc = 0;
            goto repeat_cycle;
        }
    }
    if (frc <= ocrb && newfrc > ocrb) {
        tcsr |= TCSR_OCFB;
        if (tocr & TOCR_OEB) {
            SET_FTOB( (tocr & TOCR_OLVLB) ? 1 : 0 );
        }
    }
    if (newfrc > 0xffff) {
        tcsr |= TCSR_OVF;
        frc = 0;
        newfrc -= 0x10000;
        goto repeat_cycle;
    }
    frc = newfrc;
#ifdef DEBUG_TIMER
    printf(" --> frc: %04x tcsr: %02x\n", frc, tcsr);
#endif
    my_last_cycles = add_to_cycle('Y', my_last_cycles, increment << freq[tcr & 3]);
    t16_check_next_cycle();
}

static int t16_check_irq() {
    int i;
    int irqs = tier & tcsr & 0xfe;
    for (i = 0; i < 7; i++) {
        if (irqs & (1 << (7-i)))
            return 12 + i;
    }
    return 255;
}


static void set_TIER(uint8 val) {
#ifdef VERBOSE_TIMER
    printf("set_TIER(%02x)\n", val);
#endif
    tier = val | 1;
    t16_check_next_cycle();
}

static uint8 get_TIER(void) {
    return tier;
}

static void set_TCSR(uint8 val) {
#ifdef VERBOSE_TIMER
    printf("set_TCSR(%02x)\n", val);
#endif
    tcsr = (tcsr & val) | (val&1);
    t16_check_next_cycle();
}

static uint8 get_TCSR(void) {
    return tcsr;
}

static void set_FRCH(uint8 val) {
    temp = val;
}

static void set_FRCL(uint8 val) {
#ifdef VERBOSE_TIMER
    printf("set_FRC(%04x)\n", temp << 8 | val);
#endif
    frc = temp << 8 | val;
}

static uint8 get_FRCH(void) {
    temp = frc & 0xff;
    return frc >> 8;
}

static uint8 get_FRCL(void) {
    return temp;
}

static void set_OCRH(uint8 val) {
    temp = val;
}

static void set_OCRL(uint8 val) {
#ifdef VERBOSE_TIMER
    printf("set_OCR(%04x)\n", temp << 8 | val);
#endif
    if (tocr & TOCR_OCRS)
        ocrb = temp << 8 | val;
    else
        ocra = temp << 8 | val;
}

static uint8 get_OCRH(void) {
    if (tocr & TOCR_OCRS)
        return ocrb >> 8;
    else
        return ocra >> 8;
}

static uint8 get_OCRL(void) {
    if (tocr & TOCR_OCRS)
        return ocrb & 0xff;
    else
        return ocra & 0xff;
}

static void set_TCR(uint8 val) {
    wait_peripherals();
#ifdef VERBOSE_TIMER
    printf("set_TCR(%02x)\n", val);
#endif
    tcr = val;
}

static uint8 get_TCR(void) {
    return tcr;
}

static void set_TOCR(uint8 val) {
#ifdef VERBOSE_TIMER
    printf("set_TOCR(%02x)\n", val);
#endif
    tocr = val | 0xe0;
}

static uint8 get_TOCR(void) {
    return tocr;
}

static uint8 get_ICRAH(void) {
    return 0;
}

static uint8 get_ICRAL(void) {
    return 0;
}


peripheral_ops timer16 = {
    id: 'T',
    reset: t16_reset,
    update_time: t16_update_time,
    check_irq: t16_check_irq,
    save_data: t16_save,
    load_data: t16_load
};

void t16_init() {
    port[0x90-0x88].set = set_TIER;
    port[0x90-0x88].get = get_TIER;
    port[0x91-0x88].set = set_TCSR;
    port[0x91-0x88].get = get_TCSR;
    port[0x92-0x88].set = set_FRCH;
    port[0x92-0x88].get = get_FRCH;
    port[0x93-0x88].set = set_FRCL;
    port[0x93-0x88].get = get_FRCL;
    port[0x94-0x88].set = set_OCRH;
    port[0x94-0x88].get = get_OCRH;
    port[0x95-0x88].set = set_OCRL;
    port[0x95-0x88].get = get_OCRL;
    port[0x96-0x88].set = set_TCR;
    port[0x96-0x88].get = get_TCR;
    port[0x97-0x88].set = set_TOCR;
    port[0x97-0x88].get = get_TOCR;
    port[0x98-0x88].get = get_ICRAH;
    port[0x99-0x88].get = get_ICRAL;

    register_peripheral(timer16);
}
