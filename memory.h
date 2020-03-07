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
 * $Id: memory.h 309 2006-03-05 19:05:36Z hoenicke $
 */

#ifndef _MEMORY_H_
#define  _MEMORY_H_

#include "types.h"

#define MEMTYPE_BREAKPOINT 0x01
#define MEMTYPE_LOG        0x02
#define MEMTYPE_WRITETRAP  0x04
#define MEMTYPE_READTRAP   0x08
#define MEMTYPE_MOTOR      0x20
#define MEMTYPE_FAST       0x40
#define MEMTYPE_DIV        0x80

typedef uint8 (*get_byte_func) (void);
typedef void (*set_byte_func) (uint8 val);
typedef struct {
    get_byte_func get;
    set_byte_func set;
} register_funcs;

extern uint8 memory[65536];
extern uint8 memtype[65536];
extern register_funcs port[0x10000 - 0xff88];
extern int wait_states;

extern uint8 get_byte_div(uint16 addr);
extern uint16 get_word_div(uint16 addr);
extern void SET_BYTE(uint16 addr, uint8 val);
extern void SET_WORD(uint16 addr, uint16 val);
extern int read_rom(void);
extern void set_motor(unsigned char val);

#define GET_BYTE(addr) \
    (!(memtype[(uint16)(addr)] & MEMTYPE_DIV) ? \
       memory[(uint16)(addr)] : get_byte_div(addr))

#define GET_WORD(addr) \
    (!((addr) & 1) && !(memtype[(uint16)(addr)] & MEMTYPE_DIV) ? \
      (memory[(uint16)(addr)] << 8) | memory[(uint16) (addr) + 1] \
     : get_word_div(addr))

#define GET_BYTE_CYCLES(addr) \
    (cycles += 2 + ((~memtype[(uint16)(addr)] & MEMTYPE_FAST) >> 6), \
     GET_BYTE(addr))

#define GET_WORD_CYCLES(addr) \
    (cycles += 2 + ((~memtype[(uint16)(addr)] & MEMTYPE_FAST) >> 4), \
     GET_WORD(addr))

#define SET_BYTE_CYCLES(addr, val) \
    (cycles += 2 + ((~memtype[(uint16)(addr)] & MEMTYPE_FAST) >> 6), \
     SET_BYTE(addr, val))

#define SET_WORD_CYCLES(addr, val) \
    (cycles += 2 + ((~memtype[(uint16)(addr)] & MEMTYPE_FAST) >> 4), \
     SET_WORD(addr, val))

#endif
