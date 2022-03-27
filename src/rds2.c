/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019-2020 Anthony96922
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
#include "lib.h"
#include "waveforms.h"

/*
 * RDS2-specific stuff
 */

// station logo
extern unsigned char station_logo[];
extern unsigned int station_logo_len;

/*
 * Stuff for group type C
 */

/*
 * Station logo group (not fully implemented)
 * See https://www.youtube.com/watch?v=ticcJpCPoa8
 */

/*
 * Radio Packet Protocol
 *
 */
static void get_rpp_group(uint16_t *blocks) {
	/*
	 * Function Header
	 *
	 * (from http://www.rds.org.uk/2010/pdf/RDS%202%20-%20what%20it%20is_170127_13.pdf)
	 * Could this be for indicating the tunneled group?
	 */
	static uint8_t fh = 8 << 4 | 1 << 3 /* group 8B? */ | 0;
	static uint16_t logo_pos;

	/* goup header */
	blocks[0] |= fh << 8 | 0 /* mystery byte */;

	/* image data */
	blocks[1] = station_logo[logo_pos+0] << 8 | station_logo[logo_pos+1];
	blocks[2] = station_logo[logo_pos+2] << 8 | station_logo[logo_pos+3];
	blocks[3] = station_logo[logo_pos+4] << 8 | station_logo[logo_pos+5];
	if ((logo_pos += 6) >= station_logo_len) logo_pos = 0;
}

/*
 * RDS 2 group sequence
 */
static void get_rds2_group(int stream_num, uint16_t *blocks) {
	switch (stream_num) {
	case 0:
	case 1:
	case 2:
	default:
		get_rpp_group(blocks);
		break;
	}

	fprintf(stderr, "Stream %u: %04x %04x %04x %04x\n",
		stream_num, blocks[0], blocks[1], blocks[2], blocks[3]);
}

void get_rds2_bits(uint8_t stream, uint8_t *bits) {
	static uint16_t out_blocks[GROUP_LENGTH];
	get_rds2_group(stream, out_blocks);
	add_checkwords(out_blocks, bits);
}
