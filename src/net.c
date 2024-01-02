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
#include "net.h"
#include "ascii_cmd.h"

static int listener_fd; /* fd of the listener */
static int current_fd; /* fd of the current connection */
static struct sockaddr_in6 my_sock;
static struct sockaddr_in6 peer_sock;
static socklen_t peer_addr_size;
static struct pollfd sock_poller;
static struct pollfd peer_poller;
static struct pollfd curr_sock_poller;

uint8_t already_connected = 0;

/*
 * Opens a socket to be used to control the RDS coder.
 */

int open_ctl_socket(uint16_t port, uint8_t proto) {
	struct in6_addr my_addr;
	unsigned char listen_addr[sizeof(struct in6_addr)];

	/*
	 * 1 = tcp
	 * 0 = udp
	 */
	listener_fd = socket(AF_INET6, proto ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (listener_fd == -1) return -1;

	/* use ipv6 sock stuct to support both v4 and v6 */
	memset(&my_sock, 0, sizeof(struct sockaddr_in6));
	memset(&my_addr, 0, sizeof(struct in6_addr));
	memset(&peer_sock, 0, sizeof(struct sockaddr_in6));
	memset(listen_addr, 0, sizeof(struct in6_addr));

	/* configure listening address and port */
	my_sock.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "::", &listen_addr); /* listen on both v4 and v6 */
	memcpy(my_addr.s6_addr, listen_addr, sizeof(struct in6_addr));
	my_sock.sin6_addr = my_addr;
	my_sock.sin6_port = htons(port);

	/* setup the socket */
	if (bind(listener_fd, (struct sockaddr *)&my_sock,
		sizeof(struct sockaddr_in6) == -1))
		return -1;

	/* start listening (accept only 1 connection) */
	listen(listener_fd, 1);

	/* setup the socket poller */
	sock_poller.fd = listener_fd;
	sock_poller.events = POLLIN;

	curr_sock_poller.events = POLLIN;

	/* setup the command poller events */
	peer_poller.events = POLLIN;

	/* peer socket */
	peer_addr_size = sizeof(struct sockaddr_in6);

	return 0;
}

/*
 * Polls the current socket, and if a command is received,
 * calls process_ascii_cmd.
 */
void poll_ctl_socket() {
	static unsigned char pipe_buf[CTL_BUFFER_SIZE];
	static unsigned char cmd_buf[CMD_BUFFER_SIZE];
	char *token;

	if (!already_connected) {
		/* check for a new connection */
		if (poll(&sock_poller, 1, READ_TIMEOUT_MS) <= 0) return;

		/* return early if there are no new connections */
		if (sock_poller.revents == 0) return;

		/* check for new clients */
		current_fd = accept(listener_fd, (struct sockaddr *)&peer_sock,
			&peer_addr_size);
		if (current_fd == -1)
			return;

		/* setup the command poller */
		peer_poller.fd = current_fd;

		already_connected = 1;
		return;
	}

	/* check for new commands */
	if (poll(&peer_poller, 1, READ_TIMEOUT_MS) <= 0) return;

	/* return early if there are no new commands */
	if (peer_poller.revents == 0) return;

	memset(pipe_buf, 0, CTL_BUFFER_SIZE);
	if (read(current_fd, pipe_buf, CTL_BUFFER_SIZE - 1) == -1) {
		/* client might have disconnected */
		already_connected = 0;
		close(current_fd);
		return;
	}

	/* handle commands per line */
	token = strtok((char *)pipe_buf, "\n");
	while (token != NULL) {
		memset(cmd_buf, 0, CMD_BUFFER_SIZE);
		memcpy(cmd_buf, token, CMD_BUFFER_SIZE - 1);
		token = strtok(NULL, "\n");

		process_ascii_cmd(cmd_buf);
	}
}

void close_ctl_socket() {
	if (current_fd > 0) close(current_fd);
	if (listener_fd > 0) close(listener_fd);
}
