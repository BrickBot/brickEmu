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
extern void mem_init(char *);
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
    int guiserverport = 0;
    int arg_index = 1;
	char *rom_file = NULL;
    
    for (arg_index = 1; arg_index < argc; arg_index++) {
        if (strcmp(argv[arg_index], "-d") == 0) {
            db_trap = TRAP_EXCEPTION;
            
        } else if (strcmp(argv[arg_index], "-guiserverport") == 0) {
            arg_index++;
            guiserverport = atoi(argv[arg_index]);
            printf("guiserverport=%d\n", guiserverport);
        } else if (strcmp(argv[arg_index], "-rom") == 0) {
            arg_index++;
            rom_file = argv[arg_index];
            printf("rom=%s\n", rom_file);
        } else {
            fprintf(stderr, "Unrecognized argument: %s\n", argv[arg_index]);
            fprintf(stderr, "USAGE: emu [-guiserverport port] [-d]\n");
            exit(1);
        }
    }
    
    mem_init(rom_file);
    frame_init();
    ser_init();
    db_init();
    periph_init(guiserverport);
    savefile_init();
    t16_init();
    t8_init();
        printf("BrickEmu: Preparing to Initialize Sound\n");
    sound_init();
        printf("BrickEmu: Sound Initialized\n");
    btn_init();
        printf("BrickEmu: Buttons Initialized\n");
    lcd_init();
        printf("BrickEmu: LCD Initialized\n");
    ws_init();
    ad_init();
    wdog_init();
    firm_init();
    motor_init();
    bibo_init();

        printf("BrickEmu: Initialization Complete\n");
        
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        db_trap = TRAP_EXCEPTION;
    }

    run_cpu();

    return 0;
}
