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
#include "rds2.h"
#include "lib.h"

/*
 * RDS2-specific stuff
 */


/* RFT */

/* station logo */
extern unsigned char station_logo[];
extern unsigned int station_logo_len;

static struct rft_t station_logo_group;

static void init_rft(struct rft_t *rft, uint8_t channel,
	unsigned char *img, size_t len) {
	uint32_t seg_counter = 0;
	uint32_t chunk_counter = 0;
	uint16_t chunk_size = 0;
	uint8_t *chunks;
	uint16_t packet_size = 0;
	uint16_t packet_size_remainder;

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
	/* unused data must be padded with zeroes */
	memset(rft->file_data, 0, rft->num_segs * 5);
	memcpy(rft->file_data, img, len);

	rft->crc_mode = CRC_MODE_AUTO; /* autoselect */

	if (len >= 81920) {
		chunk_size = 64;
		rft->crc_mode = CRC_MODE_64_GROUPS;
	} else if (len > 40960) {
		chunk_size = 32;
		rft->crc_mode = CRC_MODE_32_GROUPS;
	} else {
		chunk_size = 16;
		rft->crc_mode = CRC_MODE_16_GROUPS;
	}

	/* RFT packet size = chunk size * 5 data bytes per group */
	packet_size = chunk_size * 5;

	/* the remainder (if file size is not a multiple of packet_size) */
	packet_size_remainder = rft->file_len % packet_size;

	/* calculate number of file chunks */
	while (chunk_counter <= len) {
		chunk_counter += packet_size;
		rft->num_chunks++;
	}

	rft->crcs = malloc(rft->num_chunks * sizeof(uint16_t));

	chunks = malloc(packet_size);

	/* calculate CRC's for chunks */
	for (uint16_t i = 0; i < rft->num_chunks; i++) {
		memset(chunks, 0, packet_size);
		memcpy(chunks,
			rft->file_data + i * packet_size, packet_size);
		if (i == rft->num_chunks - 1) {
			/* last chunk may have less than packet_size bytes */
			rft->crcs[i] = crc16(chunks,
				packet_size_remainder ?
				packet_size_remainder : packet_size);
		} else {
			rft->crcs[i] = crc16(chunks, packet_size);
		}
	}

	free(chunks);
}

static void exit_rft(struct rft_t *rft) {
	free(rft->file_data);
	free(rft->crcs);
}

/*
 * RFT variant 0 (file metadata)
 *
 * Carries:
 * - Function Header
 * - Pipe Number
 * - ODA ID
 * - Variant code (0)
 * - CRC-16 Flag
 * - File ID
 * - File size (18 bits)
 */
static void get_rft_var_0_data_group(struct rft_t *rft, uint16_t *blocks) {
	/* function header */
	blocks[0] = 1 << 15;
	blocks[0] |= (rft->channel & INT8_L4); /* pipe number */

	/* ODA ID */
	blocks[1] = ODA_AID_RFT;

	blocks[2] = (0 & INT18_L4) << 12; /* variant code */
	blocks[2] |= ((rft->crc_mode ? 1 : 0) & INT8_L1) << 11; /* CRC */
	blocks[2] |= (1 & INT8_L3) << 8; /* file version */
	blocks[2] |= (1 & INT8_L6) << 2; /* file ID */
	blocks[2] |= (rft->file_len & INT18_U2) >> 16;

	blocks[3] = rft->file_len & INT16_ALL;
}

/*
 * RFT variant 1 (chunk to CRC mappings)
 *
 * Carries:
 * - Function Header
 * - Pipe Number
 * - ODA ID
 * - Variant Code (1)
 * - CRC Mode
 * - Chunk Address (9 bits)
 * - CRC
 */
static void get_rft_var_1_data_group(struct rft_t *rft, uint16_t *blocks) {
	/* function header */
	blocks[0] = 1 << 15;
	blocks[0] |= (rft->channel & INT8_L4); /* pipe number */

	/* ODA ID */
	blocks[1] = ODA_AID_RFT;

	blocks[2] = (1 & INT8_L4) << 12; /* variant code */
	blocks[2] |= (rft->crc_mode & INT8_L3) << 9; /* CRC mode */
	/* chunk address */
	blocks[2] |= rft->seg_addr_crc & INT16_L9;

	/* CRC */
	blocks[3] = rft->crcs[rft->seg_addr_crc];

	rft->seg_addr_crc++;
	if (rft->seg_addr_crc > rft->num_chunks) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "File CRC sending complete\n");
