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
 * $Id: memory.c 321 2006-03-17 13:36:22Z hoenicke $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "h8300.h"
#include "memory.h"

/** \file memory.c
 * \brief memory data structures and routines
 *
 * Contains memory data structures,
 * routines for accessing and
 * initialization of memory. 
 */

/* #define DEBUG_MEM */

#if defined(H8_3292)
#define ROM_END        0x4000
#define ON_CHIP_START  0xfb80
#define FAST_MEM_START 0xfd80
#elif defined(H8_3294)
#define ROM_END        0x8000
#define ON_CHIP_START  0xfb80
#define FAST_MEM_START 0xfb80
#else
#define ROM_END        0x8000
#define ON_CHIP_START  0xf780
#define FAST_MEM_START 0xf780
#endif


/** \brief 64 KB array to represent the memory
 *
 * The whole memory of the brick is addressed
 * as byte array. 
 */
uint8  memory[65536];
/** \brief 64 KB array to describe different memory types
 *
 * The memtype array is used to handle different memory types.
 * For each byte of brick memory, identified by it's array index, 
 * there exists a corresponding index entry in the memtype array describing 
 * its type. The memtype entries are filled according to the memory map.  
 */
uint8  memtype[65536];
register_funcs port[0x10000 - 0xff88];
int wait_states;

char *rom_file_name = NULL;



/** \brief get the byte at the given address
 * \param addr: 2-Byte word
 * \returns byte
 */
uint8 get_byte_div(uint16 addr) {
#ifdef DEBUG_MEM
    printf ("reading byte at %04x: %02x\n", addr & 0xffff, memory[addr]);
#endif
    if (addr > 0xff88) {
        get_byte_func gb = port[addr - 0xff88].get;
        if (gb)
            return (memory[addr] = gb());
    } else {
        printf ("Accessing illegal memory at %04x\n", addr);
        db_trap = 10;
        return 0;
    }
    return memory[addr];
}

/* we want optimize word access */
/** \brief get the word at the given address
 * \param addr: 2-Byte word
 * \returns 2-Byte word
 */

uint16 get_word_div(uint16 addr) {
#ifdef DEBUG_MEM
    printf ("reading word at %04x: %02x  from %04x\n", 
            addr & 0xffff, (memory[addr]<< 8 | memory[addr+1]), pc);
#endif

    if (addr & 1) {
        printf("Unaligned word access!\n");
        db_trap = 10;
    } else if (addr > 0xff88) {
        get_byte_func gb = port[addr - 0xff88].get;
        if (gb) memory[addr] = gb();
        gb = port[addr + 1 - 0xff88].get;
        if (gb) memory[addr+1] = gb();
    } else {
        printf ("Accessing illegal memory at %04x\n", addr);
        db_trap = 10;
    }
    return (memory[addr] << 8) | memory[addr + 1];
}

/** \brief set the byte at the given address
 * \param addr: 2-Byte word
 * \param val: byte 
 */
void SET_BYTE(uint16 addr, uint8 val) {
    int type = memtype[addr];
#ifdef DEBUG_MEM
    printf ("writing %04x: %02x <- %02x\n", addr & 0xffff, memory[addr], val);
#endif
    if ((type & MEMTYPE_MOTOR)) {
        set_motor(val);
    } else if ((type & MEMTYPE_DIV)) {
        if (addr > 0xff88) {
            set_byte_func sb = port[addr - 0xff88].set;
            if (sb)
                sb(val);
        } else {
            printf ("Accessing illegal memory at %04x\n", addr);
            db_trap = 10;
        }
    }
    memory[addr] = val;
}
/** \brief set word values starting at the given address
 * \param addr: 2-Byte word
 * \param val: 2-Byte word 
 */
