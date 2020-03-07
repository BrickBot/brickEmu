/*
 * Copyright Patrick Powell 1995
 * This code is based on code written by Patrick Powell (papowell@astart.com)
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions
 */

/**************************************************************
 * Original:
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt (dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 *
 * More Recently:
 *  Brandon Long <blong@fiction.net> 9/15/96 for mutt 0.43
 *  This was ugly.  It is still ugly.  I opted out of floating point
 *  numbers, but the formatter understands just about everything
 *  from the normal C string format, at least as far as I can tell from
 *  the Solaris 2.5 printf(3S) man page.
 *
 *  Brandon Long <blong@fiction.net> 10/22/97 for mutt 0.87.1
 *    Ok, added some minimal floating point support, which means this
 *    probably requires libm on most operating systems.  Don't yet
 *    support the exponent (e,E) and sigfig (g,G).  Also, fmtint()
 *    was pretty badly broken, it just wasn't being exercised in ways
 *    which showed it, so that's been fixed.  Also, formated the code
 *    to mutt conventions, and removed dead code left over from the
 *    original.  Also, there is now a builtin-test, just compile with:
 *           gcc -DTEST_SNPRINTF -o snprintf snprintf.c -lm
 *    and run snprintf for results.
 * 
 *  Thomas Roessler <roessler@guug.de> 01/27/98 for mutt 0.89i
 *    The PGP code was using unsigned hexadecimal formats. 
 *    Unfortunately, unsigned formats simply didn't work.
 *
 *  Michael Elkins <me@cs.hmc.edu> 03/05/98 for mutt 0.90.8
 *    The original code assumed that both snprintf() and vsnprintf() were
 *    missing.  Some systems only have snprintf() but not vsnprintf(), so
 *    the code is now broken down under HAVE_SNPRINTF and HAVE_VSNPRINTF.
 *
 *  Andrew Tridgell (tridge@samba.org) Oct 1998
 *    fixed handling of %.0f
 *    added test for HAVE_LONG_DOUBLE
 *
 * tridge@samba.org, idra@samba.org, April 2001
 *    got rid of fcvt code (twas buggy and made testing harder)
 *    added C99 semantics
 *
 * date: 2002/12/19 19:56:31;  author: herb;  state: Exp;  lines: +2 -0
 * actually print args for %g and %e
 * 
 * date: 2002/06/03 13:37:52;  author: jmcd;  state: Exp;  lines: +8 -0
 * Since includes.h isn't included here, VA_COPY has to be defined here.  I don't
 * see any include file that is guaranteed to be here, so I'm defining it
 * locally.  Fixes AIX and Solaris builds.
 * 
 * date: 2002/06/03 03:07:24;  author: tridge;  state: Exp;  lines: +5 -13
 * put the ifdef for HAVE_VA_COPY in one place rather than in lots of
 * functions
 * 
 * date: 2002/05/17 14:51:22;  author: jmcd;  state: Exp;  lines: +21 -4
 * Fix usage of va_list passed as an arg.  Use __va_copy before using it
 * when it exists.
 * 
 * date: 2002/04/16 22:38:04;  author: idra;  state: Exp;  lines: +20 -14
 * Fix incorrect zpadlen handling in fmtfp.
 * Thanks to Ollie Oldham <ollie.oldham@metro-optix.com> for spotting it.
 * few mods to make it easier to compile the tests.
 * addedd the "Ollie" test to the floating point ones.
 *
 * Martin Pool (mbp@samba.org) April 2003
 *    Remove NO_CONFIG_H so that the test case can be built within a source
 *    tree with less trouble.
 *    Remove unnecessary SAFE_FREE() definition.
 *
 * Martin Pool (mbp@samba.org) May 2003
 *    Put in a prototype for dummy_snprintf() to quiet compiler warnings.
 *
 *    Move #endif to make sure VA_COPY, LDOUBLE, etc are defined even
 *    if the C library has some snprintf functions already.
 **************************************************************/

#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "memory.h"
#include "h8300.h"
#include "peripherals.h"

/* format read states */
#define DP_S_DEFAULT 0
#define DP_S_FLAGS   1
#define DP_S_MIN     2
#define DP_S_DOT     3
#define DP_S_MAX     4
#define DP_S_MOD     5
#define DP_S_CONV    6
#define DP_S_DONE    7

/* format flags - Bits */
#define DP_F_MINUS 	(1 << 0)
#define DP_F_PLUS  	(1 << 1)
#define DP_F_SPACE 	(1 << 2)
#define DP_F_NUM   	(1 << 3)
#define DP_F_ZERO  	(1 << 4)
#define DP_F_UP    	(1 << 5)
#define DP_F_UNSIGNED 	(1 << 6)

/* Conversion Flags */
#define DP_C_SHORT   1
#define DP_C_LONG    2
#define DP_C_LDOUBLE 3
#define DP_C_LLONG   4

#define char_to_int(p) ((p)- '0')
#ifndef MAX
#define MAX(p,q) (((p) >= (q)) ? (p) : (q))
#endif

