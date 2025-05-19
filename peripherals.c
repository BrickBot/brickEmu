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
 * $Id: peripherals.c 322 2006-03-17 13:38:42Z hoenicke $
 */

#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h> /* for htonx/ntohx */
#include "h8300.h"
#include "socket.h"
#include "memory.h"
#include "peripherals.h"
#include "frame.h"

extern int monitorport;
extern int debuggerfd;

/** \file peripherals.c
 * \brief routines controlling interaction with the environment
 *
 * Routines to start the GUI program and deal with timing
 * of emulator and peripherals.
 */

/* #define VERBOSE_IRQ */

#define MAX_AUTONOMOUS_CYCLES (CYCLES_PER_USEC * 100000)

/** \brief bit 7 of SYSCR register (Software Standby)
 *
 */
#define SYSCR_SSBY  0x80

/** \brief bits 6-4 of SYSCR register (Standby Timer Select)
 *
 */
#define SYSCR_STS   0x70

/** \brief bit 3 of SYSCR register (External Reset)
 *
 */
#define SYSCR_XRST  0x08

/** \brief bit 2 of SYSCR register (NMI Edge)
 *
 */
#define SYSCR_NMIEG 0x04

/** \brief bit 0 of SYSCR register (RAM Enable)
 *
 */
#define SYSCR_RAME  0x01

/** \brief counter that holds the number of real-time usecs corresponding to
 * lastcycles
 *
 * This contains the number of real-time usecs since EPOCH modulo 2^32 that
 * corresponds to the time when processor had executed lastcycles cycles.
 */
static cycle_count_t lastusecs;

/** \brief counter that holds the last number of cycles.
 * 
 * This is updated in synchronized_time together with lastusecs.
 */
static cycle_count_t lastcycles;

/** \brief time in usecs for the next sleep
 *
 * Only if lastusecs > nextsleep we actually start sleeping and/or wait
 * for input.  For performance reasons we only sleep if more than a
 * millisecond simulated time has passed.
 */
static cycle_count_t nextsleep;


/** \brief System Control Register (SYSCR)
 *
 * 8 Bit register controlling the operation of the chip
 */
static uint8  syscr;

/** \brief The number of real-time usecs when simulation started.
 * 
 * This holds the number of usecs since EPOCH modulo 2^32 when simulation
 * started.  When CPU is freezed, startusecs is increased by the number of
 * freezed usecs.
 *
 * In any case the real-time that matches the current simulated time is
 * startusecs + (cycles / CYCLES_PER_USECS) * SLOW_DOWN.
 *
 * startusecs is only used for debugging purposes.  It can be removed later.
 */
static cycle_count_t startusecs;


/** \brief flag to mark the CPU as stopped
 *
 * When the CPU is stopped caused by debugger or sleep instruction in software
 * suspend mode, the value is set to 1 to mark the CPU as stopped.  The CPU
 * doesn't perform any cycles while stopped.  When the CPU continues
 * processing, the value is set back to 0.
 */
static int stopped;

/** \brief flag to mark the CPU as sleeping
 * When the CPU is sleeping caused by a sleep instruction,
 * the value is set to 1 to mark the CPU as sleeping. When 
 * the CPU continues processing, the value is set back to 0.
 */
static int sleeping;

/** \brief peripheral socket file descriptor
 * File descriptor of the socket used for the communication
 * with the peripherals.
 */
int periph_fd;

/** \brief array to hold all peripherals 
 *
 * This array is used to register the different
 * kinds of peripherals (buttons, sensors etc.)
 */
peripheral_ops peripherals[100];

/** \brief number of registered peripherals
 * This counter is incremented each time a
 * peripheral is registered.
 */
int num_peripherals;

/** \brief file descriptor set for the communication with the peripherals
 * Set of file descriptors used by SELECT to check for peripheral events.
 *
 */
static fd_set rdfds;

/** \brief synchronize the emulator’s time with the real time
 *
 * The routine checks how many usecs of simulated time have gone by since the
 * last by subtracting the last number of cycles from the current number of
 * cycles and dividing the result by CYCLES PER USEC.  
 *
 * The result is added to the internal usec counter lastusecs. If more then
 * one milli second has gone by since the last call or if the CPU is stopped,
 * we start sleeping so that simulated time equals real time and also check for
 * input from peripheral sensors, buttons etc.
 */