void SET_WORD(uint16 addr, uint16 val) {
    int type = memtype[addr];
#ifdef DEBUG_MEM
    printf ("writing %04x: %04x <- %04x\n", addr & 0xffff, 
            memory[addr]<<8 | memory[addr+1], val);
#endif
    memory[addr] = val >> 8;
    memory[addr+1] = val & 0xff;
    if ((type & MEMTYPE_MOTOR)) {
        set_motor(val);
    } else if ((type & MEMTYPE_DIV)) {
        if (addr > 0xff88) {
            set_byte_func sb = port[addr - 0xff88].set;
            if (sb)
                sb(val >> 8);
            sb = port[addr+1 - 0xff88].set;
            if (sb)
                sb(val & 0xff);
        } else {
            printf ("Accessing illegal memory at %04x\n", addr);
            db_trap = 10;
        }
    }
}


/** \brief read in the ROM
 *
 * Reads in the ROM file.
 * If the environment variable "BRICKEMU_DIR" is
 * set, the file is searched for in the specified
 * directory, otherwise in the current directory.
 * The file must be in "bin" or "srec" format and
 * must be named "rom.bin" or "rom.srec".
 * \return 1 on success, 0 otherwise.
 */
int read_rom() {
    int result = 0;
    char *rom_file_ext = NULL;
    FILE *romfile;

    if (rom_file_name && (rom_file_ext = strrchr(rom_file_name, '.'))) {

        if ((strcmp(rom_file_ext, ".coff") == 0) && (romfile = fopen(rom_file_name, "rb"))) {
            if (coff_init(romfile)) {
                coff_read(romfile, 0);
                coff_symbols(romfile, 0);
                result = 1;
            }
        } else if ((strcmp(rom_file_ext, ".bin") == 0) && (romfile = fopen(rom_file_name, "rb"))) {
            fread(memory, 0x4000, 1, romfile);
            result = 1;
        } else if ((strcmp(rom_file_ext, ".srec") == 0) && (romfile = fopen(rom_file_name, "r"))) {
            if (srec_read(romfile, 0) >= 0) {
                result = 1;
            }
        }

        if (romfile) {
            fclose(romfile);
        }
    }

    return result;
}

/** \brief initialize brick memory 
 * Reads in the ROM file.
 * If the environment variable "BRICKEMU_DIR" is
 * set, the file is searched for in the specified
 * directory, otherwise in the current directory.
 * The file must be in "bin" or "srec" format and
 * must be named "rom.bin" or "rom.srec".
 * \return 1 on success, 0 otherwise.
 */
void mem_init(char *rom_file) {
    int i;

    rom_file_name = rom_file;
    if (!read_rom()) {
        fprintf(stderr, "Please provide a ROM image (coff, binary, or srec format).\n");
        fprintf(stderr, "If extracting the ROM image, save it to a rom.bin or rom.srec file.\n");
        fflush(stderr);
        abort();
    }
    
    i = 0;
    while(i < ROM_END)
        memtype[i++] = MEMTYPE_FAST | MEMTYPE_WRITETRAP;
    while(i < 0x8000)
        memtype[i++] = MEMTYPE_DIV;
    while(i < 0xf000)
        memtype[i++] = 0;
    while(i < ON_CHIP_START)
        memtype[i++] = MEMTYPE_MOTOR;
    while(i < FAST_MEM_START)
        memtype[i++] = MEMTYPE_DIV;
    while(i < 0xff80)
        memtype[i++] = MEMTYPE_FAST;
    while(i < 0xff88)
        memtype[i++] = MEMTYPE_MOTOR;
    while(i < 0x10000)
        memtype[i++] = MEMTYPE_DIV;

#if 0  /* Code to debug specific instructions */
    for (i = 0x9e8a ; i < 0x9eca; i+=2)
        memtype[i] |= MEMTYPE_LOG;
    for (i = 0 ; i < 0x10000; i+=2) {
        if (memory[i] == 0x19 && memory[i+1] != 0x66
            && (memory[i-2] != 0x5a && memory[i-1] != 0)) {
            memtype[i] |= MEMTYPE_LOG;
            memtype[i+2] |= MEMTYPE_LOG;
        }
    }
#endif
}
