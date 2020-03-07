#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "h8300.h"
#include "peripherals.h"
#include "memory.h"

#define UPDATE_INTERVAL (100 * 16000)
#define SCALER 256

static int next_output_cycles;
static int motor_cycles;

static int      on[3];
static int last_on[3];
static int      dir[3];
static int last_dir[3];

static char motor_val;
static char  cur_analog_active;
static char      analog_active;
static char last_analog_active;

static void motor_update() {
    int dcycles = cycles - motor_cycles;
    if (motor_val & 0xc0) {
	on[0] += dcycles;
	dir[0] = (motor_val >> 6) & 3;
    }
    if (motor_val & 0x0c) {
	on[1] += dcycles;
	dir[1] = (motor_val >> 2) & 3;
    }
    if (motor_val & 0x03) {
	on[2] += dcycles;
	dir[2] = (motor_val >> 0) & 3;
    }
    motor_cycles = cycles;
}

static void motor_update_time() {
    int i;
    char out[20];

    motor_update(cycles - motor_cycles);
    if ((cycles - next_output_cycles) >= 0) {
	int analog_changed;
	next_output_cycles += UPDATE_INTERVAL;
	
	for (i = 0; i < 3; i++) {
	    on[i] /= UPDATE_INTERVAL / SCALER;
	    if (dir[i] != last_dir[i] || on[i] != last_on[i]) {
		sprintf(out, "M%1d,%1d,%d\n", i, dir[i], on[i]);
		write(periph_fd, out, strlen(out));
		last_on[i] = on[i];
		last_dir[i] = dir[i];
	    }
	    on[i] = 0;
	    dir[i] = 0;
	}
	    
	analog_changed = analog_active ^ last_analog_active;
	if (analog_changed) {
	    sprintf(out, "A%d\n", analog_active & 7);
	    write(periph_fd, out, strlen(out));
	    last_analog_active = analog_active;
	}
	analog_active = cur_analog_active;
    }

    if ((next_timer_cycle - next_output_cycles) > 0)
	next_timer_cycle = next_output_cycles;
}

void set_analog_active(unsigned char val) {
    cur_analog_active = val;
    analog_active |= val;
}

void set_motor(unsigned char val) {
    motor_update();
    motor_val = val;
}

peripheral_ops motor = {
    id: 'M',
    update_time: motor_update_time
};

void motor_init() {
    register_peripheral(motor);
}
