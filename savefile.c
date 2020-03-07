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
 * $Id: savefile.c 72 2004-04-26 07:25:12Z hoenicke $
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h> /* for htonx/ntohx */
#include <zlib.h>
#include "types.h"
#include "peripherals.h"
#include "memory.h"
#include "h8300.h"
#include "symbols.h"

#define MAX_PATHNAME_LEN 4096
#define SAVEFILE_MAGIC "BrEmuSF0"

/** \brief array to hold all peripherals 
 *
 * This array is used to register the different
 * kinds of peripherals (buttons, sensors etc.)
 */
extern peripheral_ops peripherals[100];

/** \brief number of registered peripherals
 * This counter is incremented each time a
 * peripheral is registered.
 */
extern int num_peripherals;

static void save_symbol(gzFile *file, 
			       uint16 addr, int16 type, char* name)
{
    struct {
	uint16 addr;
	int16  type;
	int16  namelen;
    } sym_entry;

    int namelen = strlen(name);
    
    sym_entry.addr = ntohs(addr);
    sym_entry.type = ntohs(type);
    sym_entry.namelen = ntohs(namelen);
    gzwrite(file, &sym_entry, sizeof(sym_entry));
    gzwrite(file, name, namelen);
}

static void load_symbols(gzFile *file)
{
    struct {
	uint16 addr;
	int16  type;
	int16  namelen;
    } sym_entry;
    char *name;
    
    for (;;) {
	gzread(file, &sym_entry, sizeof(sym_entry));
	if (!sym_entry.addr && !sym_entry.type && !sym_entry.namelen)
	    break;
	sym_entry.namelen = ntohs(sym_entry.namelen);
	name = malloc(sym_entry.namelen + 1);
	gzread(file, name, sym_entry.namelen);
	name[sym_entry.namelen] = 0;
	symbols_add(ntohs(sym_entry.addr), ntohs(sym_entry.type), name);
    }
}

static void savefile_save(char *path) {
    gzFile *file;
    struct {
	char   pad;
	char   id;
	uint16 len;
	char   buffer[512];
    } block;
    int i;
    int len;

    stop_time();
    file = gzopen(path, "wb9");
    gzwrite(file, SAVEFILE_MAGIC, 8);
    gzwrite(file, memory, sizeof(memory));
    gzwrite(file, memtype, sizeof(memtype));

    symbols_iterate((symbols_iterate_func) save_symbol, file);
    save_symbol(file, 0, 0, "");

    for (i = 0; i < num_peripherals; i++) {
	if (!peripherals[i].save_data)
	    continue;
	len = peripherals[i].save_data(block.buffer, sizeof(block.buffer));
	block.id = peripherals[i].id;
	block.len = ntohs(len);
	gzwrite(file, &block.id, len+sizeof(block.len)+sizeof(block.id));
    }
    block.id = 0;
    gzwrite(file, &block.id, sizeof(block.id));
    gzclose(file);
    cont_time();
}


static void savefile_load(char *path) {
    gzFile *file;
    char buffer[512];
    uint16 len;
    int i;

    stop_time();
    file = gzopen(path, "rb");
    gzread(file, buffer, 8);
    if (memcmp(buffer, SAVEFILE_MAGIC, 8) != 0) {
	printf("Wrong file type!\n");
	gzclose(file);
	return;
    }
    gzread(file, memory, sizeof(memory));
    gzread(file, memtype, sizeof(memtype));
    load_symbols(file);

    while (!gzeof(file)) {
	char id;
	gzread(file, &id, 1);
	if (id == 0)
	    break;
	gzread(file, &len, sizeof(len));
	len = ntohs(len);
	if (len > sizeof(buffer)) {
	    printf("Too much info for peripheral %c", id);
	    goto exit;
	}
	gzread(file, &buffer, len);

	for (i = 0; i < num_peripherals; i++) {
	    if (id == peripherals[i].id) {
		peripherals[i].load_data(buffer, len);
		break;
	    }
	}
    }

    frame_init();
    cont_time();
 exit:
    gzclose(file);
    return;
}

static void savefile_read_fd(int fd) {
    char buf[1];
    char filename[MAX_PATHNAME_LEN+1];

    /* read in next byte: id */
    read(fd, buf, 1);

    /* read in filename */
    int len = 0;
    do {
	len += read(fd, filename + len, 1);
    } while (filename[len-1] != '\n' && filename[len-1] != '\r');
    filename[len-1] = 0;


    switch (buf[0]) {
	case 'L':
	case 'l':
	    savefile_load(filename);
	case 'S':
	case 's':
	    savefile_save(filename);
    }
}

static peripheral_ops savefile = {
    id: 'C',
    read_fd: savefile_read_fd
};

void savefile_init() {
    register_peripheral(savefile);
}
