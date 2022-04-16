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

typedef struct rds2_oda_t {
	uint16_t aid;
	uint8_t channel;
} rds2_oda_t;

#define GET_RDS2_ODA_CHANNEL(x)	(x & INT8_L5)

#define MAX_IMAGE_LEN	163840

/* RDS2 File Transfer */
typedef struct rft_t {
	uint8_t channel;

	unsigned char *file_data;
	size_t file_len;
	uint8_t file_version;
	uint8_t file_id;
	uint8_t variant_code;

	uint16_t seg_addr;
	uint16_t num_segs;

	uint16_t chunk_addr;
	uint16_t num_chunks;

	uint8_t crc_mode;
	uint16_t *crcs;
} rft_t;

extern void get_rds2_bits(uint8_t stream_num, uint8_t *bits);
extern void init_rds2_encoder();
extern void exit_rds2_encoder();
