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

/*
 * this is for manipulating the RDS data over a network connection
 *
 */

#include "common.h"
#include "ascii_cmd.h"
#include "net.h"

/*
 * Opens a socket to be used to control the RDS coder.
 */

int open_ctl_socket(struct rds_obj_t *rds, uint16_t port, uint8_t proto) {
	ctl_socket_obj_t *sock_obj = rds->ctl_sock;
	struct in6_addr my_addr;
	unsigned char listen_addr[sizeof(struct in6_addr)];

	/*
	 * 1 = tcp
	 * 0 = udp
	 */
	sock_obj->listener_fd = socket(
		AF_INET6,
		proto ? SOCK_STREAM : SOCK_DGRAM, 0
	);
	if (sock_obj->listener_fd == -1) return -1;

	/* use ipv6 sock stuct to support both v4 and v6 */
	memset(&sock_obj->my_sock, 0, sizeof(struct sockaddr_in6));
	memset(&my_addr, 0, sizeof(struct in6_addr));
	memset(&sock_obj->peer_sock, 0, sizeof(struct sockaddr_in6));
	memset(listen_addr, 0, sizeof(struct in6_addr));

	/* configure listening address and port */
	sock_obj->my_sock.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "::", &listen_addr); /* listen on both v4 and v6 */
	memcpy(my_addr.s6_addr, listen_addr, sizeof(struct in6_addr));
	sock_obj->my_sock.sin6_addr = my_addr;
	sock_obj->my_sock.sin6_port = htons(port);

	/* setup the socket */
	if (bind(sock_obj->listener_fd, (struct sockaddr *)&sock_obj->my_sock,
		sizeof(struct sockaddr_in6) == -1))
		return -1;

	/* start listening (accept only 1 connection) */
	listen(sock_obj->listener_fd, 1);

	/* setup the socket poller */
	sock_obj->sock_poller.fd = sock_obj->listener_fd;
	sock_obj->sock_poller.events = POLLIN;

	sock_obj->curr_sock_poller.events = POLLIN;

	/* setup the command poller events */
	sock_obj->peer_poller.events = POLLIN;

	/* peer socket */
	sock_obj->peer_addr_size = sizeof(struct sockaddr_in6);

	return 0;
}

/*
 * Polls the current socket, and if a command is received,
 * calls process_ascii_cmd.
 */
void poll_ctl_socket(struct rds_obj_t *rds) {
	ctl_socket_obj_t *sock_obj = rds->ctl_sock;
	static char pipe_buf[CTL_BUFFER_SIZE];
	static char cmd_buf[CMD_BUFFER_SIZE];
	char *token;

	if (!sock_obj->already_connected) {
		/* check for a new connection */
		if (poll(&sock_obj->sock_poller, 1, READ_TIMEOUT_MS) <= 0)
			return;

		/* return early if there are no new connections */
		if (sock_obj->sock_poller.revents == 0) return;

		/* check for new clients */
		sock_obj->current_fd = accept(sock_obj->listener_fd,
			(struct sockaddr *)&sock_obj->peer_sock,
			&sock_obj->peer_addr_size);
		if (sock_obj->current_fd == -1)
			return;

		/* setup the command poller */
		sock_obj->peer_poller.fd = sock_obj->current_fd;

		sock_obj->already_connected = 1;
		return;
	}

	/* check for new commands */
	if (poll(&sock_obj->peer_poller, 1, READ_TIMEOUT_MS) <= 0) return;

	/* return early if there are no new commands */
	if (sock_obj->peer_poller.revents == 0) return;

	memset(pipe_buf, 0, CTL_BUFFER_SIZE);
	if (read(sock_obj->current_fd, pipe_buf, CTL_BUFFER_SIZE - 1) == -1) {
		/* client might have disconnected */
		sock_obj->already_connected = 0;
		close(sock_obj->current_fd);
		return;
	}

	/* handle commands per line */
	token = strtok(pipe_buf, "\n");
	while (token != NULL) {
		memset(cmd_buf, 0, CMD_BUFFER_SIZE);
		strncpy(cmd_buf, token, CMD_BUFFER_SIZE - 1);
		token = strtok(NULL, "\n");

		process_ascii_cmd(rds, cmd_buf);
	}
}

void close_ctl_socket(struct rds_obj_t *rds) {
	ctl_socket_obj_t *sock_obj = rds->ctl_sock;
	if (sock_obj->current_fd > 0) close(sock_obj->current_fd);
	if (sock_obj->listener_fd > 0) close(sock_obj->listener_fd);
}
