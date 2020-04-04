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
 * $Id: ir-server.c 97 2004-08-17 16:33:38Z hoenicke $
 */

#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BRICK_BROADCAST_PORT 50637
#define MAX_CLIENTS 1000
#undef FAST

#ifdef FAST
#define SLOT_LEN  10
#define BAUD_RATE 4800
#else
#define SLOT_LEN  11
#define BAUD_RATE 2400
#endif

int sockets[MAX_CLIENTS];

static void check_exit(void) {
    int i;
    for (i = 1; i < MAX_CLIENTS; i++) {
        if (sockets[i] != -1)
            return;
    }
    exit(0);
}

int main() {
    fd_set rfds;
    struct sigaction sigact;
    struct sockaddr addr;
    struct sockaddr_in* addr_in = (struct sockaddr_in*) &addr;
    int val = 1;
    char buff[4096];
    
    int i;
    memset(sockets, -1, sizeof(sockets));
    sockets[0] = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sockets[0], SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    memset(&addr, 0, sizeof(addr));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(BRICK_BROADCAST_PORT);
    addr_in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sockets[0], &addr, sizeof(addr)) < 0) {
      perror("bind");
      return 1;
    }
    listen(sockets[0], 10);
    printf("Server started.\n");
    if (fork())
      return 0;

    sigact.sa_handler = SIG_IGN;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sigact, NULL);
    /* close stdin/out/err */
    close(0);
    close(1);
    close(2);
    
    while (1) {
        int max = 0;
        FD_ZERO(&rfds);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (sockets[i] >= 0) {
                if (sockets[i] >= max)
                    max = sockets[i] + 1;
                FD_SET(sockets[i], &rfds);
            }
        }
        select(max, &rfds, NULL, NULL, NULL);
        if (FD_ISSET(sockets[0], &rfds)) {
            socklen_t addr_len = sizeof(addr);
            int client;
            memset(&addr, 0, sizeof(addr));
            client = accept(sockets[0], &addr, &addr_len);
            if (client == -1)
                continue;

            for (i = 1; i < MAX_CLIENTS; i++) {
                if (sockets[i] == -1) {
                    sockets[i] = client;                    
                    break;
                }
            }
            if (i == MAX_CLIENTS) {
                /* too many open connections */
                close(client);
            }
        }
        for (i = 1; i < MAX_CLIENTS; i++) {
            if (sockets[i] >= 0 && FD_ISSET(sockets[i], &rfds)) {
                int len = read(sockets[i], buff, sizeof(buff));
                if (len <= 0) {
                    close(sockets[i]);
                    sockets[i] = -1;
                    check_exit();
                    continue;
                } else {
                    int j;
                    for (j = 1; j < MAX_CLIENTS; j++) {
                                // write to other sockets
                                if (i != j && sockets[j] >= 0) {
                                        if (write(sockets[j], buff, len) < 0) {
                                                close(sockets[j]);
                                                sockets[j] = -1;
                                                check_exit();
                                        }
                                }
                        }
                }
#if 0
                usleep(1000000 * SLOT_LEN / BAUD_RATE * len);
#endif
#if 0
                {
                    struct timeval current_time;
                    gettimeofday(&current_time, NULL);
                    if (next_time.tv_sec > current_time.tv_sec
                                        || (next_time.tv_sec == current_time.tv_sec
                                    && next_time.tv_usec > current_time.tv_usec)) {
                                usleep((next_time.tv_sec - current_time.tv_sec)
                               * 1000000
                               + next_time.tv_usec - current_time.tv_usec);
                    }
                    gettimeofday(&current_time, NULL);
                    next_time = current_time;
                    next_time.tv_usec += 1000000 * SLOT_LEN * len / BAUD_RATE;
                    if (next_time.tv_usec > 1000000) {
                                next_time.tv_sec++;
                                next_time.tv_usec -= 1000000;
                    }
                }
#endif
                // echo back to self
                if (write(sockets[i], buff, len) < 0) {
                    close(sockets[i]);
                    sockets[i] = -1;
                    check_exit();
                }
            }
        }
    }
}
