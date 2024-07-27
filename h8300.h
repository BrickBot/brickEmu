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
 * $Id: h8300.h 309 2006-03-05 19:05:36Z hoenicke $
 */

#ifndef _H8300_H_
#define  _H8300_H_

#include "types.h"


#define NMI_HANDLER   3
#define IRQ0_HANDLER  4
#define IRQ1_HANDLER  5
#define IRQ2_HANDLER  6
#define ICIA_HANDLER  12
#define ICIB_HANDLER  13
#define ICIC_HANDLER  14
#define ICID_HANDLER  15
#define OCIA_HANDLER  16
#define OCIB_HANDLER  17
#define FOVI_HANDLER  18
#define CMI0A_HANDLER 19
#define CMI0B_HANDLER 20
#define OVI0_HANDLER  21
#define CMI1A_HANDLER 22
#define CMI1B_HANDLER 23
#define OVI1_HANDLER  24
#define ERI_HANDLER   27
#define RXI_HANDLER   28
#define TXI_HANDLER   29
#define TEI_HANDLER   30
#define ADI_HANDLER   35
#define WOVF_HANDLER  36

#define H8_3292

#define GET_REG16(nr)     ((reg[(nr)] << 8) | reg[(nr) + 8])
#define SET_REG16(nr,val) do { reg[(nr)] = (val) >> 8; \
                               reg[(nr) + 8] = (val) & 0xff; } while (0)


extern uint8 reg[16];
extern uint16 pc;
extern uint8 ccr;
extern cycle_count_t cycles, next_timer_cycle, next_nmi_cycle;
extern int irq_disabled_one;
extern volatile int db_trap;
extern int db_singlestep;
extern void dump_state(void);
extern void run_cpu(void);

#endif
