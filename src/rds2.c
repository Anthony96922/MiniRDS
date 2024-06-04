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

/* fallback station logo */
extern unsigned char *station_logo;
extern unsigned int station_logo_len;

/* station logo */
static struct rft_t station_logo_stream;

static void init_rft(struct rft_t *rft, uint8_t file_id,
	bool usecrc, uint8_t crc_mode, char *file_path) {
	uint32_t seg_counter = 0;
	uint32_t crc_chunk_counter = 0;
	uint16_t crc_chunk_size = 0;
	FILE *image_file_hdlr;

	rft->file_path = malloc(MAX_FILE_NAME_LEN + 1);
	rft->file_path[MAX_FILE_NAME_LEN] = 0;
	rft->file_data = malloc(MAX_IMAGE_LEN);
	rft->crcs = malloc(MAX_CRC_CHUNK_ADDR * sizeof(uint16_t));

	/* unused data must be padded with zeroes */
	memset(rft->file_data, 0, MAX_IMAGE_LEN);

	strncpy(rft->file_path, file_path, MAX_FILE_NAME_LEN);

	/* open file in binary mode */
	image_file_hdlr = fopen(file_path, "rb");
	if (image_file_hdlr != NULL) {
		fseek(image_file_hdlr, 0, SEEK_END);
		rft->file_len = ftell(image_file_hdlr);
		rewind(image_file_hdlr);

		/* image cannot be larger than 163840 bytes */
		if (rft->file_len > MAX_IMAGE_LEN) {
#ifdef RDS2_DEBUG
			fprintf(stderr, "W: Image too large!\n");
#endif
			rft->file_len = MAX_IMAGE_LEN;
		}

		/* copy image data to local buffer */
		fread(rft->file_data, rft->file_len, 1, image_file_hdlr);

		fclose(image_file_hdlr);
	} else { /* fallback in case file can't be read at startup */
		rft->file_len = station_logo_len;

		if (rft->file_len > MAX_IMAGE_LEN) {
			rft->file_len = MAX_IMAGE_LEN;
		}

		memcpy(rft->file_data, station_logo, rft->file_len);
	}

	/* keep track of file lengths so we know when to toggle */
	rft->prev_file_len = rft->file_len;
	rft->channel = 0;
	rft->file_id = file_id;
	rft->toggle = 0;

	/* determine how many segments we need */
	while (seg_counter < rft->file_len) {
		seg_counter += 5;
		rft->num_segs++;
	}

	rft->use_crc = usecrc;

	/* if CRC is disabled exit early */
	if (!rft->use_crc) {
		rft->crc_mode = 0;
		rft->num_crc_chunks = 1; /* only 1 */
		rft->crcs[0] = 0x0000;
		return;
	}

	rft->crc_mode = crc_mode & 7;

	switch (rft->crc_mode) {
	case RFT_CRC_MODE_ENTIRE_FILE:
		/* no-op */
		break;

	case RFT_CRC_MODE_16_GROUPS:
		crc_chunk_size = 16;
		break;

	case RFT_CRC_MODE_32_GROUPS:
		crc_chunk_size = 32;
		break;

	case RFT_CRC_MODE_64_GROUPS:
		crc_chunk_size = 64;
		break;

	case RFT_CRC_MODE_128_GROUPS:
		crc_chunk_size = 128;
		break;

	case RFT_CRC_MODE_256_GROUPS:
		crc_chunk_size = 256;
		break;

	case RFT_CRC_MODE_RFU: /* reserved - use auto mode */
	case RFT_CRC_MODE_AUTO:
		if (rft->file_len >= 81920) {
			crc_chunk_size = 64;
			rft->crc_mode = RFT_CRC_MODE_64_GROUPS;
		} else if (rft->file_len > 40960) {
			crc_chunk_size = 32;
			rft->crc_mode = RFT_CRC_MODE_32_GROUPS;
		} else {
			crc_chunk_size = 16;
			rft->crc_mode = RFT_CRC_MODE_16_GROUPS;
		}
		break;
	}

	if (rft->crc_mode) { /* chunked modes */
		/* RFT packet size = chunk size * 5 data bytes per group */
		rft->pkt_size = crc_chunk_size * 5;

		/* the remainder (if file size is not a multiple of pkt_size) */
		rft->pkt_size_rem = rft->file_len % rft->pkt_size;

		/* calculate number of CRC chunks */
		while (crc_chunk_counter <= rft->file_len) {
			crc_chunk_counter += rft->pkt_size;
			rft->num_crc_chunks++;
		}

		rft->crc_chunks = malloc(rft->pkt_size);

		/* calculate CRC's for chunks */
		for (uint16_t i = 0; i < rft->num_crc_chunks; i++) {
			memset(rft->crc_chunks, 0, rft->pkt_size);
			memcpy(rft->crc_chunks,
				rft->file_data + i * rft->pkt_size, rft->pkt_size);
			if ((i == rft->num_crc_chunks - 1) && rft->pkt_size_rem) {
				/* last chunk may have less bytes */
				rft->crcs[i] = crc16(rft->crc_chunks, rft->pkt_size_rem);
			} else {
				rft->crcs[i] = crc16(rft->crc_chunks, rft->pkt_size);
			}
		}

		free(rft->crc_chunks);
	} else { /* CRC of entire file */
		rft->num_crc_chunks = 1; /* only 1 */
		rft->crcs[0] = crc16(rft->file_data, rft->file_len);
	}
}

