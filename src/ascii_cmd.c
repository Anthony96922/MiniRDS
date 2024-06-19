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
#include "rds.h"
#include "fm_mpx.h"
#include "lib.h"
#include "ascii_cmd.h"

#define CMD_MATCHES(a) (ustrcmp(cmd, (unsigned char *)a) == 0)

/*
 * If a command is received, process it and update the RDS data.
 *
 */

void process_ascii_cmd(unsigned char *str) {
	unsigned char *cmd, *arg;
	uint16_t cmd_len = 0;

	while (str[cmd_len] != 0 && cmd_len < CTL_BUFFER_SIZE)
		cmd_len++;

	if (cmd_len > 3 && str[2] == ' ') {
		cmd = str;
		cmd[2] = 0;
		arg = str + 3;

		if (CMD_MATCHES("PI")) {
			arg[4] = 0;
#ifdef RBDS
			if (arg[0] == 'K' || arg[0] == 'W' ||
				arg[0] == 'k' || arg[0] == 'w') {
				set_rds_pi(callsign2pi(arg));
			} else
#endif
			set_rds_pi(strtoul((char *)arg, NULL, 16));
			return;
		}
		if (CMD_MATCHES("PS")) {
			arg[PS_LENGTH * 2] = 0;
			set_rds_ps(xlat(arg));
			return;
		}
		if (CMD_MATCHES("RT")) {
			arg[RT_LENGTH * 2] = 0;
			set_rds_rt(xlat(arg));
			return;
		}
		if (CMD_MATCHES("TA")) {
			set_rds_ta(arg[0]);
			return;
		}
		if (CMD_MATCHES("TP")) {
			set_rds_tp(arg[0]);
			return;
		}
		if (CMD_MATCHES("MS")) {
			set_rds_ms(arg[0]);
			return;
		}
		if (CMD_MATCHES("DI")) {
			arg[2] = 0;
			set_rds_di(strtoul((char *)arg, NULL, 10));
			return;
		}
		if (CMD_MATCHES("AF")) {
			/* TODO: work on existing AF list */
			uint8_t arg_count;
			rds_af_t new_af;
			char af_cmd;
			float af[MAX_AFS], *af_iter;

			arg_count = sscanf((char *)arg,
				"%c " /* AF command */
				"%f %f %f %f %f " /* AF list */
				"%f %f %f %f %f "
				"%f %f %f %f %f "
				"%f %f %f %f %f "
				"%f %f %f %f %f",
			&af_cmd,
			&af[0],  &af[1],  &af[2],  &af[3],  &af[4],
			&af[5],  &af[6],  &af[7],  &af[8],  &af[9],
			&af[10], &af[11], &af[12], &af[13], &af[14],
			&af[15], &af[16], &af[17], &af[18], &af[19],
			&af[20], &af[21], &af[22], &af[23], &af[24]);
			switch (af_cmd) {
			case 's': /* set */
				af_iter = af;
				memset(&new_af, 0, sizeof(struct rds_af_t));
				while ((arg_count-- - 1) != 0) {
					add_rds_af(&new_af, *af_iter++);
				}
				set_rds_af(new_af);
				break;
			case 'c': /* clear */
				clear_rds_af();
				break;
			default: /* other */
				return;
			}
		}
	}

	if (cmd_len > 4 && str[3] == ' ') {
		cmd = str;
		cmd[3] = 0;
		arg = str + 4;

		if (CMD_MATCHES("PTY")) {
			if (arg[0] >= 'A') { /* PTY ID was passed */
				set_rds_pty(get_pty_code((char *)arg));
			} else {
				arg[2] = 0;
				set_rds_pty(strtoul((char *)arg, NULL, 10));
			}
			return;
		}
		if (CMD_MATCHES("RTP")) {
			char tag_names[2][32];
			uint8_t tags[6];
			if (sscanf((char *)arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
				&tags[0], &tags[1], &tags[2], &tags[3],
				&tags[4], &tags[5]) == 6) {
				set_rds_rtplus_tags(tags);
			} else if (sscanf((char *)arg, "%31[^,],%hhu,%hhu,%31[^,],%hhu,%hhu",
				tag_names[0], &tags[1], &tags[2],
				tag_names[1], &tags[4], &tags[5]) == 6) {
				tags[0] = get_rtp_tag_id(tag_names[0]);
				tags[3] = get_rtp_tag_id(tag_names[1]);
				set_rds_rtplus_tags(tags);
			}
			return;
		}
		if (CMD_MATCHES("MPX")) {
			float gains[5];
			if (sscanf((char *)arg, "%f,%f,%f,%f,%f",
				&gains[0], &gains[1], &gains[2], &gains[3],
				&gains[4]) == 5) {
				set_carrier_volume(0, gains[0]);
				set_carrier_volume(1, gains[1]);
				set_carrier_volume(2, gains[2]);
				set_carrier_volume(3, gains[3]);
				set_carrier_volume(4, gains[4]);
			}
			return;
		}
		if (CMD_MATCHES("VOL")) {
			arg[4] = 0;
			set_output_volume(strtof((char *)arg, NULL));
			return;
		}
		if (CMD_MATCHES("LPS")) {
			arg[LPS_LENGTH] = 0;
			if (arg[0] == '-') arg[0] = 0;
			set_rds_lps(arg);
			return;
		}
		if (CMD_MATCHES("ERT")) {
			arg[ERT_LENGTH] = 0;
			if (arg[0] == '-') arg[0] = 0;
			set_rds_ert(arg);
			return;
		}
	}

	if (cmd_len > 5 && str[4] == ' ') {
		cmd = str;
		cmd[4] = 0;
		arg = str + 5;

		if (CMD_MATCHES("RTPF")) {
			arg[1] = 0;
			set_rds_rtplus_flags(strtoul((char *)arg, NULL, 10));
			return;
		}
		if (CMD_MATCHES("PTYN")) {
			arg[PTYN_LENGTH] = 0;
			if (arg[0] == '-') arg[0] = 0;
			set_rds_ptyn(arg);
			return;
		}
		if (CMD_MATCHES("ERTP")) {
			char tag_names[2][32];
			uint8_t tags[6];
			if (sscanf((char *)arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
				&tags[0], &tags[1], &tags[2], &tags[3],
				&tags[4], &tags[5]) == 6) {
				set_rds_ertplus_tags(tags);
			} else if (sscanf((char *)arg, "%31[^,],%hhu,%hhu,%31[^,],%hhu,%hhu",
				tag_names[0], &tags[1], &tags[2],
				tag_names[1], &tags[4], &tags[5]) == 6) {
				tags[0] = get_rtp_tag_id(tag_names[0]);
				tags[3] = get_rtp_tag_id(tag_names[1]);
				set_rds_ertplus_tags(tags);
			}
			return;
		}
	}

	if (cmd_len > 6 && str[5] == ' ') {
		cmd = str;
		cmd[5] = 0;
		arg = str + 6;

		if (CMD_MATCHES("ERTPF")) {
			arg[1] = 0;
			set_rds_ertplus_flags(strtoul((char *)arg, NULL, 10));
			return;
		}
	}
}
