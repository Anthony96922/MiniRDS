/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct ctl_socket_obj_t {
	int listener_fd; /* fd of the listener */
	int current_fd; /* fd of the current connection */
	struct sockaddr_in6 my_sock;
	struct sockaddr_in6 peer_sock;
	socklen_t peer_addr_size;
	struct pollfd sock_poller;
	struct pollfd peer_poller;
	struct pollfd curr_sock_poller;
	uint8_t already_connected;
} ctl_socket_obj_t;

extern int open_ctl_socket(struct rds_obj_t *rds, uint16_t port, uint8_t proto);
extern void close_ctl_socket(struct rds_obj_t *rds);
extern void poll_ctl_socket(struct rds_obj_t *rds);
