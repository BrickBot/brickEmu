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
 * $Id: adsensors.c 152 2005-08-13 15:24:05Z hoenicke $
 */

#include <unistd.h>
#include <fcntl.h>

#include <netinet/in.h> /* for htonx/ntohx */

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"

#define ADCSR_ADF  0x80
#define ADCSR_ADIE 0x40
#define ADCSR_ADST 0x20
#define ADCSR_SCAN 0x10
#define ADCSR_CKS  0x08
#define ADCSR_CH   0x07


static uint16 values[8], polled[4];
static uint8 tmp;
static uint8 adcsr, read_adcsr, adcr, adchannel;
static int32 ad_start_cycle;

typedef struct {
    int32 ad_start_cycle;
    uint16 polled[4];
    uint8 tmp;
    uint8 adcsr, read_adcsr, adcr, adchannel;
} ad_save_type;

static int ad_save(void *buffer, int maxlen) {
    int i;
    ad_save_type *data = buffer;
    for (i = 0; i< 4; i++)
	data->polled[i] = htons(polled[i]);
    data->tmp = tmp;
    data->adcsr = adcsr;
    data->read_adcsr = read_adcsr;
    data->adcr = adcr;
    data->adchannel = adchannel;
    data->ad_start_cycle = htonl(ad_start_cycle);
    return sizeof(ad_save_type);
}

static void ad_load(void *buffer, int len) {
    int i;
    ad_save_type *data = buffer;
    for (i = 0; i< 4; i++)
	polled[i] = ntohs(data->polled[i]);
    tmp = data->tmp;
    adcsr = data->adcsr;
    read_adcsr = data->read_adcsr;
    adcr = data->adcr;
    adchannel = data->adchannel;
    ad_start_cycle = ntohl(data->ad_start_cycle);
}

static void ad_reset() {
    adcr = 0x7f;
    adcsr = 0;
    adchannel = 0;
}

static void ad_read_fd(int fd) {
    char buf[5];
    int len = 0;

    /* read in 5 bytes: sensorid 3xVal newline */
    do {
	len += read(fd, buf + len, 5 - len);
    } while (len < 5);

    values[buf[0]-'0'] =  
	((buf[1] > '9' ? buf[1] - 'a' + 10 : buf[1]-'0') << 14)
	| ((buf[2] > '9' ? buf[2] - 'a' + 10 : buf[2]-'0') << 10)
	| ((buf[3] > '9' ? buf[3] - 'a' + 10 : buf[3]-'0') <<  6);
}

static void ad_check_next_cycle() {
    if ((adcsr & 0xc0) == 0xc0)
	next_timer_cycle = cycles;

    else if ((adcsr & (ADCSR_ADIE|ADCSR_ADST)) == (ADCSR_ADIE|ADCSR_ADST)) {

	int next_conv_time = ad_start_cycle + (adcsr & ADCSR_CKS ? 134 : 266);
	if ((int32) (next_conv_time - cycles)
	    < (int32) (next_timer_cycle - cycles))
	    next_timer_cycle = next_conv_time;
    }
}

static void ad_update_time() {
    int conv_time = (adcsr & ADCSR_CKS ? 134 : 266);
    while ((adcsr & ADCSR_ADST)) {
	int32 polling_time = cycles - ad_start_cycle;
	if (polling_time < conv_time)
	    break;

	polled[adchannel & 3]  = values[adchannel];
	if ((adcsr & ADCSR_SCAN)) {
	    ad_start_cycle += conv_time;
	    if (adchannel < (adcsr & ADCSR_CH)) {
		adchannel++;
	    } else {
		adchannel &= 4;
		adcsr = (adcsr & ~ADCSR_ADST) | ADCSR_ADF;
	    }
	} else {
	    adcsr = (adcsr & ~ADCSR_ADST) | ADCSR_ADF;
	}
    }
    ad_check_next_cycle();
}

static int ad_check_irq() {
    if ((adcsr & 0xc0) == 0xc0)
	return 35;
    return 255;
}

static void set_adcsr(uint8 value) {
    value &= 0x7f | read_adcsr; /* high bit can only be cleared */

#ifdef VERBOSE_ADSENSORS
    printf("adsensors.c: set_adcsr(%02x)\n", value);
#endif

    if ((~adcsr & value & ADCSR_ADST)) {
	adchannel = (value & ADCSR_SCAN) ? value & 4 : value & ADCSR_CH;
	ad_start_cycle = cycles;
    }

    adcsr = value;
    ad_check_next_cycle();
}
static uint8 get_adcsr(void) {
    return read_adcsr = adcsr;
}
static void set_adcr(uint8 value) {
    adcr = value | 0x7f;
#ifdef VERBOSE_ADSENSORS
    printf("adsensors.c: set_adcr(%02x)\n", value);
#endif
    ad_check_next_cycle();
}
static uint8 get_adcr(void) {
    return adcr;
}

static uint8 get_addrah(void) {
    tmp = (polled[0] & 0xc0);
    return (polled[0] >> 8);
}
static uint8 get_addral(void) {
    return tmp;
}
static uint8 get_addrbh(void) {
    tmp = (polled[1] & 0xc0);
    return (polled[1] >> 8);
}
static uint8 get_addrbl(void) {
    return tmp;
}
static uint8 get_addrch(void) {
    tmp = (polled[2] & 0xc0);
    return (polled[2] >> 8);
}
static uint8 get_addrcl(void) {
    return tmp;
}
static uint8 get_addrdh(void) {
    tmp = (polled[3] & 0xc0);
    return (polled[3] >> 8);
}
static uint8 get_addrdl(void) {
    return tmp;
}

static peripheral_ops adsensor = {
    id: 'A',
    reset: ad_reset,
    update_time: ad_update_time,
    read_fd: ad_read_fd,
    check_irq: ad_check_irq,
    load_data: ad_load,
    save_data: ad_save
};

void ad_init() {
    port[0xe0 - 0x88].get = get_addrah;
    port[0xe1 - 0x88].get = get_addral;
    port[0xe2 - 0x88].get = get_addrbh;
    port[0xe3 - 0x88].get = get_addrbl;
    port[0xe4 - 0x88].get = get_addrch;
    port[0xe5 - 0x88].get = get_addrcl;
    port[0xe6 - 0x88].get = get_addrdh;
    port[0xe7 - 0x88].get = get_addrdl;

    port[0xe8 - 0x88].get = get_adcsr;
    port[0xe8 - 0x88].set = set_adcsr;
    port[0xe9 - 0x88].get = get_adcr;
    port[0xe9 - 0x88].set = set_adcr;

    register_peripheral(adsensor);
}