static void fmtstr(char *buffer, size_t *currlen, size_t maxlen,
		    char *value, int flags, int min, int max);
static void fmtint(char *buffer, size_t *currlen, size_t maxlen,
		    long value, int base, int min, int max, int flags);
static void dopr_outch(char *buffer, size_t *currlen, size_t maxlen, char c);

void debug_printf(void) {
    char buffer[4096];
    size_t maxlen = sizeof(buffer);

    int fp = GET_REG16(7)+2;
    const char *format = (char*) &memory[GET_WORD(fp)];
    fp += 2;

    char ch;
    long value;
    char *strvalue;
    int min;
    int max;
    int state;
    int flags;
    int cflags;
    size_t currlen;
    
    state = DP_S_DEFAULT;
    currlen = flags = cflags = min = 0;
    max = -1;
    ch = *format++;
    
    while (state != DP_S_DONE) {
	if (ch == '\0') 
	    state = DP_S_DONE;
	
	switch(state) {
	case DP_S_DEFAULT:
	    if (ch == '%') 
		state = DP_S_FLAGS;
	    else 
		dopr_outch (buffer, &currlen, maxlen, ch);
	    ch = *format++;
	    break;
	case DP_S_FLAGS:
	    switch (ch) {
	    case '-':
		flags |= DP_F_MINUS;
		ch = *format++;
		break;
	    case '+':
		flags |= DP_F_PLUS;
		ch = *format++;
		break;
	    case ' ':
		flags |= DP_F_SPACE;
		ch = *format++;
		break;
	    case '#':
		flags |= DP_F_NUM;
		ch = *format++;
		break;
	    case '0':
		flags |= DP_F_ZERO;
		ch = *format++;
		break;
	    default:
		state = DP_S_MIN;
		break;
	    }
	    break;
	case DP_S_MIN:
	    if (isdigit((unsigned char)ch)) {
		min = 10*min + char_to_int (ch);
		ch = *format++;
	    } else if (ch == '*') {
		min = (short) GET_WORD(fp);
		fp += 2;
		ch = *format++;
		state = DP_S_DOT;
	    } else {
		state = DP_S_DOT;
	    }
	    break;
	case DP_S_DOT:
	    if (ch == '.') {
		state = DP_S_MAX;
		ch = *format++;
	    } else { 
		state = DP_S_MOD;
	    }
	    break;
	case DP_S_MAX:
	    if (isdigit((unsigned char)ch)) {
		if (max < 0)
		    max = 0;
		max = 10*max + char_to_int (ch);
		ch = *format++;
	    } else if (ch == '*') {
		min = (short) GET_WORD(fp);
		fp += 2;
		ch = *format++;
		state = DP_S_MOD;
	    } else {
		state = DP_S_MOD;
	    }
	    break;
	case DP_S_MOD:
	    switch (ch) {
	    case 'h':
		cflags = DP_C_SHORT;
		ch = *format++;
		break;
	    case 'l':
		cflags = DP_C_LONG;
		ch = *format++;
		break;
	    default:
		break;
	    }
	    state = DP_S_CONV;
	    break;
	case DP_S_CONV:
	    switch (ch) {
	    case 'd':
	    case 'i':
		if (cflags == DP_C_LONG) {
		    value = ((int) (short) GET_WORD(fp)) << 16
			| GET_WORD(fp+2);
		    fp += 4;
		} else {
		    value = (short) GET_WORD(fp);
		    fp += 2;
		}
		fmtint (buffer, &currlen, maxlen, value, 10, min, max, flags);
		break;
	    case 'o':
		flags |= DP_F_UNSIGNED;
		if (cflags == DP_C_LONG) {
		    value = ((unsigned int) GET_WORD(fp)) << 16
			| GET_WORD(fp+2);
		    fp += 4;
		} else {
		    value = GET_WORD(fp);
		    fp += 2;
		}
		fmtint (buffer, &currlen, maxlen, value, 8, min, max, flags);
		break;
	    case 'u':
		flags |= DP_F_UNSIGNED;
		if (cflags == DP_C_LONG) {
		    value = ((unsigned int) GET_WORD(fp)) << 16
			| GET_WORD(fp+2);
		    fp += 4;
		} else {
		    value = GET_WORD(fp);
		    fp += 2;
		}
		fmtint (buffer, &currlen, maxlen, value, 10, min, max, flags);
		break;
	    case 'X':
		flags |= DP_F_UP;
	    case 'x':
		flags |= DP_F_UNSIGNED;
		if (cflags == DP_C_LONG) {
		    value = ((unsigned int) GET_WORD(fp)) << 16
			| GET_WORD(fp+2);
		    fp += 4;
		} else {
		    value = GET_WORD(fp);
		    fp += 2;
		}
		fmtint (buffer, &currlen, maxlen, value, 16, min, max, flags);
		break;
	    case 'c':
		dopr_outch (buffer, &currlen, maxlen, 
			    (int)(short)GET_WORD(fp));
		fp += 2;
		break;
	    case 's':
		{
		    int addr = GET_WORD(fp);
		    strvalue = (char*) &memory[addr];
		    if (!addr) 
			strvalue = "(NULL)";
		    if (max == -1) {
			max = strlen(strvalue);
		    }
		    if (min > 0 && max >= 0 && min > max) max = min;
		    fmtstr (buffer, &currlen, maxlen, strvalue, flags, 
			    min, max);
		}
		break;
	    case 'p':
		value = GET_WORD(fp);
		fmtint (buffer, &currlen, maxlen, value, 16, min, max, flags);
		break;
	    case '%':
		dopr_outch (buffer, &currlen, maxlen, ch);
		break;
	    default:
		/* Unknown, skip */
		break;
	    }
	    ch = *format++;
	    state = DP_S_DEFAULT;
	    flags = cflags = min = 0;
	    max = -1;
	    break;
	case DP_S_DONE:
	    break;
	default:
	    /* hmm? */
	    break; /* some picky compilers need this */
	}
    }
    if (maxlen != 0) {
	if (currlen < maxlen - 1) 
	    buffer[currlen] = '\0';
	else if (maxlen > 0) 
	    buffer[maxlen - 1] = '\0';
    }
    
    printf("Program said: `%s'\n", buffer);
}

