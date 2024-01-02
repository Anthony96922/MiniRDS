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

#include "common.h"
#include "ascii_cmd.h"
#include "control_pipe.h"

static int fd;
static struct pollfd poller;

/*
 * Opens a file (pipe) to be used to control the RDS coder.
 */
int open_control_pipe(char *filename) {
	fd = open(filename, O_RDONLY | O_NONBLOCK);
	if (fd == -1) return -1;

	/* setup the poller */
	poller.fd = fd;
	poller.events = POLLIN;

	return 0;
}

/*
 * Polls the control file (pipe), and if a command is received,
 * calls process_ascii_cmd.
 */
void poll_control_pipe() {
	static unsigned char pipe_buf[CTL_BUFFER_SIZE];
	static unsigned char cmd_buf[CMD_BUFFER_SIZE];
	struct timeval timeout;
	int ret;
	fd_set set;
	char *token;

	FD_ZERO(&set);
	FD_SET(fd, &set);
	timeout.tv_sec = 0;
	timeout.tv_usec = READ_TIMEOUT_MS * 1000;

	/* check for new commands */
	if (poll(&poller, 1, READ_TIMEOUT_MS) <= 0) return;

	/* return early if there are no new commands */
	if (poller.revents == 0) return;

	memset(pipe_buf, 0, CTL_BUFFER_SIZE);

	ret = select(fd + 1, &set, NULL, NULL, &timeout);
	if (ret == -1 || ret == 0) {
		return;
	} else {
		read(fd, pipe_buf, CTL_BUFFER_SIZE - 1);
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

void close_control_pipe() {
	if (fd > 0) close(fd);
}
