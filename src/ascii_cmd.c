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
#include "rds_setters.h"

/*
 * If a command is received, process it and update the RDS data.
 *
 */
void process_ascii_cmd(struct rds_obj_t *rds_obj, char *cmd) {
	char *arg;
	uint8_t cmd_len = 0;

	while (cmd[cmd_len] != 0 && cmd_len < 255)
		cmd_len++;

	if (cmd_len > 3 && cmd[2] == ' ') {
		arg = cmd + 3;

		if (strncmp(cmd, "PI", 2) == 0) {
			arg[4] = 0;
			set_rds_pi(rds_obj, strtoul(arg, NULL, 16));
			return;
		}
		if (strncmp(cmd, "PS", 2) == 0) {
			arg[PS_LENGTH] = 0;
			set_rds_ps(rds_obj, arg);
			return;
		}
		if (strncmp(cmd, "RT", 2) == 0) {
			arg[RT_LENGTH] = 0;
			set_rds_rt(rds_obj, arg);
			return;
		}
		if (strncmp(cmd, "TA", 2) == 0) {
			set_rds_ta(rds_obj, arg[0] == '1' ? 1 : 0);
			return;
		}
		if (strncmp(cmd, "TP", 2) == 0) {
			set_rds_tp(rds_obj, arg[0] == '1' ? 1 : 0);
			return;
		}
		if (strncmp(cmd, "MS", 2) == 0) {
			set_rds_ms(rds_obj, arg[0] == '1' ? 1 : 0);
			return;
		}
		if (strncmp(cmd, "DI", 2) == 0) {
			arg[2] = 0;
			set_rds_di(rds_obj, strtoul(arg, NULL, 10));
			return;
		}
	}

	if (cmd_len > 4 && cmd[3] == ' ') {
		arg = cmd + 4;

		if (strncmp(cmd, "PTY", 3) == 0) {
			set_rds_pty(rds_obj, strtoul(arg, NULL, 10));
			return;
		}
		if (strncmp(cmd, "RTP", 3) == 0) {
			uint8_t tags[8];
			if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
				&tags[0], &tags[1], &tags[2], &tags[3],
				&tags[4], &tags[5]) == 6) {
				set_rds_rtplus_tags(rds_obj, tags);
			}
			return;
		}
		if (strncmp(cmd, "MPX", 3) == 0) {
			uint8_t gains[5];
			if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu",
				&gains[0], &gains[1], &gains[2], &gains[3],
				&gains[4]) == 5) {
				for (uint8_t i = 0; i < 5; i++) {
					set_carrier_volume(rds_obj, i, gains[i]);
				}
			}
			return;
		}
		if (strncmp(cmd, "VOL", 3) == 0) {
			arg[4] = 0;
			set_output_volume(rds_obj, strtoul(arg, NULL, 10));
			return;
		}
#ifdef RDS2
		if (strncmp(cmd, "LPS", 3) == 0) {
			arg[LPS_LENGTH] = 0;
			if (arg[0] == '-') {
				char tmp[1];
				tmp[0] = 0;
				set_rds_lps(rds_obj, tmp);
			} else {
				set_rds_lps(rds_obj, arg);
			}
			return;
		}
		if (strncmp(cmd, "ERT", 3) == 0) {
			arg[ERT_LENGTH] = 0;
			if (arg[0] == '-') {
				char tmp[1];
				tmp[0] = 0;
				set_rds_ert(rds_obj, tmp);
			} else {
				set_rds_ert(rds_obj, arg);
			}
			return;
		}
#endif /* RDS2 */
	}

	if (cmd_len > 5 && cmd[4] == ' ') {
		arg = cmd + 5;

		if (strncmp(cmd, "RTPF", 4) == 0) {
			uint8_t rtp_flags[2];
			if (sscanf(arg, "%hhu,%hhu",
				&rtp_flags[0], &rtp_flags[1]) == 2) {
				set_rds_rtplus_flags(rds_obj, rtp_flags);
			}
			return;
		}
		if (strncmp(cmd, "PTYN", 4) == 0) {
			arg[8] = 0;
			if (arg[0] == '-') {
				char tmp[1];
				tmp[0] = 0;
				set_rds_ptyn(tmp);
			} else {
				set_rds_ptyn(arg);
			}
			return;
		}
#ifdef RDS2
		if (strncmp(cmd, "ERTP", 3) == 0) {
			uint8_t tags[8];
			if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
				&tags[0], &tags[1], &tags[2], &tags[3],
				&tags[4], &tags[5]) == 6) {
				set_rds_ertplus_tags(rds_obj, tags);
			}
			return;
		}
#endif /* RDS2 */
	}

#ifdef RDS2
	if (cmd_len > 6 && cmd[5] == ' ') {
		arg = cmd + 6;

		if (strncmp(cmd, "ERTPF", 5) == 0) {
			uint8_t ertp_flags[2];
			if (sscanf(arg, "%hhu,%hhu",
				&ertp_flags[0], &ertp_flags[1]) == 2) {
				set_rds_ertplus_flags(rds_obj, ertp_flags);
			}
			return;
		}
	}
#endif /* RDS2 */
}
