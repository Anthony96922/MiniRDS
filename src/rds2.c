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
#include "rds2.h"

/*
 * RDS2-specific stuff
 */

// station logo
extern unsigned char station_logo[];
extern unsigned int station_logo_len;

static struct rds2_oda_t rds2_odas[MAX_ODAS];

static struct {
	uint8_t current;
	uint8_t count;
} oda_state;

static struct rft_t station_logo_group;

static void init_rft(struct rft_t *rft, uint8_t channel, unsigned char *img, size_t len) {
	uint32_t seg_counter = 0;
	uint32_t chunk_counter = 0;
	uint16_t chunk_size = 0;
	unsigned char chunks[64];

	/* image cannot be larger than 163840 bytes */
	if (len > MAX_IMAGE_LEN) {
		fprintf(stderr, "Image too large!\n");
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

	while (chunk_counter < len) {
		chunk_counter += chunk_size;
		rft->num_chunks++;
        }

	rft->crcs = malloc(rft->num_chunks * sizeof(uint16_t));

	for (uint16_t i = 0; i < rft->num_chunks; i++) {
		memset(chunks, 0, chunk_size);
		memcpy(chunks, rft->file_data + chunk_size * i, chunk_size);
		rft->crcs[i] = crc16(chunks, chunk_size);
	}
}

static void exit_rft(struct rft_t *rft) {
	free(rft->file_data);
	free(rft->crcs);
}

static void register_rds2_oda(uint8_t channel, uint16_t aid) {

	if (oda_state.count == MAX_ODAS) return; /* can't accept more ODAs */

	rds2_odas[oda_state.count].channel = channel;
	rds2_odas[oda_state.count].aid = aid;
	oda_state.count++;
}

void init_rds2_encoder() {
	init_rft(&station_logo_group, 8, station_logo, station_logo_len);

	/* register the RFT ODA */
	register_rds2_oda(8, 0xFF7F);
}

static void get_rft_data_group(struct rft_t *rft, uint16_t *blocks) {

	/* function header */
	blocks[0] = 2 << 12;
	blocks[0] |= (rft->channel & INT8_L4) << 8;

	/* toggle */
	blocks[0] |= (0 & INT8_L1) << 7;

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
	if (rft->seg_addr >= rft->num_segs)
		rft->seg_addr = 0;
}

static void get_rft_variant_0_group(struct rft_t *rft, uint16_t *blocks) {

	blocks[0] = (2 << 6) << 8;
	blocks[0] |= rft->channel & INT8_L4;

	blocks[1] = 0xFF7F;

	blocks[2] = (0 & INT8_L4) << 12;
	blocks[2] |= (rft->crc_mode & INT8_L1) << 11;
	blocks[2] |= (rft->file_version & INT8_L3) << 8;
	blocks[2] |= (rft->file_id & INT8_L6) << 2;
	blocks[2] |= (rft->file_len & (3 << 16));

	blocks[3] = rft->file_len & 0xFFFF;

}

/*
 * RFT CRC16
 *
 */
static void get_rft_variant_1_group(struct rft_t *rft, uint16_t *blocks) {
	blocks[0] = (2 << 6) << 8;
	blocks[0] |= rft->channel & INT8_L4;

	blocks[1] = 0xFF7F;

	blocks[2] = (1 & INT8_L4) << 12;
	blocks[2] |= (rft->crc_mode & INT8_L1) << 11;
	blocks[2] |= rft->chunk_addr;

	blocks[3] = rft->crcs[rft->chunk_addr];

	rft->chunk_addr++;
	if (rft->chunk_addr == rft->num_chunks) rft->chunk_addr = 0;
}

static void get_rft_stream(uint16_t *blocks) {
	static uint8_t rft_state;

	switch (rft_state) {
		case 0:
			get_rft_data_group(&station_logo_group, blocks);
			break;

		case 1:
			get_rft_variant_1_group(&station_logo_group, blocks);
			break;

		default:
			get_rft_variant_0_group(&station_logo_group, blocks);
			break;
	}

	rft_state++;
	if (rft_state == 3) rft_state = 0;
}

/*
 * RDS 2 group sequence
 */
static void get_rds2_group(uint8_t stream_num, uint16_t *blocks) {
	switch (stream_num) {
	case 0:
	case 1:
	case 2:
	default:
		get_rft_stream(blocks);
		break;
	}

#if 0
	fprintf(stderr, "Stream %u: %04x %04x %04x %04x\n",
		stream_num, blocks[0], blocks[1], blocks[2], blocks[3]);
#endif
}

void get_rds2_bits(uint8_t stream, uint8_t *bits) {
	static uint16_t out_blocks[GROUP_LENGTH];
	get_rds2_group(stream, out_blocks);
	add_checkwords(out_blocks, bits);
}

void exit_rds2_encoder() {
	exit_rft(&station_logo_group);
}