#endif
		rft->seg_addr_crc = 0;
	}
}

/*
 * RFT variants 2-15 (custom variants)
 *
 * Carries:
 * - Function Header
 * - Pipe Number
 * - Variant code
 * - Variant data:
 *   - 2-7 carries file-related data
 *   - 8-15 ODA data that is not related
 */
#if 0
static void get_rft_var_2_15_data_group(struct rft_t *rft, uint16_t *blocks) {
	/* function header */
	blocks[0] = (2 << 6) << 8;
	blocks[0] |= (rft->channel & INT8_L4); /* pipe number */

	/* ODA ID */
	blocks[1] = ODA_AID_RFT;

	blocks[2] = (8 & INT8_L4) << 12; /* variant code */
	blocks[2] |= 0 & INT16_L12;
	blocks[3] = 0 & INT16_ALL;
}
#endif

/*
 * RFT data group (for file data TXing)
 *
 * Carries:
 * - Function Header
 * - Pipe Number
 * - Toggle Bit
 * - Segment address (15 bits)
 * - File data (5 bytes per group)
 */
static void get_rft_file_data_group(struct rft_t *rft, uint16_t *blocks) {
	/* function header */
	blocks[0] = 2 << 12;
	blocks[0] |= (rft->channel & INT8_L4) << 8; /* pipe number */

	/* toggle bit */
	blocks[0] |= (0 & INT16_L1) << 7;

	/* segment address */
	blocks[0] |= ((rft->seg_addr_img & INT16_U8) >> 8) & INT16_L7;
	blocks[1] = (rft->seg_addr_img & INT16_L8) << 8;

	/* image data */
	blocks[1] |= rft->file_data[rft->seg_addr_img * 5 + 0];
	blocks[2] =  rft->file_data[rft->seg_addr_img * 5 + 1] << 8;
	blocks[2] |= rft->file_data[rft->seg_addr_img * 5 + 2];
	blocks[3] =  rft->file_data[rft->seg_addr_img * 5 + 3] << 8;
	blocks[3] |= rft->file_data[rft->seg_addr_img * 5 + 4];

	rft->seg_addr_img++;
	if (rft->seg_addr_img > rft->num_segs) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "File sending complete\n");
#endif
		rft->seg_addr_img = 0;
	}
}

static void get_rft_stream(uint16_t *blocks) {
	static uint8_t rft_state;

	switch (rft_state) {
		case 0:
			get_rft_var_0_data_group(&station_logo_group, blocks);
			break;
		case 1:
			get_rft_var_1_data_group(&station_logo_group, blocks);
			break;
#if 0
		case 2:
			get_rft_var_2_15_data_group(&station_logo_group, blocks);
			break;
#endif

		default:
			get_rft_file_data_group(&station_logo_group, blocks);
			break;
	}

	rft_state++;
	if (rft_state == 15) rft_state = 0;
}

/*
 * RDS 2 group sequence
 */
static void get_rds2_group(uint8_t stream_num, uint16_t *blocks) {

	switch (stream_num) {
	case 1:
	case 2:
	case 3:
	default:
		get_rft_stream(blocks);
		break;
	}

#ifdef RDS2_DEBUG
	fprintf(stderr, "Stream %u: %04x %04x %04x %04x\n",
		stream_num, blocks[0], blocks[1], blocks[2], blocks[3]);
#endif
}

void get_rds2_bits(uint8_t stream, uint8_t *bits) {
	static uint16_t out_blocks[GROUP_LENGTH];
	get_rds2_group(stream, out_blocks);
	add_checkwords(out_blocks, bits, true);
}

void init_rds2_encoder() {
	/* create a new stream */
	init_rft(&station_logo_group,
		0 /* pipe number */,
		station_logo,
		station_logo_len
	);
}
void exit_rds2_encoder() {
	exit_rft(&station_logo_group);
}
