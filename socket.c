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
 * $Id: socket.c 70 2004-04-26 06:22:48Z hoenicke $
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "socket.h"

/** \file socket.c
 * \brief socket handling routines
 *
 */


/** \brief create a server socket
 *
 * This routine is used to create the server socket for
 * the communication with the debugger.
 * \param port socket port
 * \returns socket file descriptor on success, -1 otherwise
 */
int create_server_socket(int port) {
    struct sockaddr addr;
    struct sockaddr_in* addr_in = (struct sockaddr_in*) &addr;
    int fd;
    int val = 1;
    fd = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    memset(&addr, 0, sizeof(addr));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(port);
    addr_in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, &addr, sizeof(addr)) < 0) {
	perror("bind");
	return -1;
    }
    listen(fd, 1);
    return fd;
}

/** \brief create a server socket 
 *
 * The server socket created by this routine is
 * used to communicate with the GUI.
 * \param port socket port
 * \returns socket file descriptor on success, -1 otherwise
 */

int create_anon_socket(int *port) {
    struct sockaddr addr;
    struct sockaddr_in* addr_in = (struct sockaddr_in*) &addr;
    int addrlen = sizeof(addr);
    int fd;
    int val = 1;
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	return -1;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = 0;
    addr_in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, &addr, sizeof(addr)) < 0) {
	perror("bind");
	return -1;
    }
    listen(fd, 1);

    getsockname(fd, &addr, &addrlen);
    *port = ntohs(addr_in->sin_port);
    return fd;
}

/** \brief accept a socket connection
 *
 *  This routine is used by the server to accept a
 *  client socket connection.
 * \param serverfd file descriptor of the server socket
 * \returns socket file descriptor on success, -1 otherwise
 */

int accept_socket(int serverfd) {
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    return accept(serverfd, &addr, &addr_len);
}

/** \brief create a client socket 
 *
 * The client socket created by this routine is
 * used if the emulator is connected against a server as a client.
 * \param port socket port
 * \returns socket file descriptor on success, -1 otherwise
 */

int create_client_socket(int serverport) {
    int client_fd;
    int len;
    struct sockaddr_in address;
    int result;
    int val = 1;
    
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(serverport);
    len = sizeof(address);
    result = connect(client_fd, (struct sockaddr *)&address, len);
    
    if(result == -1) {
	perror("unable to establish client socket");
	exit(1);
    }
    // printf("client:connected!\n");
    return client_fd;
}
