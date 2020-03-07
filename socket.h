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
 * $Id: socket.h 71 2004-04-26 07:24:37Z hoenicke $
 */

#ifndef _SOCKET_H_
#define  _SOCKET_H_

#include "types.h"

extern int create_server_socket(int port);
extern int create_anon_socket(int *port);
extern int accept_socket(int serverfd);
extern int create_client_socket(int serverport);

#endif
