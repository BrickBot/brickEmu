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
 * $Id: types.h 309 2006-03-05 19:05:36Z hoenicke $
 */

/** \file types.h
 * \brief type definitions for the basic data types needed by the emulator
 * In this file the various integer data types needed by the emulator 
 * are mapped to builtin types. For other OS's and compilers these
 * mappings may need to be changed.
 */

#ifndef _BRICK_EMU_TYPES_H_
#define  _BRICK_EMU_TYPES_H_

typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;

// Cycle count main data type
typedef unsigned long long cycle_count_t;

// Cycle count printf format
// - https://stackoverflow.com/a/30221946
// - https://stackoverflow.com/a/8679
// usage: printf("x: %"CYCLE_COUNT_F", y: %"CYCLE_COUNT_F"\n", x, y);
#define CYCLE_COUNT_F "12llu"

// hton and ntoh for 64-bit
// - adapted from https://stackoverflow.com/a/28592202
// - conditions based on https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define hton64(x) (x)
# define ntoh64(x) (x)
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define hton64(x) (((cycle_count_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
# define ntoh64(x) (((cycle_count_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
# error "Detected __ORDER_PDP_ENDIAN__ but neither hton64(x) nor ntoh64(x) are defined for this byte ordering."
#else
// Alternate implementation if byte ordering constants are NOT defined
// While this approach does require a runtime check, hton/ntoh is only used in save/load methods,
// so it should not incur a significant performance impact.
# warning "Neither __BIG_ENDIAN__ nor __LITTLE_ENDIAN__ are built-in defined."
# define hton64(x) ((1==htonl(1)) ? (x) : ((cycle_count_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
# define ntoh64(x) ((1==ntohl(1)) ? (x) : ((cycle_count_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

#endif
