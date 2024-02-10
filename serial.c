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
 * $Id: serial.c 83 2004-05-13 11:52:49Z hoenicke $
 */

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

#include "h8300.h"
#include "memory.h"
#include "peripherals.h"

//#define VERBOSE_SERIAL

#define BRICK_BROADCAST_PORT 50637

#define SMR_CA   0x80
#define SMR_CHR  0x40
#define SMR_PE   0x20
#define SMR_OE   0x10
#define SMR_STOP 0x08
#define SMR_MP   0x04
#define SMR_CKS1 0x02
#define SMR_CKS0 0x01

#define SCR_TE   0x20
#define SCR_RE   0x10

#define SSR_TDRE 0x80
#define SSR_RDRF 0x40
#define SSR_ORER 0x20
#define SSR_FER  0x10
#define SSR_PER  0x08
#define SSR_TEND 0x04
#define SSR_MPB  0x02
#define SSR_MPBT 0x01

int ser_cycles;

#ifdef VERBOSE_SERIAL
int first = 1, last = 1;
uint32 next_debug_out = 0;
uint8  serd[100000];
uint32 serc[100000];
uint8  serb[100000];
#endif

int serfd;

static int8 receiving;

static uint8  rdr, tdr, smr, scr, ssr, readssr, brr, stcr;
static cycle_count_t rx_cycle, tx_cycle;

/* Hook to add ftoa/b listeners */
#define SET_FTOA(v) do {} while(0)
#define SET_FTOB(v) do {} while(0)

static void ser_update_cycles() {
    int bits = (smr & SMR_CHR ? 7 : 8) + (smr & SMR_PE ? 1 : 0)
        + (smr & SMR_STOP ? 2 : 1) + 1;
    ser_cycles = ((brr+1) << (2*(smr & 3) + 5)) * bits;
    rx_cycle = add_to_cycle('r', cycles, ser_cycles);
#ifdef VERBOSE_SERIAL
    printf("ser_cycles is %d.\n", ser_cycles);
#endif
}

typedef struct {
    uint8  rdr, tdr, smr, scr, ssr, readssr, brr, stcr;
} ser_save_type;

static int ser_save(void *buffer, int maxlen) {
    ser_save_type *data = buffer;
    data->rdr = rdr;
    data->tdr = tdr;
    data->smr = smr;
    data->scr = scr;
    data->ssr = ssr;
    data->readssr = readssr;
    data->brr = brr;
    data->stcr = stcr;
    return sizeof(ser_save_type);
}

static void ser_load(void *buffer, int len) {
    ser_save_type *data = buffer;
    rdr = data->rdr;
    tdr = data->tdr;
    smr = data->smr;
    scr = data->scr;
    ssr = data->ssr;
    readssr = data->readssr;
    brr = data->brr;
    stcr = data->stcr;
    ser_update_cycles();
    tx_cycle = add_to_cycle('t', cycles, ser_cycles);
}

static void ser_reset() {
    rdr = smr = scr = 0;
    tdr = brr = 0xff;
    ssr = readssr = 0x84;
    stcr = 0xf8;
    receiving = 0;
    ser_update_cycles();
    tx_cycle = cycles;
}

static void ser_check_next_cycle() {
    uint32 next = ser_cycles;

#ifdef VERBOSE_SERIAL
    if ((int32)(cycles - next_debug_out) >= 0) {
      if (first < last) {
        while (first <last) {
          printf("%10ld: %s %02x (%ld).\n", serc[first], 
                 serd[first]?"->":"<-", serb[first],
                 serc[first] - serc[first-1]);
          first++;
        }
        next_debug_out = cycles + 5000000;
      } else {
        next_debug_out = cycles;
      }
    }
#endif

    if (scr & ssr & (SSR_RDRF | SSR_TDRE | SSR_TEND)) {
        /* interrupt is pending */
        next_timer_cycle = cycles;
        return;
    }

    if (rx_cycle != cycles && (uint32) (rx_cycle - cycles) < next
        && (scr & SSR_RDRF)) {
        next = rx_cycle - cycles;
    }
    if ((uint32) (tx_cycle - cycles) < next
        && (scr & (SSR_TDRE| SSR_TEND)))
        next = tx_cycle - cycles;

#ifdef VERBOSE_TX
    if (scr & ~ssr & (SSR_TDRE | SSR_TEND)) {
        printf("%10d: Sleeping for at most %d cycles until %d %02x/%02x\n", cycles, next, cycles+next, (int) scr, (int) ssr);
    }
#endif

    if ((int32) next < (int32) (next_timer_cycle - cycles)) {
        next_timer_cycle = add_to_cycle('q', cycles, next);
    }
}

