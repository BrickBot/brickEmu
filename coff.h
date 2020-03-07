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
 * $Id: coff.h 85 2004-05-13 11:54:04Z hoenicke $
 */

#ifndef _COFF_H_
#define  _COFF_H_

#include "types.h"

typedef struct coff_header {
    uint16 magic;	/* magic number			*/
    uint16 nscns;	/* number of sections		*/
    uint32 timdat; 	/* time & date stamp		*/
    uint32 symptr;	/* file pointer to symtab	*/
    uint32 nsyms;	/* number of symtab entries	*/
    uint16 opthdr;	/* sizeof(optional hdr)		*/
    uint16 flags;	/* flags			*/
} coff_header;

#define	F_RELFLG	(0x0001)
#define	F_EXEC		(0x0002)
#define	F_LNNO		(0x0004)
#define	F_LSYMS		(0x0008)
#define	F_AR16WR	(0x0080)
#define	F_AR32WR	(0x0100)
#define	F_AR32W     	(0x0200)
#define	F_DYNLOAD	(0x1000)
#define	F_SHROBJ	(0x2000)
#define F_DLL           (0x2000)


typedef struct coff_aouthdr {
    uint16 magic;	/* type of file				*/
    uint16 vstamp;	/* version stamp			*/
    uint32 tsize;	/* text size in bytes, padded to FW bdry*/
    uint32 dsize;	/* initialized data "  "		*/
    uint32 bsize;	/* uninitialized data "   "		*/
    uint32 entry;	/* entry pt.				*/
    uint32 text_start;	/* base of text used for this file 	*/
    uint32 data_start;	/* base of data used for this file 	*/
} coff_aouthdr;


typedef struct coff_section {
    uint8  sectname[8];
    uint32 paddr;
    uint32 vaddr;
    uint32 size;
    uint32 scnptr;
    uint32 relptr;
    uint32 lnnoptr;
    uint16 nreloc;
    uint16 nlnno;
    uint32 flags;
} coff_section;

#define STYP_REG	 (0x0000)	/* "regular": allocated, relocated, loaded */
#define STYP_DSECT	 (0x0001)	/* "dummy":  relocated only*/
#define STYP_NOLOAD	 (0x0002)	/* "noload": allocated, relocated, not loaded */
#define STYP_GROUP	 (0x0004)	/* "grouped": formed of input sections */
#define STYP_PAD	 (0x0008)	/* "padding": not allocated, not relocated, loaded */
#define STYP_COPY	 (0x0010)	/* "copy": for decision function used by field update;  not allocated, not relocated, */
					/* loaded; reloc & lineno entries processed normally */
#define STYP_TEXT	 (0x0020)	/* section contains text only */
#define S_SHRSEG	 (0x0020)	/* In 3b Update files (output of ogen), sections which appear in SHARED segments of the Pfile */
					/* will have the S_SHRSEG flag set by ogen, to inform dufr that updating 1 copy of the proc. will */
					/* update all process invocations. */
#define STYP_DATA	 (0x0040)	/* section contains data only */
#define STYP_BSS	 (0x0080)	/* section contains bss only */
#define S_NEWFCN	 (0x0100)	/* In a minimal file or an update file, a new function (as compared with a replaced function) */
#define STYP_INFO	 (0x0200)	/* comment: not allocated not relocated, not loaded */
#define STYP_OVER	 (0x0400)	/* overlay: relocated not allocated or loaded */
#define STYP_LIB	 (0x0800)	/* for .lib: same as INFO */
#define STYP_MERGE	 (0x2000)	/* merge section -- combines with text, data or bss sections only */
#define STYP_REVERSE_PAD (0x4000)	/* section will be padded with no-op instructions */

#define STYP_LOAD (STYP_REG | STYP_TEXT | STYP_DATA)

typedef struct coff_syment {
  union {
    char e_name[8];
    struct {
      uint32 e_zeroes;
      uint32 e_offset;
    } e;
  } e;
  uint32 e_value;
  int16  e_scnum;
  uint16 e_type;
  uint8  e_sclass;
  uint8  e_numaux;
} coff_syment;

#define C_NULL   0
#define C_AUTO   1
#define C_EXT    2
#define C_STAT   3
#define C_REG    4
#define C_EXTDEF 5
#define C_LABEL  6
#define C_ULABEL 7
#define C_MOS    8
#define C_ARG    9
#define C_STRTAG 10
#define C_MOU    11
#define C_UNTAG  12
#define C_TPDEF  13
#define C_USTATIC 14
#define C_ENTAG  15
#define C_MOE    16
#define C_REGPARM 17
#define C_FIELD  18
#define C_AUTOARG 19
#define C_LASTENT 20
#define C_BLOCK  100
#define C_FCN    101
#define C_EOS    102
#define C_FILE   103
#define C_LINE   104
#define C_ALIAS  105
#define C_HIDDEN 106
#define C_EFCN   255


extern int coff_read (FILE *file, int start);
extern int coff_init (FILE *file);
extern void coff_symbols (FILE *file, int start);

#endif
