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
 * $Id: lx.h 21 2003-09-18 10:03:13Z hoenicke $
 */

#ifndef _LX_H_
#define  _LX_H_

typedef struct lx_header {
  unsigned short version;     //!< version number
  unsigned short base;        //!< current text segment base address
  unsigned short text_size;   //!< size of read-only segment 
  unsigned short data_size;   //!< size of initialized data segment
  unsigned short bss_size;    //!< size of uninitialized data segment
  unsigned short stack_size;  //!< stack size
  unsigned short offset;      //!< start offset from text
  unsigned short num_relocs;  //!< number of relocations.
} lx_header;


int lx_init(FILE *file, lx_header *header);
int lx_read(FILE *file, lx_header *header, int start);

#endif