static void ser_update_time() {
    if ((int32) (cycles - tx_cycle) >= 0) {
        if (!(ssr & SSR_TDRE)) {
            if ((scr & SCR_TE))
                write(serfd, &tdr, 1);
            ssr |= SSR_TDRE;
            tx_cycle = add_to_cycle('u', tx_cycle, ser_cycles);
        } else {
            ssr |= SSR_TEND;
            tx_cycle = cycles-1;
        }
    }

    if ((int32) (cycles - rx_cycle) >= 0) {
        uint8 buf;
      //      printf("%10d: update_time (%10d)\n", cycles, rx_cycle);
        if (receiving) {
            /* wait for 5 ms in case there was some lag due to external
             * processes.
             */
            struct timeval timeval;
            fd_set rdfds;
            timeval.tv_sec = 0;
            timeval.tv_usec = 5000;
            FD_ZERO(&rdfds);
            FD_SET(serfd, &rdfds);
            select(serfd+1, &rdfds, NULL, NULL, &timeval);
        }
        if (read(serfd, &buf, 1) > 0 && (scr & SCR_RE)) {
#ifdef VERBOSE_SERIAL
            serd[last] = 0;
            serc[last] = cycles;
            serb[last++] = buf;
#endif
            rx_cycle = add_to_cycle('s', rx_cycle, ser_cycles);
            receiving = 1;
            if (ssr & SSR_RDRF)
                ssr |= SSR_ORER;
            else {
                if (smr & SMR_CHR)
                    buf &= 0x7f;
                rdr = buf;
                ssr |= SSR_RDRF;
            }
        } else {
            rx_cycle = cycles-1;
            receiving = 0;
        }
    }

    ser_check_next_cycle();
}

static int ser_check_irq() {
    int irqs;

    irqs = scr & ssr & (SSR_RDRF | SSR_TDRE | SSR_TEND);
#ifdef DEBUG_SERIAL
    if (irqs) printf("check_irq(%02x, %02x, %02x)\n", scr, ssr, irqs);
#endif
    if (irqs & SSR_RDRF)
        return RXI_HANDLER;
    if (irqs & SSR_TDRE)
        return TXI_HANDLER;
    if (irqs & SSR_TEND)
        return TEI_HANDLER;
    return 255;
}


static void set_SMR(uint8 val) {
    smr = val;
    ser_update_cycles();
}
static uint8 get_SMR(void) {
    return smr;
}
static void set_BRR(uint8 val) {
    brr = val;
    ser_update_cycles();
}
static uint8 get_BRR(void) {
    return brr;
}

static void set_SCR(uint8 val) {
#ifdef VERBOSE_SERIAL
    printf("%10ld: set_SCR(%02x)\n", cycles, val);
#endif
    scr = val;
    ser_check_next_cycle();
}
static uint8 get_SCR(void) {
    return scr;
}
static void set_SSR(uint8 val) {
#ifdef DEBUG_SERIAL
    printf("set_SSR(%02x)\n", val);
#endif
    if ((readssr & ~val) & SSR_TDRE) {
#ifdef VERBOSE_SERIAL
      if (scr & SCR_TE) {
            serd[last] = 1;
            serc[last] = cycles;
            serb[last++] = tdr;
      }
#endif
        ssr &= ~(SSR_TDRE | SSR_TEND);
        if ((int32)(tx_cycle - cycles) < 0) {
            if (scr & SCR_TE) {
                write(serfd, &tdr, 1);
            }
            tx_cycle = add_to_cycle('v', cycles, ser_cycles);
            ssr |= SSR_TDRE;
        }
    }
    /* clear RDRF bits in ssr, if they were read as 1 and set to 0 */
    ssr &= ~readssr | val | ~(SSR_RDRF | SSR_ORER | SSR_FER | SSR_PER);
    /* set MPBT */
    ssr ^= (ssr ^ val) & SSR_MPBT;
    ser_check_next_cycle();
}

static uint8 get_SSR(void) {
#ifdef DEBUG_SERIAL
    printf("get_SSR(%02x)\n", ssr);
#endif
    return readssr = ssr;
}
static void set_TDR(uint8 val) {
    tdr = val;
}
static uint8 get_TDR(void) {
    return tdr;
}
static uint8 get_RDR(void) {
    return rdr;
}

peripheral_ops serial = {
    id: 'S',
    reset: ser_reset,
    update_time: ser_update_time,
    check_irq: ser_check_irq,
    load_data: ser_load,
    save_data: ser_save
};

int connect_server() {
    int sockfd;
    struct sockaddr addr;
    struct sockaddr_in* addr_in = (struct sockaddr_in*) &addr;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(BRICK_BROADCAST_PORT);
    addr_in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sockfd, &addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void ser_init() {
    serfd = connect_server();
    if (serfd < 0) {
        printf("Starting server...");
        system("./ir-server");
        serfd = connect_server();
        if (serfd < 0) {
            printf ("Can't connect to server!\n");
            abort();
        }
    }
    printf("Connected to server via %d.\n", serfd);

    fcntl(serfd, F_SETFL, O_NONBLOCK);

    port[0xd8-0x88].set = set_SMR;
    port[0xd8-0x88].get = get_SMR;
    port[0xd9-0x88].set = set_BRR;
    port[0xd9-0x88].get = get_BRR;
    port[0xda-0x88].set = set_SCR;
    port[0xda-0x88].get = get_SCR;
    port[0xdb-0x88].set = set_TDR;
    port[0xdb-0x88].get = get_TDR;
    port[0xdc-0x88].set = set_SSR;
    port[0xdc-0x88].get = get_SSR;
    port[0xdd-0x88].get = get_RDR;

    register_peripheral(serial);
}
