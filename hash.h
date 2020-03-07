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
 * $Id: hash.h 266 2006-02-26 17:30:02Z hoenicke $
 */

#ifndef _HASH_H_
#define  _HASH_H_

typedef struct hash_type {
    unsigned int size, elems;
    struct hash_bucket **list;
} hash_type;

extern void hash_init(hash_type *hash, int initsize);
extern void *hash_get(hash_type *hash, unsigned int key);
extern void *hash_move(hash_type *hash, unsigned int key, unsigned int newkey);
extern void *hash_create(hash_type *hash, unsigned int key, size_t elemsize);
extern void *hash_realloc(hash_type *hash, unsigned int key, size_t elemsize);
extern int hash_remove(hash_type *hash, unsigned int key);
extern void hash_enumerate(hash_type *hash, 
			   void (*func) (unsigned int key, void *data));

#endif
