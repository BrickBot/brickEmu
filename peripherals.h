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
 * $Id: peripherals.h 313 2006-03-15 21:09:32Z hoenicke $
 */

#ifndef _PERIPHERALS_H_
#define  _PERIPHERALS_H_

#include <stdio.h>

/** \file peripherals.h
 * \brief routines controlling interaction with the environment
 *
 * Routines to start the GUI program and deal with timing
 * of emulator and peripherals.
 */


/** \brief struct to hold a peripheral type with associated routines
 * 
 * This struct holds information for a peripheral, identified by
 * it's id and associated routines as function pointers. Depending
 * on the peripheral type, not all function pointers are assigned
 * real routines. E.g a button doesn't need load/save-capability.
 */

typedef struct peripheral_ops {
    char id;
    void (*reset) (void);
    void (*update_time) (void);
    void (*read_fd) (int fd);
    int  (*check_irq) (void);
    int  (*save_data) (void *data, int maxlen);
    void (*load_data) (void *data, int len);
} peripheral_ops;

/** \brief The number cycles per micro second.
 *
 * This constant specifies the cycles per micro second.  If you increase this
 * everything on the brick will run faster.
 */
#define CYCLES_PER_USEC 16

/** \brief The slow down
 *
 * This constant specifies the ratio of simulated time to real time.  It
 * must be a simple fraction a/b without parenthesis.
 */
#define SLOW_DOWN 1

/** \brief socket file descriptor for communication with peripherals
 * 
 */
extern int periph_fd;
/** \brief retain current system time when CPU starts sleeping and mark CPU as stopped
 *
 * The routine is called by cpu_sleep. It sets the stopusecs
 * to the current time and sets stopped to 1, indicating the CPU
 * is sleeping.
 */
extern void stop_time(void);

/** \brief set stopusecs to the amount time the CPU was sleeping and mark CPU as running
 *
 * The routine is called by cpu_sleep. It sets the stopusecs
 * to the current time and sets stopped to 1, indicating the CPU
 * is sleeping.
 */
extern void cont_time(void);
/** \brief make processor time match real time and update peripheral times
 *
 * Uses synchronize time to make the processors time match real time.
 * Than updates the peripherals times using their update_time()-routine, 
 * which for some peripherals (e.g. buttons) just does nothing.
 */
extern void wait_peripherals(void);
/** \brief perform a reset
 *
 * Reset the CPU, perform a reset also for all peripherals
 * and set the program counter to the value stored in memory[0], memory[1]
 */
extern void do_reset(void);
/** \brief check for interrupt
 *
 * Synchronize processor with real time, check for 
 * interrupts wanting to fire. If the latter is the 
 * case, check for the interrupt mask bit; if set,
 * initiate interrupt handling.
 * \returns 1 if an interrupt fired, 0 otherwise.
 */
extern int check_irq(void);


/** \brief handle CPU traps
 *
 * This is called by the emulator whenever a trap (breakpoint, 
 * singlestep, trap instruction) occured.  This halts the CPU
 * and waits until the debugger or GUI reset or continue the 
 * CPU.
 */
extern void periph_handletrap(void);

/** \brief send CPU to sleep
 *
 * The routine is used to emulate the sleep-Instruction of the
 * CPU. The emulators timer variables are updated using stop_time
 * and cont_time.
 */
extern void cpu_sleep(void);

/** \brief add to cycle, handle timing wraparound
 *
 * The routine is used to add to the cycle count and handle when
 * timing routine values wrap around.
 */
extern long long add_to_cycle(char, long long, int32);


/** \brief register a peripheral 
 *
 * Register a new peripheral.
 * 
 */
extern void register_peripheral(peripheral_ops ops);

/**
 * Handle special nop opcodes to print debugging messages that
 * appear in brickOS programs.
 */
extern void debug_printf(void);

#endif