static void update_rft(struct rft_t *rft) {
	size_t file_size;
	FILE *image_file_hdlr;

	/* open file in binary mode */
	image_file_hdlr = fopen(rft->file_path, "rb");
	if (image_file_hdlr == NULL) {
		return;
	}

	fseek(image_file_hdlr, 0, SEEK_END);
	file_size = ftell(image_file_hdlr);
	rewind(image_file_hdlr);

	/* file didn't change so return early */
	if (file_size == rft->prev_file_len) {
		fclose(image_file_hdlr);
		return;
	}

	/* update size field */
	rft->file_len = file_size;

	/* image cannot be larger than 163840 bytes */
	if (rft->file_len > MAX_IMAGE_LEN) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "W: Image too large!\n");
#endif
		rft->file_len = MAX_IMAGE_LEN;
	}

	/* copy image data to local buffer */
	fread(rft->file_data, rft->file_len, 1, image_file_hdlr);

	fclose(image_file_hdlr);

	if (rft->crc_mode) {
		/* recalculate CRC's for chunks */
		for (uint16_t i = 0; i < rft->num_crc_chunks; i++) {
			memset(rft->crc_chunks, 0, rft->pkt_size);
			memcpy(rft->crc_chunks,
				rft->file_data + i * rft->pkt_size, rft->pkt_size);
			if ((i == rft->num_crc_chunks - 1) && rft->pkt_size_rem) {
				/* last chunk may have less bytes */
				rft->crcs[i] = crc16(rft->crc_chunks, rft->pkt_size_rem);
			} else {
				rft->crcs[i] = crc16(rft->crc_chunks, rft->pkt_size);
			}
		}

		free(rft->crc_chunks);
	} else { /* CRC of entire file */
		rft->num_crc_chunks = 1; /* only 1 */
		rft->crcs[0] = crc16(rft->file_data, rft->file_len);
	}

	if (++rft->file_version > 64) {
		rft->file_version = 0;
	}

	rft->toggle ^= 1;
}

static void exit_rft(struct rft_t *rft) {
	free(rft->file_data);
	free(rft->file_path);
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
	blocks[2] |= ((rft->use_crc ? 1 : 0) & INT8_L1) << 11; /* CRC */
	blocks[2] |= (rft->file_version & INT8_L3) << 8; /* file version */
	blocks[2] |= (rft->file_id & INT8_L6) << 2; /* file ID */
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
	blocks[2] |= rft->crc_chunk_addr & INT16_L9;

	/* CRC */
	blocks[3] = rft->crcs[rft->crc_chunk_addr];

	if (++rft->crc_chunk_addr > rft->num_crc_chunks) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "File CRC sending complete\n");
#endif
		rft->crc_chunk_addr = 0;
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
	blocks[0] |= (rft->toggle & INT16_L1) << 7;

	/* segment address */
	blocks[0] |= ((rft->seg_addr_img & INT16_U8) >> 8) & INT16_L7;
	blocks[1] = (rft->seg_addr_img & INT16_L8) << 8;

	/* image data */
	blocks[1] |= rft->file_data[rft->seg_addr_img * 5 + 0];
	blocks[2] =  rft->file_data[rft->seg_addr_img * 5 + 1] << 8;
	blocks[2] |= rft->file_data[rft->seg_addr_img * 5 + 2];
	blocks[3] =  rft->file_data[rft->seg_addr_img * 5 + 3] << 8;
	blocks[3] |= rft->file_data[rft->seg_addr_img * 5 + 4];

	if (++rft->seg_addr_img > rft->num_segs) {
#ifdef RDS2_DEBUG
		fprintf(stderr, "File sending complete\n");
#endif
		rft->seg_addr_img = 0;
		update_rft(rft);
	}
}

static void get_rft_stream(uint16_t *blocks) {
	static uint8_t rft_state;

	switch (rft_state) {
		case 0:
			get_rft_var_0_data_group(&station_logo_stream, blocks);
			break;
		case 1:
			get_rft_var_1_data_group(&station_logo_stream, blocks);
			break;
		case 2:
			get_rft_var_0_data_group(&station_logo_stream, blocks);
			break;
		case 3:
			get_rft_var_1_data_group(&station_logo_stream, blocks);
			break;

		default:
			get_rft_file_data_group(&station_logo_stream, blocks);
			break;
	}

	rft_state++;
	if (rft_state == 50) rft_state = 0;
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

void init_rds2_encoder(char *station_logo_path) {
	/* create a new stream for the station logo */
	init_rft(&station_logo_stream,
		0 /* file ID */,
		false /* don't use crc */,
		RFT_CRC_MODE_AUTO,
		station_logo_path
	);
}

void exit_rds2_encoder() {
	exit_rft(&station_logo_stream);
}