static void synchronize_time(void) {
    struct timeval timeval;
    cycle_count_t  tosleep;
    int   maxfd;
    
    /* The result of this operation is expected to be positive
     * (c.f. the "+=" assignment statements that then follow this one)
     */
    cycle_count_t usecs = (cycles - lastcycles) / CYCLES_PER_USEC;

    /* With this trick we don't loose precision: We update both lastusecs
     * and lastcycles as if exactly usecs micro seconds have passed since
     * the last call.
     */
    lastusecs += usecs * SLOW_DOWN;
    lastcycles += usecs * CYCLES_PER_USEC;

    if ((lastusecs > nextsleep) || stopped) {
        gettimeofday(&timeval, NULL);


        cycle_count_t timeval_usec = timeval.tv_sec * 1000000 + timeval.tv_usec;
        tosleep = (lastusecs > timeval_usec) ? (lastusecs - timeval_usec) : 0;
#ifdef DEBUG_TIMER
        else
            printf("simulated time: %8.2f  (%" CYCLE_COUNT_F " cycles) sleeping %" CYCLE_COUNT_F " usec\n", 
                   (lastusecs-startusecs)/1000000.0, lastcycles, tosleep);
#endif
        
        FD_SET(periph_fd, &rdfds);
        maxfd = periph_fd + 1;
        FD_SET(debuggerfd, &rdfds);
        if (debuggerfd >= maxfd)
            maxfd = debuggerfd + 1;
        timeval.tv_sec = 0;
        timeval.tv_usec = tosleep;
        if (select(maxfd, &rdfds, NULL, NULL, 
                   stopped ? NULL : &timeval) > 0) {
            if (FD_ISSET(periph_fd, &rdfds)) {
                char id;
                int i;
                if (read(periph_fd, &id, 1) <= 0) {
                    printf("GUI closed!\n");
                    frame_dump_profile();
                    exit(0);
                }
                for (i = 0; i < num_peripherals; i++) {
                    if (peripherals[i].id == id)
                        peripherals[i].read_fd(periph_fd);
                }
            }
            if (debuggerfd >= 0 && FD_ISSET(debuggerfd, &rdfds)) {
                db_handlefd();
            }
        }
        nextsleep = lastusecs + 1000;
    }
}

 
/** \brief completely freeze the CPU until cont_time is called
 *
 * This routine makes lastusecs and startusecs relative to current time and
 * marks the CPU as stopped.  In this mode the simulated time will stay
 * constant and no cycle is executed until cont_time is called.
 *
 * stop_time and cont_time may be nested.
 */

void stop_time(void) {
    if (!stopped++) {
        struct timeval current_time;
        gettimeofday(&current_time, NULL);

        cycle_count_t timeval_usec = (current_time.tv_sec * 1000000 + current_time.tv_usec);
        lastusecs -= timeval_usec;
        startusecs -= timeval_usec;
    }
}

/** \brief wakes up the CPU that was previously stopped with stop_time.
 * as running
 *
 * The routine decreases stopped.  If the CPU should wakeup,
 *  it makes lastusecs and startusecs absolute again.
 */

void cont_time(void) {
    if (!--stopped) {
        struct timeval current_time;
        gettimeofday(&current_time, NULL);

        cycle_count_t timeval_usec = (current_time.tv_sec * 1000000 + current_time.tv_usec);
        lastusecs += timeval_usec;
        startusecs += timeval_usec;
        nextsleep = lastusecs;
    }
}


/** \brief make processor time match real time and update peripheral times

 *
 * Uses synchronize time to make the processors time match real time.
 * Then updates the peripherals times using their update_time()-routine, 
 * which for some peripherals (e.g. buttons) just does nothing.
 */

void wait_peripherals(void)
{
    int i;
    synchronize_time();

    for (i = 0; i < num_peripherals; i++) {
        if(peripherals[i].update_time)
            peripherals[i].update_time();
    }
}

/** \brief perform a reset
 *
 * Reset the CPU, perform a reset also for all peripherals
 * and set the program counter to the value stored in memory[0], memory[1]
 */

void do_reset(void) {
    int i;
    irq_disabled_one = 1;
    ccr = 0x80;
    for (i = 0; i < num_peripherals; i++) {
        if(peripherals[i].reset)
            peripherals[i].reset();
    }
    /* Clear all symbols and reread rom file and its symbols. */
    symbols_removeall();
    read_rom();
    wait_peripherals();
    cycles++;
    db_trap = 0;
    pc = GET_WORD_CYCLES(0);
}

/** \brief check for interrupt
 *
 * Synchronize processor with real time, check for 
 * interrupts wanting to fire. If the latter is the 
 * case, check for the interrupt mask bit; if set,
 * initiate interrupt handling.
 * \returns 1 if an interrupt fired, 0 otherwise.
 */

