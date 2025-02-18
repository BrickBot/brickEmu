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
 * $Id: coff.c 85 2004-05-13 11:54:04Z hoenicke $
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <netinet/in.h> /* for ntohs/ntohl */
#include "types.h"
#include "memory.h"
#include "symbols.h"
#include "coff.h"

int coff_read (FILE *file, int start)
{
    coff_header header;
    coff_aouthdr aouthdr;
    coff_section sect;
    int i, numsect;
    fseek(file, 0, SEEK_SET);
    fread(&header, sizeof(coff_header), 1, file);
    fread(&aouthdr, sizeof(aouthdr), 1, file);
    numsect = ntohs(header.nscns);
    for (i = 0; i < numsect; i++) {
        int offset = sizeof(coff_header) + sizeof(coff_aouthdr)
            + i * sizeof(coff_section);
        fseek(file, offset, SEEK_SET);
    
        fread(&sect, sizeof(sect), 1, file);
        if ((ntohl(sect.flags) & (STYP_LOAD))) {
            uint32 paddr = ntohl(sect.paddr);
            uint32 size = ntohl(sect.size);
            uint32 scnptr = ntohl(sect.scnptr);
            fseek(file, scnptr, SEEK_SET);
            fread(memory + paddr, size, 1, file);
#ifdef VERBOSE_COFF
            fprintf(stderr, "section %8s loaded to %04lx-%04lx\n", 
                    sect.sectname, paddr, paddr+size);
#endif
        }
    }
    return ntohl(aouthdr.entry);
}


int coff_init (FILE *file)
{
    coff_header header;
    fread(&header, sizeof(coff_header), 1, file);
    return ntohs(header.magic) == 0x8300 && (ntohs(header.flags) & F_EXEC)
        && ntohs(header.opthdr) == sizeof(coff_aouthdr);
}

void coff_symbols (FILE *file, int start)
{
    coff_header header;
    coff_syment entry;
    uint32 nsyms, symptr, length, i;
    char buf[10];
    char *symtable;
    
    fseek(file, 0, SEEK_SET);
    fread(&header, sizeof(coff_header), 1, file);
    nsyms = ntohl(header.nsyms);
    symptr = ntohl(header.symptr);

    fseek(file, symptr + 18 * nsyms, SEEK_SET);
    fread(&buf, 4, 1, file);
    length = ntohl(*(uint32 *)buf);
    symtable = malloc(length + 4);
    fread(symtable+4, 1, length, file);

    fseek(file, symptr, SEEK_SET);
    for (i = 0; i < nsyms; i++) {
        char * name;
        uint16 address;
        fread(&entry, 18, 1, file);
        address = ntohl(entry.e_value);

        if (entry.e.e.e_zeroes) {
            /* name is inlined */
            memcpy(buf, entry.e.e_name, 8);
            buf[8] = 0;
            name = buf;
        } else {
            name = symtable + ntohl(entry.e.e.e_offset);
        }
        if (name[0] != '.' 
            && (entry.e_sclass == C_EXT
                || entry.e_sclass == C_STAT
                || (entry.e_sclass == C_LABEL
                    && symbols_get(address, 0) == NULL))) {
            symbols_add(address, 0, strdup(name));
        }
#if 0
        if (entry.e_sclass == C_EXT
            || entry.e_sclass == C_STAT) {
            printf("Symbol class:%3d section:%2d type:%02x numaux:%3d  addr:%04x  name:%s\n",
                   entry.e_sclass, ntohs(entry.e_scnum), ntohs(entry.e_type), 
                   entry.e_numaux, address, name);
        }
#endif
        if (entry.e_numaux)
            fseek(file, 18 * entry.e_numaux, SEEK_CUR);
    }
    free(symtable);
}
