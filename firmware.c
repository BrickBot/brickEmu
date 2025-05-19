#include <stdio.h>
#include <unistd.h>
#include "memory.h"
#include "peripherals.h"
#include "coff.h"
#include "symbols.h"

#define MAX_PATHNAME_LEN 4096

static void firm_null() {
}

static int firm_check_irq() {
    return 255;
}

static void firm_read_fd(int fd) {
    FILE *file;
    char filename[MAX_PATHNAME_LEN+1];
    int entry;

    /* read in 3 bytes: btnid val newline */
    int len = 0;
    do {
        len += read(fd, filename + len, 1);
    } while (filename[len-1] != '\n' && filename[len-1] != '\r');
    filename[len-1] = 0;

    if (memory[0xee5e] != 0xd) {
        fprintf (stderr, "RCX not ready to receive firmware\n");
        return;
    }

    /* Open file */
    if ((file = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "%s: failed to open\n", filename);
        return;
    }

    if (coff_init(file)) {
        entry = coff_read(file, 0x8000);
        coff_symbols(file, 0x8000);
    } else {
        fseek(file, 0, SEEK_SET);
        if (!srec_init(file) || (entry = srec_read(file, 0x8000)) < 0) {
            fprintf (stderr, "Can't detect file format\n");
            return;
        }
    }

    if (entry == 0) {
        fprintf (stderr, "Error loading firmware\n");
        return;
    }

    /* firmware should be loaded, now tell ROM about it. */
    memory[0xee82] = 0;  /* run firmware holdoff timer */
    memory[0xee83] = 2;  /* run firmware holdoff timer */
    memory[0xee62] = (entry >> 8);
    memory[0xee63] = (entry & 0xff);  /* firmware entry point */
    memory[0xee5e] = 0x13; /* run firmware */
    memory[0xef06] = 8;  /* no more updates */
    
    fprintf (stderr, "BrickEmu: Firmware loaded to %04x: %s\n", entry, filename);
}

static peripheral_ops firmware = {
    id: 'F',
    read_fd: firm_read_fd,
    reset: firm_null,
    update_time: firm_null,
    check_irq: firm_check_irq
};

void firm_init() {
    register_peripheral(firmware);
}
