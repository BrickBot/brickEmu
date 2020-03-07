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
 * $Id: sound_sdl.c 111 2005-03-13 15:43:31Z hoenicke $
 */

#include <string.h>
#include <SDL.h>

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"


#define SAMPLE_RATE 48000
#define CYCLES_PER_SEC_DIV_50  ((CYCLES_PER_USEC * 1000000 / 50))
#define DECAY     0xf400              /* Depends on sample rate */
#define AMPLITUDE ((0x10000 - DECAY) >> 4)


#define BUFFER_LENGTH 40960

static int16 sample_buffer[BUFFER_LENGTH];
volatile static unsigned int sample_start, sample_end;
#define FRAGMENTS ((2<<16) | 7)
static int16 sample_level = 0;


static void sound_send_samples(void *buff, Uint8 *data, int len) {
    unsigned int send   = sample_end;
    unsigned int sstart = sample_start;
    unsigned int size;
    int16 *ptr;
    int16 *dest = (int16*) data;

    if (send < sstart) {
	size = BUFFER_LENGTH - sstart;
	if (size > (unsigned) len)
	    size = len;

	len -= size;
	ptr = sample_buffer + sstart;
	sstart = (sstart + size) % BUFFER_LENGTH;
	memcpy(dest, ptr, 2*size);
	dest += size;
	ptr += size;
    }
    if (len > 0) {
	size = send - sstart;
	if (size > (unsigned) len)
	    size = len;
	len -= size;
	ptr = sample_buffer + sstart;
	sstart += size;
	memcpy(dest, ptr, 2*size);
	dest += size;
	ptr += size;
    }
    sample_start = sstart;
    if (len > 0) {
#if 0
	printf("Buffer underflow: %d\n", len);
#endif
	unsigned int level = sample_level;
	while (len-- > 0)
	    *dest++ = level;
    }

    return;
}


void sound_update(int bit, uint32 new_incr) {
    static uint32 incr = 0;
    int output = bit ? AMPLITUDE : -AMPLITUDE;
    int level = sample_level;

    /*printf("Update Sound: %d for %ld  (%d/%d/%d)\n", bit, incr, tcnt[0], tcora[0], tcorb[0]);*/

    incr += new_incr * (SAMPLE_RATE / 50);
    if (incr < CYCLES_PER_SEC_DIV_50)
	return;

    unsigned int ptr = sample_end;
    while (incr > CYCLES_PER_SEC_DIV_50) {
	unsigned int len = BUFFER_LENGTH - ptr;
	if (sample_start > ptr)
	    len = sample_start - ptr - 1;

	if (len == 0) {
	    /* buffer overflow, move sample_start */
#if 0
	    printf("Buffer overflow: %d\n", len);
#endif
	    incr = 0;
	}
	while (incr > CYCLES_PER_SEC_DIV_50 && len-- > 0) {
	    level = ((level * DECAY) >> 16) + output;
	    sample_buffer[ptr++] = level;
	    incr -= CYCLES_PER_SEC_DIV_50;
	}
	if (ptr == BUFFER_LENGTH) {
	    ptr = 0;
	}
    }
    sample_end = ptr;
    sample_level = level;
}

void sound_init() {
    SDL_AudioSpec desired, obtained;
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = sound_send_samples;
    desired.userdata = NULL;

    if (SDL_OpenAudio(&desired, &obtained) == 0) {
	printf("SDL Audio: Obtained %dx%d %d\n", 
	       obtained.freq, obtained.channels, obtained.format);
	SDL_PauseAudio(0);
    }
    sample_start = sample_end = 0;
}
