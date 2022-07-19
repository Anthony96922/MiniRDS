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

/*
 * RDS2-specific stuff
 */


void init_rds2_encoder(struct rds2_obj_t *rds2_obj) {
	rds2_obj = malloc(sizeof(struct rds2_obj_t));

        /* RFT (one or more) */
	rds2_obj->rfts = malloc(4 * sizeof(struct rds2_rft_t));

	for (uint8_t i = 0; i < 4; i++) {
		rds2_obj->rfts[i] = malloc(sizeof(struct rds2_rft_t));
	}
}

/*
 * RDS 2 group sequence
 */
static void get_rds2_group(struct rds2_obj_t *rds2_obj, uint8_t stream_num) {

	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	switch (stream_num) {
	case 0:
	case 1:
	case 2:
	default:
		get_rft_stream(rds2_obj);
		break;
	}

#ifdef RDS2_DEBUG
	fprintf(stderr, "Stream %u: %04x %04x %04x %04x\n",
		stream_num, block_data[0], block_data[1], block_data[2], block_data[3]);
#endif
}

void get_rds2_bits(struct rds2_obj_t *rds2_obj, uint8_t stream_num) {
	get_rds2_group(rds2_obj, stream_num);
	add_checkwords(rds2_obj->blocks, rds2_obj->bits);
}

void exit_rds2_encoder(struct rds2_obj_t *rds2_obj) {
	for (uint8_t i = 0; i < 4; i++) {
		free(rds2_obj->rfts[i]);
	}
	free(rds2_obj->rfts);
	free(rds2_obj);
}
