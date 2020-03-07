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
 * $Id: sound_alsa.c 313 2006-03-15 21:09:32Z hoenicke $
 */

#include <string.h>

#include <alsa/asoundlib.h>


#include "h8300.h"
#include "memory.h"
#include "peripherals.h"

#define SAMPLE_RATE 48000
#define CYCLES_PER_SEC_DIV_50  ((CYCLES_PER_USEC * 1000000 / SLOW_DOWN / 50))
#define DECAY     0xf400              /* Depends on sample rate */
#define AMPLITUDE ((0x10000 - DECAY) >> 4)

static int16 sample_level = 0;
snd_pcm_t *pcm_handle;
int pcm_stopped;

#define CHANNELS 1

void sound_update(int bit, uint32 new_incr) {
    static uint32 incr = 0;
    int output = bit ? AMPLITUDE : -AMPLITUDE;
    int level = sample_level;

    if (!pcm_handle)
	return;

    /*printf("Update Sound: %d for %ld  (%d/%d/%d)\n", bit, incr, tcnt[0], tcora[0], tcorb[0]);*/

    incr += new_incr * (SAMPLE_RATE / 50);
    if (incr < CYCLES_PER_SEC_DIV_50)
	return;
    
    snd_pcm_sframes_t avail,commitres;
    const snd_pcm_channel_area_t *mmap_areas;
    snd_pcm_uframes_t offset, frames;
    int err;
    snd_pcm_state_t state;
    
    state = snd_pcm_state(pcm_handle);
    if (state == SND_PCM_STATE_XRUN) {
	err = snd_pcm_prepare(pcm_handle);
	if (err < 0)
	    printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
	printf("Underrun!\n");
	pcm_stopped = 1;
    } else if (state == SND_PCM_STATE_SUSPENDED) {
	    printf("Sound suspended\n");
	while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN) {
	    printf("Sound suspended\n");
	    sleep(1);	/* wait until the suspend flag is released */
	}
	if (err < 0) {
	    err = snd_pcm_prepare(pcm_handle);
	    if (err < 0)
		printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
	}
    }
    avail = snd_pcm_avail_update(pcm_handle);
    if (avail < 0)
	return;

    if (pcm_stopped) {
	snd_pcm_sframes_t delay;
	err = snd_pcm_delay(pcm_handle, &delay);
	if (err < 0)
	    printf("Can't get delay: %s\n", snd_strerror(err));
	if (avail < 5000) {
	    pcm_stopped = 0;
	    printf("Restarting with %ld delay\n", delay);
	    err = snd_pcm_start(pcm_handle);
	    if (err < 0) {
		printf("Start error: %s\n", snd_strerror(err));
	    }
	}
    }

    while (incr > CYCLES_PER_SEC_DIV_50) {
	short *sptr[CHANNELS];
	int    step[CHANNELS];
	int i;
	frames = incr / CYCLES_PER_SEC_DIV_50;
	if (frames > avail) {
	    printf("Overrun: %ld\n", frames - avail);
	    frames = avail;
	    incr = frames * CYCLES_PER_SEC_DIV_50;
	}
	snd_pcm_mmap_begin(pcm_handle, &mmap_areas, &offset, &frames);
	for (i = 0; i < CHANNELS; i++) {
	    step[i] = mmap_areas[i].step / 16;  /* step is in bits,not shorts*/
	    sptr[i] = (short*) mmap_areas[i].addr 
		+ (mmap_areas[i].first / 16)
		+ step[i] * offset;
	}
	int len = frames;
	while (len-- > 0) {
	    level = ((level * DECAY) >> 16) + output;
	    for (i = 0; i < CHANNELS; i++) {
		*sptr[i] = level;
		sptr[i] += step[i];
	    }
	}
	commitres = snd_pcm_mmap_commit(pcm_handle, offset, frames);
	if (commitres != frames)
	    printf ("commit res: %ld\n", commitres);
	incr -= frames * CYCLES_PER_SEC_DIV_50;
    }
	

    sample_level = level;
}


void sound_init() {
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    unsigned int rate;
    unsigned int buffer_time = 500000;
    unsigned int period_time = 100000;
    int err;
    int dir;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
    char *alsa_device;
    alsa_device = getenv("AUDIODEV");
    if (!alsa_device)
	alsa_device="default";

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_pcm_open(&pcm_handle, alsa_device, SND_PCM_STREAM_PLAYBACK, 0);
    if (err) {
	printf("Unable to open pcm device: %s\n", snd_strerror(err));
	return;
    }

    /* choose all parameters */
    err = snd_pcm_hw_params_any(pcm_handle, hwparams);
    if (err < 0) {
	printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
	return;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(pcm_handle, hwparams,
				       SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (err < 0) {
	printf("Access type not available for playback: %s\n", snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params_set_format(pcm_handle, hwparams, 
				       SND_PCM_FORMAT_S16);
    if (err < 0) {
	perror("snd_pcm_set_formats");
	return;
    }
    err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, CHANNELS);
    if (err < 0) {
	perror("snd_pcm_set_channels");
	return;
    }
    rate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &rate, 0);
    if (err < 0) {
	perror("snd_pcm_set_rate_near");
	return;
    }
    printf("Set sample rate to %d\n", rate);
    
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hwparams, &buffer_time, &dir);
    if (err < 0) {
	printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
    if (err < 0) {
	printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
	return;
    }
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(pcm_handle, hwparams, &period_time, &dir);
    if (err < 0) {
	printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
	return;
    }
    /* write the parameters to device */
    err = snd_pcm_hw_params(pcm_handle, hwparams);
    if (err < 0) {
	printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
	return;
    }
    
    err = snd_pcm_hw_params_get_period_size(hwparams, &period_size, &dir);
    if (err < 0) {
	printf("Unable to get period size for playback: %s\n", snd_strerror(err));
	return;
    }
    {
	unsigned int channels;
	int dir;
	snd_pcm_hw_params_get_channels(hwparams, &channels);
	snd_pcm_hw_params_get_rate(hwparams, &rate, &dir);
	printf("Period time: %lu, channels: %d, sample rate: %d%c\n", 
	       period_size, channels, rate, dir>0 ? '+' : dir < 0 ? '-' : '=');
    }
    
    /* get the current swparams */
    err = snd_pcm_sw_params_current(pcm_handle, swparams);
    if (err < 0) {
	printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
	return;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
	printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
	return;
    }
    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, period_size);
    if (err < 0) {
	printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
	return;
    }
    /* align all transfers to 1 sample */
    err = snd_pcm_sw_params_set_xfer_align(pcm_handle, swparams, 1);
    if (err < 0) {
	printf("Unable to set transfer align for playback: %s\n", snd_strerror(err));
	return;
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(pcm_handle, swparams);
    if (err < 0) {
	printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
	return;
    }
    
    err = snd_pcm_prepare(pcm_handle);
    if (err < 0) {
	printf("Prepare error: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }

    pcm_stopped= 1;
}
