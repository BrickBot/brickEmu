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
 * $Id: main.c 336 2006-04-01 19:27:09Z hoenicke $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "h8300.h"
#include "peripherals.h"


/** \file main.c
 * \brief main program to start emulator and gui.
 *
 * 
 */


#define TRAP_EXCEPTION 5

extern void periph_init(int port);
extern void savefile_init(void);
extern void mem_init(void);
extern void frame_init(void);
extern void lcd_init(void);
extern void t16_init(void);
extern void t8_init(void);
extern void sound_init(void);
extern void btn_init(void);
extern void ws_init(void);
extern void ser_init(void);
extern void db_init(void);
extern void ad_init(void);
extern void wdog_init(void);
extern void firm_init(void);
extern void motor_init(void);
extern void bibo_init(void);


/** \brief start emulator and GUI
 *
 * Initializes all peripherals, 
 * checks if debugging is wanted
 * and starts the emulated H8300 CPU.
 * \param argc 0 or 1 
 * \param argv argv[1] = "-d" to wait for debugger.
 * \return 0 always.
 */
int main(int argc, char**argv) {
    int serverport = 0;
    int arg_index = 1;
    
    while (arg_index < argc) {
	if (strcmp(argv[arg_index], "-d") == 0) {
	    db_trap = TRAP_EXCEPTION;
	    arg_index++;
	    
	} else if (strcmp(argv[arg_index], "-serverport") == 0) {
	    arg_index++;
	    serverport = atoi(argv[arg_index]);
	    printf("serverport=%d\n", serverport);
	    arg_index++;
	} else {
	    fprintf(stderr, "USAGE: emu [-serverport port] [-d]\n");
	    exit(1);
	}
    }
   
    mem_init();
    frame_init();
    ser_init();
    db_init();
    periph_init(serverport);
    savefile_init();
    t16_init();
    t8_init();
    sound_init();
    btn_init();
    lcd_init();
    ws_init();
    ad_init();
    wdog_init();
    firm_init();
    motor_init();
    bibo_init();

    if (argc > 1 && strcmp(argv[1], "-d") == 0)
	db_trap = TRAP_EXCEPTION;
    run_cpu();
    return 0;
}
