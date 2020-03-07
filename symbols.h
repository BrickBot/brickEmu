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
 * $Id: symbols.h 240 2006-02-23 19:19:23Z hoenicke $
 */

#ifndef _SYMBOLS_H_
#define  _SYMBOLS_H_

#include "types.h"

typedef void (*symbols_iterate_func) (void *info, 
				      uint16 addr, int16 type, char *name);

extern void symbols_add(uint16 addr, int16 type, char* name);
extern char *symbols_get(uint16 addr, int16 type);
extern uint16 symbols_getaddr(char *name);
extern int symbols_remove(uint16 addr, int16 type);
extern void symbols_removeall(void);
extern void symbols_iterate(symbols_iterate_func f, void *info);


#endif
