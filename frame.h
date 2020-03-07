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
 * $Id: frame.h 266 2006-02-26 17:30:02Z hoenicke $
 */

#ifndef _FRAME_H_
#define  _FRAME_H_

#include "types.h"

extern void frame_init(void);
extern void frame_dump_stack(FILE *out, uint16 fp);
extern void frame_switch(uint16 oldframe, uint16 newframe);
extern void frame_begin(uint16 fp, int in_irq);
extern void frame_end(uint16 fp, int in_irq);
extern void frame_dump_profile(void);

#endif
