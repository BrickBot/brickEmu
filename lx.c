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
 * $Id: lx.c 87 2004-05-13 11:56:36Z hoenicke $
 */

#include <stdio.h>
#include <netinet/in.h> /* for ntohs/ntohl */
#include <string.h>
#include "types.h"
#include "memory.h"
#include "lx.h"


int lx_read (FILE *file, lx_header *header, int start)
{
    uint16 reloc;
    uint16 val;
    int i;

    fseek(file, 8 + sizeof(lx_header), SEEK_SET);

    fread(memory + start, 1, header->text_size + header->data_size, file);
    
    for (i = 0; i < header->num_relocs; i++) {
        fread(&reloc, 2, 1, file);
        reloc = ntohs(reloc);
        val = (memory[start + reloc] << 8) | memory[start + reloc + 1];
        val += start - header->base;
        memory[start + reloc] = (val >> 8);
        memory[start + reloc + 1] = (val & 0xff);
    }
    return 1;
}


int lx_init (FILE *file, lx_header *header)
{
    uint8 signature[8];
    fseek(file, 0, SEEK_SET);
    fread(&signature, sizeof(signature), 1, file);
    if (strcmp(signature, "brickOS"))
        return 0;
    fread(header, sizeof(lx_header), 1, file);
    header->version    = ntohs(header->version);
    header->base       = ntohs(header->base);
    header->text_size  = ntohs(header->text_size);
    header->data_size  = ntohs(header->data_size);
    header->bss_size   = ntohs(header->bss_size);
    header->stack_size = ntohs(header->stack_size);
    header->offset     = ntohs(header->offset);
    header->num_relocs = ntohs(header->num_relocs);
    
    if (header->version != 0)
        return 0;

    return 1;
}

