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
 * $Id: lcd.c 102 2005-02-02 01:56:58Z hoenicke $
 */

#include <unistd.h>
#include <string.h>
#include "h8300.h"
#include "memory.h"
#include "peripherals.h"

/*
 * LCD i2c bus constants
 */
#define LCD_DEV_ID      0x7c

#define I2C_WRITE       0
#define I2C_READ        1

/* #define VERBOSE_LCD */

static uint8 lcd_data[12];
static int8 lcd_mode, lcd_ptr, lcd_subaddr;
static int8 i2c_state;
static uint8 i2c_data, i2c_bitnr;
static uint8 port6;

extern void set_analog_active(unsigned char val);

typedef struct {
    uint8 data[12];
    int8  mode, ptr, subaddr;
    int8  i2c_state, i2c_data, i2c_bitnr;
    uint8 port6;
} lcd_save_type;

static int lcd_save(void *buffer, int maxlen) {
    lcd_save_type *data = buffer;
    memcpy(data->data, lcd_data, sizeof(lcd_data));
    data->mode = lcd_mode;
    data->ptr = lcd_ptr;
    data->subaddr = lcd_subaddr;
    data->i2c_state = i2c_state;
    data->i2c_data = i2c_data;
    data->i2c_bitnr = i2c_bitnr;
    data->port6 = port6;
    return sizeof(*data);
}

static void lcd_load(void *buffer, int len) {
    int i;
    char out[20];
    lcd_save_type *data = buffer;

    memcpy(lcd_data, data->data, sizeof(lcd_data));
    lcd_mode = data->mode;
    lcd_ptr = data->ptr;
    lcd_subaddr = data->subaddr;
    i2c_state = data->i2c_state;
    i2c_data = data->i2c_data;
    i2c_bitnr = data->i2c_bitnr;
    port6 = data->port6;

    for (i = 0; i < 12; i++) {
        sprintf(out, "L%d,%02x\n", i, lcd_data[i]);
        write(periph_fd, out, strlen(out));
    }
    sprintf(out, "L99,%d\n", lcd_mode & 0x08 ? 1 : 0);
    write(periph_fd, out, strlen(out));

    sprintf(out, "A%d\n", port6 & 7);
    write(periph_fd, out, strlen(out));
}

static void lcd_display(int ptr, int data)
{
    char out[20];
    if (lcd_data[ptr] == data)
        return;
    lcd_data[ptr] = data;
    sprintf(out, "L%d,%02x\n", ptr, data);
    wait_peripherals();
#ifdef VERBOSE_LCD
    printf ("%10d: %s", cycles, out);
#endif
    write(periph_fd, out, strlen(out));
}

static void lcd_enable(int data)
{
    char out[20];
    sprintf(out, "L99,%d\n", data);
    wait_peripherals();
#ifdef VERBOSE_LCD
    printf ("%10d: %s", cycles, out);
#endif
    write(periph_fd, out, strlen(out));
}

static void set_port6(uint8 val) {
    if (!(val & 0x20))
        goto out;
    if (!(port6 & 0x20) && i2c_state >= 0) {
        if (++i2c_bitnr < 8) {
            i2c_data |= ((val+val) & 0x80) >> i2c_bitnr;
        } else {
#ifdef VERBOSE_LCD
            printf("LCD: %d %d %02x\n", i2c_state, lcd_ptr, i2c_data);
#endif
            switch (i2c_state) {
            case -1:
                break;
            case 0: /* start condition */
                if (i2c_data == (LCD_DEV_ID | I2C_WRITE))
                    i2c_state = 1;
                else
                    i2c_state = -1;
                break;
            case 1: /* write command */
                switch ((i2c_data & 0x60)) {
                case 0x40:
                    /* mode set */
                    if ((lcd_mode ^ i2c_data) & 0x08)
                        lcd_enable(i2c_data & 0x08 ? 1 : 0);
                    lcd_mode = i2c_data & 0x1f;
                    break;
                case 0x00:
                    /* Load Data pointer */
                    lcd_ptr = i2c_data & 0x1f;
                    break;
                case 0x60:
                    /* load subaddr */
                    lcd_subaddr = i2c_data & 0x7;
                    break;
                case 0x30:
                    /* blink / bank select, not implementd */
                    /*XXX*/
                    break;

                }
                if (!(i2c_data & 0x80))
                    /* last command*/
                    i2c_state = 2;
                break;
            case 2: /* data value */
                /*XXX handle other than 1:4 MUX? */
                if (lcd_subaddr == 0) {
                    lcd_display((lcd_ptr>>1), i2c_data);
                    lcd_ptr += 2;
                }
                break;
            }
            i2c_bitnr = -1;
            i2c_data = 0;
        }
    } else if ((port6 & ~val & 0x40)) {
        /* CLOCK is high and data goes low.  this is i2c start */
#ifdef VERBOSE_LCD
        printf("LCD start\n");
#endif
        i2c_state = 0;
        i2c_bitnr = -1;
        i2c_data = 0;
    } else if ((~port6 & val & 0x40)) {
        /* CLOCK is high and data goes high.  this is i2c stop */
        i2c_state = -1;
#ifdef VERBOSE_LCD
        printf("LCD stop\n");
#endif
    }

    if ((port6 ^ val) & 7) {
        set_analog_active(val & 7);
    }
out:
    port6 = val;
}

peripheral_ops lcd = {
    id: 'L',
    save_data: lcd_save,
    load_data: lcd_load
};

void lcd_init() {
    register_peripheral(lcd);
    port[0xbb-0x88].set = set_port6;
    i2c_state = -1;
}
