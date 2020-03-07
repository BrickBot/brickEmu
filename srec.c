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
 * $Id: srec.c 50 2003-10-06 14:08:51Z hoenicke $
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "memory.h"

#define SREC_DATA_SIZE 512

typedef struct {
    unsigned char type;
    unsigned long addr;
    unsigned char count;
    unsigned char data[SREC_DATA_SIZE];
} srec_t;

/* Error values */
#define SREC_OK               0
#define SREC_NULL            -1
#define SREC_INVALID_HDR     -2
#define SREC_INVALID_CHAR    -3
#define SREC_INVALID_TYPE    -4
#define SREC_TOO_SHORT       -5
#define SREC_TOO_LONG        -6
#define SREC_INVALID_LEN     -7
#define SREC_INVALID_CKSUM   -8

static signed char hex2int[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7,   8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
};
static int ltab[10] = {4,4,6,8,0,4,0,8,6,4};

/* Macros */

#define C1(l,p)    hex2int[l[p]]
#define C2(l,p)    ((C1(l,p)<<4)|C1(l,p+1))


static char *
srec_strerror (int error)
{
    switch (error) {
    case SREC_OK: return "no error";
    case SREC_NULL: return "null string error";
    case SREC_INVALID_HDR: return "invalid header";
    case SREC_INVALID_CHAR: return "invalid character";
    case SREC_TOO_SHORT: return "line too short";
    case SREC_TOO_LONG: return "line too long";
    case SREC_INVALID_LEN: return "length error";
    case SREC_INVALID_CKSUM: return "checksum error";
    default: return "unknown error";
    }
}

static int
srec_decode(srec_t *srec, char *_line)
{
    int len, pos = 0, count, alen, sum = 0;
    unsigned char *line = (unsigned char *)_line;

    if (!srec || !line)
	return SREC_NULL;

    for (len = 0; line[len]; len++)
	if (line[len] == '\n' || line[len] == '\r')
	    break;

    if (len < 4)
	return SREC_INVALID_HDR;

    if (line[0] != 'S')
	return SREC_INVALID_HDR;

    for (pos = 1; pos < len; pos++) {
	if (C1(line, pos) < 0)
	    return SREC_INVALID_CHAR;
    }

    srec->type = C1(line, 1);
    count = C2(line, 2);

    if (srec->type > 9)
	return SREC_INVALID_TYPE;
    alen = ltab[srec->type];
    if (alen == 0)
	return SREC_INVALID_TYPE;
    if (len < alen + 6)
	return SREC_TOO_SHORT;
    if (count > alen + SREC_DATA_SIZE + 2)
	return SREC_TOO_LONG;
    if (len != count * 2 + 4)
	return SREC_INVALID_LEN;

    sum += count;

    len -= 4;
    line += 4;

    srec->addr = 0;
    for (pos = 0; pos < alen; pos += 2) {
	unsigned char value = C2(line, pos);
	srec->addr = (srec->addr << 8) | value;
	sum += value;
    }

    len -= alen;
    line += alen;

    for (pos = 0; pos < len - 2; pos += 2) {
	unsigned char value = C2(line, pos);
	srec->data[pos / 2] = value;
	sum += value;
    }

    srec->count = count - (alen / 2) - 1;

    sum += C2(line, pos);

    if ((sum & 0xff) != 0xff)
	return SREC_INVALID_CKSUM;

    return SREC_OK;
}

int srec_read (FILE *file, int start) {
    char buf[256];
    srec_t srec;
    int line = 0;
    int entry = 0;

    /* Read image file */
    while (fgets(buf, sizeof(buf), file)) {
	int error, i;
	line++;
	/* Skip blank lines */
	for (i = 0; buf[i]; i++)
	    if (!isspace(buf[i]))
		break;
	if (!buf[i])
	    continue;
	/* Decode line */
	if ((error = srec_decode(&srec, buf)) < 0) {
	    if (error != SREC_INVALID_CKSUM) {
		fprintf(stderr, "firmware: %s on line %d\n",
			srec_strerror(error), line);
		return -1;
	    }
	}
	/* Process s-record data */
	if (srec.type == 1) {
	    memcpy(&memory[srec.addr], &srec.data, srec.count);
	}
	/* Process image starting address */
	else if (srec.type == 9) {
	    entry = srec.addr;
	}
    }

    return entry;
}

int srec_init (FILE *file)
{
    return 1;
}