int check_irq(void) {
    int i;
    int selirq;

    /* reset next_cycle */
    next_timer_cycle = add_to_cycle('T', cycles, MAX_AUTONOMOUS_CYCLES);
    next_nmi_cycle = add_to_cycle('I', cycles, MAX_AUTONOMOUS_CYCLES);

    /* Make processor time match real time */
    wait_peripherals();

    selirq = 255;
    /* Check whether an interrupt wants to fire */
    for (i = 0; i < num_peripherals; i++) {
        int irq;

        if(peripherals[i].check_irq) {
            irq = peripherals[i].check_irq();
            if (irq < selirq)
                selirq = irq;
        }
    }

    if (selirq < (ccr & 0x80 ? 4 : 255)) {
        uint16 sp;
        cycle_count_t irqcycles, tmpcycles;
        /* IRQ was selected, fire it */
#ifdef VERBOSE_IRQ
        printf("%" CYCLE_COUNT_F ": IRQ %d fired!\n", cycles, selirq);
#endif
        if (selirq == 0) {
            /* reset was caused, probably by watchdog.
             * next_nmi_cycle is the cycle when reset is finished.
             */
            cycles = next_nmi_cycle;
            do_reset();
            return 1;
        }

        irqcycles = cycles;
        GET_WORD_CYCLES(pc); /* simulate lookahead */
        sp = GET_REG16(7)-4;
        SET_WORD_CYCLES(sp + 2, pc);
        SET_WORD_CYCLES(sp, ((ccr << 8) |  ccr));
        SET_REG16(7, sp);
        /* disable IRQ */
        ccr |= 0x80;
        pc = GET_WORD_CYCLES(selirq * 2);
        tmpcycles = cycles;
        cycles = irqcycles;
        frame_begin(sp + 2, 1);
        cycles = tmpcycles;
        return 1;
    }
    return 0;
}

/** \brief handle CPU traps
 *
 * This is called by the emulator whenever a trap (breakpoint, 
 * singlestep, trap instruction) occured.  This halts the CPU
 * and waits until the debugger or GUI reset or continue the 
 * CPU.
 */
void periph_handletrap(void) {
    /* freeze CPU */
    stop_time();
    db_handletrap();
    
    while (db_trap) {
        /* In this case synchronize time wil just wait for
         * input from the GUI or debugger
         */
        synchronize_time();
    }
    cont_time();
}

/** \brief send CPU to sleep
 *
 * The routine is used to emulate the sleep-Instruction of the
 * CPU. The emulators timer variables are updated using stop_time
 * and cont_time.
 */
void cpu_sleep(void) {
    int i;
    sleeping = 1;
#ifdef DEBUG_TIMER
    printf("SLEEP START: %" CYCLE_COUNT_F ", pc=%04x\n", cycles, pc);
#endif
    if (!(syscr & SYSCR_SSBY)) {
        do {
            if (db_trap) {
                pc -= 2;
                break;
            }
            cycles = (ccr & 0x80 ? next_nmi_cycle : next_timer_cycle);
        } while (!check_irq() && sleeping);

    } else {
        /* freeze CPU */
        stop_time();

        do {
            if (db_trap) {
                pc -= 2;
                break;
            }
        } while (!check_irq() && sleeping);
        
        cont_time();
    }


    if ((syscr & SYSCR_SSBY)) {
        int32 cycles_to_add = 0;
        switch ((syscr & SYSCR_STS)) {
        case 0x00:
            cycles_to_add = 8192;
            break;
        case 0x10:
            cycles_to_add = 16384;
            break;
        case 0x20:
            cycles_to_add = 32768;
            break;
        case 0x30:
            cycles_to_add = 65536;
            break;
        case 0x40:
        case 0x50:
            cycles_to_add = 131072;
            break;
        }
        cycles = add_to_cycle('C', cycles, cycles_to_add );
        
        for (i = 0; i < num_peripherals; i++) {
            if (peripherals[i].reset)
                peripherals[i].reset();
        }
    }
#ifdef DEBUG_TIMER
    printf("SLEEP STOPS: %" CYCLE_COUNT_F ", pc=%04x\n", cycles, pc);
#endif
    sleeping = 0;
}

/** \brief add to cycle, handle timing wraparound
 *
 * The routine is used to add to the cycle count and handle when
 * timing routine values wrap around.
 */

