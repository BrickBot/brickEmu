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
 * $Id: waitstate.c 83 2004-05-13 11:52:49Z hoenicke $
 */

#include <stdio.h>
#include "h8300.h"
#include "memory.h"

#define PIN_WAIT_STATES 0  /*XXX check it out */

uint16 wcsr;

static void set_WCSR(uint8 val) {
    wcsr = val;
    switch (val & 0x0c) {
    case 0:
        wait_states = val & 3;
        break;
    case 4:
        wait_states = 0;
        break;
    case 8:
        /* XXX Can't handle these correctly yet. */
        wait_states = PIN_WAIT_STATES;
        break;
    case 12:
        /* XXX Can't handle these correctly yet. */
        wait_states = (val & 3);
        break;
    }
#ifdef VERBOSE_WAITSTATE
    printf("set_WCSR(%02x):  %d\n",val, wait_states);
#endif
}
static uint8 get_WCSR(void) {
    return wcsr;
}

void ws_init() {
    port[0xc2-0x88].set = set_WCSR;
    port[0xc2-0x88].get = get_WCSR;
    set_WCSR(8);
}