static void fmtstr(char *buffer, size_t *currlen, size_t maxlen,
		    char *value, int flags, int min, int max)
{
	int padlen, strln;     /* amount to pad */
	int cnt = 0;

#ifdef DEBUG_SNPRINTF
	printf("fmtstr min=%d max=%d s=[%s]\n", min, max, value);
#endif
	if (value == 0) {
		value = "<NULL>";
	}

	for (strln = 0; value[strln]; ++strln); /* strlen */
	padlen = min - strln;
	if (padlen < 0) 
		padlen = 0;
	if (flags & DP_F_MINUS) 
		padlen = -padlen; /* Left Justify */
	
	while ((padlen > 0) && (cnt < max)) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		--padlen;
		++cnt;
	}
	while (*value && (cnt < max)) {
		dopr_outch (buffer, currlen, maxlen, *value++);
		++cnt;
	}
	while ((padlen < 0) && (cnt < max)) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		++padlen;
		++cnt;
	}
}

/* Have to handle DP_F_NUM (ie 0x and 0 alternates) */

static void fmtint(char *buffer, size_t *currlen, size_t maxlen,
		    long value, int base, int min, int max, int flags)
{
	int signvalue = 0;
	unsigned long uvalue;
	char convert[20];
	int place = 0;
	int spadlen = 0; /* amount to space pad */
	int zpadlen = 0; /* amount to zero pad */
	int caps = 0;
	
	if (max < 0)
		max = 0;
	
	uvalue = value;
	
	if(!(flags & DP_F_UNSIGNED)) {
		if( value < 0 ) {
			signvalue = '-';
			uvalue = -value;
		} else {
			if (flags & DP_F_PLUS)  /* Do a sign (+/i) */
				signvalue = '+';
			else if (flags & DP_F_SPACE)
				signvalue = ' ';
		}
	}
  
	if (flags & DP_F_UP) caps = 1; /* Should characters be upper case? */

	do {
		convert[place++] =
			(caps? "0123456789ABCDEF":"0123456789abcdef")
			[uvalue % (unsigned)base  ];
		uvalue = (uvalue / (unsigned)base );
	} while(uvalue && (place < 20));
	if (place == 20) place--;
	convert[place] = 0;

	zpadlen = max - place;
	spadlen = min - MAX (max, place) - (signvalue ? 1 : 0);
	if (zpadlen < 0) zpadlen = 0;
	if (spadlen < 0) spadlen = 0;
	if (flags & DP_F_ZERO) {
		zpadlen = MAX(zpadlen, spadlen);
		spadlen = 0;
	}
	if (flags & DP_F_MINUS) 
		spadlen = -spadlen; /* Left Justifty */

#ifdef DEBUG_SNPRINTF
	printf("zpad: %d, spad: %d, min: %d, max: %d, place: %d\n",
	       zpadlen, spadlen, min, max, place);
#endif

	/* Spaces */
	while (spadlen > 0) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		--spadlen;
	}

	/* Sign */
	if (signvalue) 
		dopr_outch (buffer, currlen, maxlen, signvalue);

	/* Zeros */
	if (zpadlen > 0) {
		while (zpadlen > 0) {
			dopr_outch (buffer, currlen, maxlen, '0');
			--zpadlen;
		}
	}

	/* Digits */
	while (place > 0) 
		dopr_outch (buffer, currlen, maxlen, convert[--place]);
  
	/* Left Justified spaces */
	while (spadlen < 0) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		++spadlen;
	}
}

static void dopr_outch(char *buffer, size_t *currlen, size_t maxlen, char c)
{
	if (*currlen < maxlen) {
		buffer[(*currlen)] = c;
	}
	(*currlen)++;
}