cycle_count_t add_to_cycle(char id, cycle_count_t base_value, int32 value_to_add) {
    cycle_count_t new_value = base_value + value_to_add;
    
    /**
     * TODO: Check for and handle wraparound
     */
    
    return new_value;
}

/** \brief register a peripheral 
 *
 * Register a new peripheral.
 * 
 */

void register_peripheral(peripheral_ops ops) {
    peripherals[num_peripherals++] = ops;
}

/** \brief set the value of the SYSCR register 
 *
 */
static void set_syscr(uint8 value) {
    value = value ^ ((syscr ^ value) & 0x0A);
#ifdef VERBOSE_SYSCR
    printf("peripherals.c: set_syscr(%02x)\n", value);
#endif
    syscr = value;
}

/** \brief get the value of the SYSCR register 
 *
 */
static uint8 get_syscr(void) {
    return syscr;
}

typedef struct {
    int32  wait_states;
    cycle_count_t  cycles, next_timer_cycle, next_nmi_cycle;
    int32  db_trap;
    uint8  reg[16];
    uint16 pc;
    uint8  ccr;
    uint8  syscr;
} periph_save_type;

static int periph_save(void *buffer, int maxlen) {
    periph_save_type *data = buffer;
    memcpy(data->reg, reg, sizeof(data->reg));
    data->pc = htons(sleeping ? pc - 2 : pc);
    data->ccr = ccr;
    data->wait_states = htonl(wait_states);
    data->cycles = htonl(cycles);
    data->next_timer_cycle = htonl(next_timer_cycle);
    data->next_nmi_cycle = htonl(next_nmi_cycle);
    data->syscr = syscr;
    data->db_trap = htonl(db_trap);
    return sizeof(*data);
}

static void periph_load(void *buffer, int len) {
    periph_save_type *data = buffer;

    memcpy(reg, data->reg, sizeof(data->reg));
    pc = ntohs(data->pc);
    ccr = data->ccr;
    syscr = data->syscr;
    wait_states = ntohl(data->wait_states);
    cycles = ntohl(data->cycles);
    next_timer_cycle = ntohl(data->next_timer_cycle);
    next_nmi_cycle = ntohl(data->next_nmi_cycle);
    db_trap = ntohl(data->db_trap);
    sleeping = 0;
    lastcycles = cycles;
    lastusecs = 0;
}

static void periph_read_fd(int fd) {
    char cmd;
    read(fd, &cmd, 1);
    switch (cmd) {
    case 'R':
        do_reset();
        sleeping = 0;
        break;
    case 'D': 
        {
            char buf[20];
            int len = sprintf(buf, "PD%d\n", monitorport);
            write(fd, buf, len);
            break;
        }
    }
}

peripheral_ops periph = {
    id: 'P',
    read_fd: periph_read_fd,
    save_data: periph_save,
    load_data: periph_load
};

/** \brief start GUI program
 *
 * 1. Create a server socket for communication with the GUI\n
 * 2. Read out environment variable "BRICKEMU" and start
 * the specified GUI program, if $BRICKEMU is set, otherwise
 * start the default program GUI.tcl\n
 * 3. Accept a socket connection from the GUI program
 */
void periph_init(int guiserverport) {
    struct timeval current_time;
    int server_fd;
    int guiport;
    char *gui;
    char cmd[1024];

    if (guiserverport == 0) {
        /* the emulator is the server for the gui,
           create the server socket, start the gui
           and let the gui connect                   */
        server_fd = create_anon_socket(&guiport);
        if (server_fd < 0) {
            printf("Can't create socket!\n");
            exit(1);
        }
        gui = getenv("BRICKEMU_GUI");
        if (!gui)
            gui = "wish GUI.tcl";
        snprintf(cmd, sizeof(cmd),"%s %d &", gui, guiport);
        printf("Starting GUI: %s\n", cmd);
        system(cmd);
        periph_fd = accept_socket(server_fd);
      
    } else {

        printf("Connecting to GUI-Server at localhost port %d.\n", 
               guiserverport);
        periph_fd = create_client_socket(guiserverport);
    }
    
    FD_ZERO(&rdfds);

    cycles = lastcycles = 0;
    
    gettimeofday(&current_time, NULL);
    lastusecs = (current_time.tv_sec * 1000000 + current_time.tv_usec);
    nextsleep = startusecs = lastusecs;
    
    port[0xc4 - 0x88].get = get_syscr;
    port[0xc4 - 0x88].set = set_syscr;
    syscr = 0x0b;
    register_peripheral(periph);
}
