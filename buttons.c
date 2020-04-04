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
 * $Id: buttons.c 103 2005-02-04 18:55:13Z hoenicke $
 */

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"
#include <unistd.h>
#include <fcntl.h>

#define POLLING_CYCLES 16000

#define BUTTON_RUN   0x01
#define BUTTON_ONOFF 0x02
#define BUTTON_VIEW  0x40
#define BUTTON_PRGM  0x80


static uint8 btn_state, iscr, ier, irqpending;

typedef struct {
    uint8 btn_state, iscr, ier, irqpending;
} btn_save_type;

static void btn_reset() {
    iscr = ier = irqpending = 0;
    btn_state = 0xff;
}

static void btn_check_next_cycle() {
    if ((~btn_state & ier & ~iscr & 7) | (irqpending & iscr))
        next_timer_cycle = cycles;
}

static int btn_save(void *buffer, int maxlen) {
    btn_save_type *data = buffer;
    data->iscr = iscr;
    data->ier = ier;
    data->irqpending = irqpending;
    data->btn_state = btn_state;
    return sizeof(btn_save_type);
}

static void btn_load(void *buffer, int len) {
    btn_save_type *data = buffer;
    iscr = data->iscr;
    ier = data->ier;
    irqpending = data->irqpending;
    irqpending |= data->btn_state & ~btn_state & iscr & 7;
    btn_check_next_cycle();
}

static void btn_read_fd(int fd) {
    char buf[3];
    int mask, newval;

    /* read in 3 bytes: btnid val newline */
       int len = 0;
    do {
        len += read(fd, buf + len, 3 - len);
    } while (len < 3);

    mask = 0;
    switch (buf[0]) {
    case 'O':
    case 'o':
        mask = BUTTON_ONOFF;
        break;
    case 'P':
    case 'p':
        mask = BUTTON_PRGM;
        break;
    case 'R':
    case 'r':
        mask = BUTTON_RUN;
        break;
    case 'V':
    case 'v':
        mask = BUTTON_VIEW;
        break;
    }
    newval = (buf[1] == '1' ? 0 : 0xff);
    
    if ((btn_state ^ newval) & mask) {
        irqpending |= btn_state & mask & iscr & 7;
        btn_state ^= mask;
#ifdef VERBOSE_BUTTON
        printf("%10ld: BTN %02x\n", cycles, btn_state);
#endif
        btn_check_next_cycle();
    }
}

static int btn_check_irq() {
    int i;
    int irqs;

    irqs = (~btn_state & ier & ~iscr & 7) | irqpending;

    for (i = 0; i < 3; i++) {
        if (irqs & (1 << i)) {
            irqpending &= ~(1 << i);
#ifdef VERBOSE_BUTTON
            printf("Generated IRQ%d\n", i);
#endif
            return 4 + i;
        }
    }
    return 255;
}

static void set_iscr(uint8 value) {
#ifdef VERBOSE_BUTTON
    printf("buttons.c: set_iscr(%02x)\n", value);
#endif
    iscr = value & 0x7;
    irqpending &= iscr;
    btn_check_next_cycle();
}
static uint8 get_iscr(void) {
    return ier | 0xf8;
}
static void set_ier(uint8 value) {
#ifdef VERBOSE_BUTTON
    printf("buttons.c: set_ier(%02x)\n", value);
#endif
    ier = value & 0x7;
    btn_check_next_cycle();
}
static uint8 get_ier(void) {
    return ier | 0xf8;
}
static uint8 get_port4(void) {
    /* port 4 irq0-2 are somewhat swapped */
    return 1 | (btn_state & 2) | ((btn_state & 1) << 2);
}
static uint8 get_port7(void) {
    /* XXX handle analog input? */
    return btn_state & 0xc0;
}


peripheral_ops buttons = {
    id: 'B',
    reset: btn_reset,
    read_fd: btn_read_fd,
    check_irq: btn_check_irq,
    load_data: btn_load,
    save_data: btn_save
};

void btn_init() {
    port[0xb7 - 0x88].get = get_port4;
    port[0xbe - 0x88].get = get_port7;

    port[0xc6 - 0x88].get = get_iscr;
    port[0xc6 - 0x88].set = set_iscr;
    port[0xc7 - 0x88].get = get_ier;
    port[0xc7 - 0x88].set = set_ier;

    register_peripheral(buttons);
}
