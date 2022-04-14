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
#include <fcntl.h>
#include <poll.h>

#include "rds.h"
#include "fm_mpx.h"

//#define CONTROL_PIPE_MESSAGES

#define CMD_BUFFER_SIZE	200
#define CTL_BUFFER_SIZE	(CMD_BUFFER_SIZE * 10)
#define READ_TIMEOUT_MS	100

static int fd;
static struct pollfd poller;

/*
 * Opens a file (pipe) to be used to control the RDS coder, in non-blocking mode.
 */

int open_control_pipe(char *filename) {
	fd = open(filename, O_RDONLY);
	if (fd == -1) return -1;

	/* setup the poller */
	poller.fd = fd;
	poller.events = POLLIN | POLLPRI;

	return 0;
}

/*
 * Polls the control file (pipe), non-blockingly, and if a command is received,
 * processes it and updates the RDS data.
 */
void poll_control_pipe() {
	static char buf[CTL_BUFFER_SIZE];
	static char cmd_buf[CMD_BUFFER_SIZE];
	char *arg;
	char *token;

	/* check for new commands */
	if (poll(&poller, 1, READ_TIMEOUT_MS) <= 0) return;

	/* return early if there are no new commands */
	if ((poller.revents | poller.events) == 0) return;

	memset(buf, 0, CTL_BUFFER_SIZE);
	read(fd, buf, CTL_BUFFER_SIZE - 1);

	token = strtok(buf, "\n");

	while (token != NULL) {
		memset(cmd_buf, 0, CMD_BUFFER_SIZE);
		strncpy(cmd_buf, token, CMD_BUFFER_SIZE - 1);
		token = strtok(NULL, "\n");

		if (strlen(cmd_buf) > 3 && cmd_buf[2] == ' ') {
			arg = cmd_buf + 3;

			if (strncmp(cmd_buf, "PI", 2) == 0) {
				arg[4] = 0;
				uint16_t pi = strtoul(arg, NULL, 16) & 65535;
				set_rds_pi(pi);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "PI set to \"%04X\"\n", pi);
#endif
			}
			if (strncmp(cmd_buf, "PS", 2) == 0) {
				arg[8] = 0;
				set_rds_ps(arg);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "PS set to \"%s\"\n", arg);
#endif
			}
			if (strncmp(cmd_buf, "RT", 2) == 0) {
				arg[64] = 0;
				set_rds_rt(arg);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "RT set to \"%s\"\n", arg);
#endif
			}
			if (strncmp(cmd_buf, "TA", 2) == 0) {
			uint8_t ta = arg[0] == '1' ? 1 : 0;
				set_rds_ta(ta);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "Set TA to %s\n", ta ? "ON" : "OFF");
#endif
			}
			if (strncmp(cmd_buf, "TP", 2) == 0) {
				uint8_t tp = arg[0] == '1' ? 1 : 0;
				set_rds_tp(tp);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "Set TP to %s\n", tp ? "ON" : "OFF");
#endif
			}
			if (strncmp(cmd_buf, "MS", 2) == 0) {
				uint8_t ms = arg[0] == '1' ? 1 : 0;
				set_rds_ms(ms);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "Set MS to %s\n", ms ? "ON" : "OFF");
#endif
			}
			if (strncmp(cmd_buf, "DI", 2) == 0) {
				arg[1] = 0;
				uint8_t di = strtoul(arg, NULL, 10) & 7;
				set_rds_di(di);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "DI value set to %u\n", di);
#endif
			}
		}

		if (strlen(cmd_buf) > 4 && cmd_buf[3] == ' ') {
			arg = cmd_buf + 4;

			if (strncmp(cmd_buf, "PTY", 3) == 0) {
				uint8_t pty = strtoul(arg, NULL, 10) & 31;
				set_rds_pty(pty);
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "PTY set to %u\n", pty);
#endif
			}
			if (strncmp(cmd_buf, "RTP", 3) == 0) {
				uint8_t tags[8];
				if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
					&tags[0], &tags[1], &tags[2], &tags[3],
					&tags[4], &tags[5]
				) == 6) {
#ifdef CONTROL_PIPE_MESSAGES
					for (uint8_t i = 0; i < 2; i++) {
						fprintf(stderr,
							"RT+ tag %u: type: %u, start: %u, length: %u\n",
							i, tags[i*3+0], tags[i*3+1], tags[i*3+2]);
					}
#endif
					set_rds_rtplus_tags(tags);
				}
#ifdef CONTROL_PIPE_MESSAGES
				else {
					fprintf(stderr, "Could not parse RT+ tag info.\n");
				}
#endif
			}
			if (strncmp(cmd_buf, "MPX", 3) == 0) {
				uint8_t gains[5];
				if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu", &gains[0], &gains[1], &gains[2], &gains[3], &gains[4]) == 5) {
					for (uint8_t i = 0; i < 5; i++) {
						set_carrier_volume(i, gains[i]);
					}
				}
			}
			if (strncmp(cmd_buf, "VOL", 3) == 0) {
				arg[4] = 0;
				set_output_volume(strtoul(arg, NULL, 10));
			}
		}

		if (strlen(cmd_buf) > 5 && cmd_buf[4] == ' ') {
			arg = cmd_buf + 5;

			if (strncmp(cmd_buf, "RTPF", 4) == 0) {
				uint8_t rtp_flags[2];
				if (sscanf(arg, "%hhu,%hhu", &rtp_flags[0], &rtp_flags[1]) == 2) {
#ifdef CONTROL_PIPE_MESSAGES
					fprintf(stderr, "RT+ flags: running: %u, toggle: %u\n", rtp_flags[0], rtp_flags[1]);
#endif
					set_rds_rtplus_flags(rtp_flags[0], rtp_flags[1]);
				}
#ifdef CONTROL_PIPE_MESSAGES
				else {
					fprintf(stderr, "Could not parse RT+ flags.\n");
				}
#endif
			}
			if (strncmp(cmd_buf, "PTYN", 4) == 0) {
				arg[8] = 0;
				if (arg[0] == '-') {
#ifdef CONTROL_PIPE_MESSAGES
					fprintf(stderr, "PTYN disabled\n");
#endif
					char tmp[8];
					tmp[0] = 0;
					set_rds_ptyn(tmp);
				} else {
#ifdef CONTROL_PIPE_MESSAGES
					fprintf(stderr, "PTYN set to \"%s\"\n", arg);
#endif
					set_rds_ptyn(arg);
				}
			}
		}
	}
}

void close_control_pipe() {
	if (fd > 0) close(fd);
}
