/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019-2022 Anthony96922
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
 * RDS2 RFT-specific stuff
 */

static void init_rft(struct rds2_obj_t *rds2_obj, uint8_t channel,
	unsigned char *img, size_t len) {

	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	uint32_t seg_counter = 0;
	uint32_t chunk_counter = 0;
	uint16_t chunk_size = 0;
	uint8_t *chunks;
	uint16_t packet_size = 0;

	/* image cannot be larger than 163840 bytes */
	if (len > MAX_IMAGE_LEN) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "Image too large!\n");
#endif
		return;
	}

	rft->channel = channel;
	rft->file_len = len;

	/* determine how many segments we need */
	while (seg_counter < rft->file_len) {
		seg_counter += 5;
		rft->num_segs++;
	}

	rft->file_data = malloc(rft->num_segs * 5);
	memset(rft->file_data, 0, rft->num_segs * 5);
	memcpy(rft->file_data, img, len);

	rft->crc_mode = 7; /* autoselect */

	if (len > 81960) {
		chunk_size = 64;
	} else if (len > 40960) {
		chunk_size = 32;
	} else {
		chunk_size = 16;
	}

	/* RFT packet size = chunk size * 5 */
	packet_size = chunk_size * 5;

	while (chunk_counter < len) {
		chunk_counter += packet_size;
		rft->num_chunks++;
	}

	rft->crcs = malloc(rft->num_chunks * sizeof(uint16_t));

	chunks = malloc(packet_size);

	for (uint16_t i = 0; i < rft->num_chunks; i++) {
		memset(chunks, 0, packet_size);
		memcpy(chunks,
			rft->file_data + i * packet_size, packet_size);
		rft->crcs[i] = crc16(chunks, packet_size);
	}

	free(chunks);
}

static void exit_rds2_rft(struct rds2_rft_t *rft) {
	free(rft->file_data);
	free(rft->crcs);
}


void init_rds2_rft(struct rds2_obj_t *rds2_obj, uint8_t *image, size_t len, uint8_t channel) {
	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	init_rft(rft, 8, image, len);
}

static void get_rft_data_group(struct rds2_obj_t *rds2_obj) {
	uint16_t *blocks = rds2_obj->blocks;
	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	/* function header */
	blocks[0] = 2 << 12;

	/* pipe number */
	blocks[0] |= (rft->channel & INT8_L4) << 8;

	/* toggle */
	blocks[0] |= (0 & 1) << 7;

	/* segment address */
	blocks[0] |= (rft->seg_addr >> 8) & INT8_L7;
	blocks[1] = (rft->seg_addr & 255) << 8;

	/* image data */
	blocks[1] |= rft->file_data[rft->seg_addr*5+0];
	blocks[2] = rft->file_data[rft->seg_addr*5+1] << 8;
	blocks[2] |= rft->file_data[rft->seg_addr*5+2];
	blocks[3] = rft->file_data[rft->seg_addr*5+3] << 8;
	blocks[3] |= rft->file_data[rft->seg_addr*5+4];

	rft->seg_addr++;
	if (rft->seg_addr >= rft->num_segs) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "File sending complete\n");
#endif
		rft->seg_addr = 0;
	}
}

/*
 * RFT metadata
 *
 */
static void get_rft_variant_0_group(struct rds2_obj_t *rds2_obj) {
	uint16_t *blocks = rds2_obj->blocks;
	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	/* function header */
	blocks[0] = (2 << 6) << 8;

	/* pipe number */
	blocks[0] |= rft->channel & INT8_L4;

	/* ODA AID */
	blocks[1] = 0xFF7F;

	/* variant code */
	blocks[2] = (0 & INT8_L4) << 12;

	/* crc flag */
	blocks[2] |= (rft->crc_mode & 1) << 11;

	/* file version */
	blocks[2] |= (rft->file_version & INT8_L3) << 8;

	/* file indentification */
	blocks[2] |= (rft->file_id & INT8_L6) << 2;

	/* file size */
	blocks[2] |= (rft->file_len & (3 << 16));
	blocks[3] = rft->file_len & 0xFFFF;
}

/*
 * RFT CRC16
 *
 */
static void get_rft_variant_1_group(struct rds2_obj_t *rds2_obj) {
	uint16_t *blocks = rds2_obj->blocks;
	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	/* function header */
	blocks[0] = (2 << 6) << 8;

	/* pipe number */
	blocks[0] |= rft->channel & INT8_L4;

	/* ODA AID */
	blocks[1] = 0xFF7F;

	/* variant code */
	blocks[2] = (1 & INT8_L4) << 12;

	/* mode */
	blocks[2] |= (rft->crc_mode & INT8_L3) << 9;

	/* chunk address */
	blocks[2] |= rft->chunk_addr;

	/* crc */
	blocks[3] = rft->crcs[rft->chunk_addr];

	rft->chunk_addr++;
	if (rft->chunk_addr == rft->num_chunks) rft->chunk_addr = 0;
}

void get_rft_stream(struct rds2_obj_t *rds2_obj) {
	struct rds2_rft_t *rft = rds2_obj->rfts[0];

	static uint8_t rft_state;

	switch (rft_state) {
		case 0:
			get_rft_variant_0_group(rft);
			break;

		case 1:
			get_rft_variant_1_group(rft);
			break;

		default:
			get_rft_data_group(rft);
			break;
	}

	rft_state++;
	if (rft_state == 16) rft_state = 0;
}
