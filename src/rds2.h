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

#define MAX_FILE_NAME_LEN	64 /* does not include terminating null char */
#define MAX_IMAGE_LEN		163840
#define MAX_CRC_CHUNK_ADDR	511

/* RFT CRC mode */
enum rft_crc_mode {
	RFT_CRC_MODE_ENTIRE_FILE,	/* over entire file (universal) */
	RFT_CRC_MODE_16_GROUPS,		/* chunks of 16 groups (size <= 40,960) */
	RFT_CRC_MODE_32_GROUPS,		/* chunks of 32 groups (40,960 < size <= 81,920) */
	RFT_CRC_MODE_64_GROUPS,		/* chunks of 64 groups (size > 81,960) */
	RFT_CRC_MODE_128_GROUPS,	/* chunks of 128 groups (size > 81,960) */
	RFT_CRC_MODE_256_GROUPS,	/* chunks of 256 groups (size > 81,960) */
	RFT_CRC_MODE_RFU,		/* reserved */
	RFT_CRC_MODE_AUTO		/* automatic between 1-3 based on size */
};

/* RDS2 File Transfer */
typedef struct rft_t {
	uint8_t channel;

	char *file_path;
	unsigned char *file_data;
	size_t file_len;
	uint8_t file_version;
	uint8_t file_id;
	uint8_t variant_code;

	uint16_t pkt_size;
	uint16_t pkt_size_rem;
	uint16_t seg_addr_img;
	uint16_t num_segs;

	/* CRC chunk map */
	bool use_crc;
	uint8_t crc_mode;
	uint16_t crc_chunk_addr;
	uint16_t num_crc_chunks;
	uint8_t *crc_chunks;
	uint16_t *crcs;
} rft_t;

extern void get_rds2_bits(uint8_t stream_num, uint8_t *bits);
extern void init_rds2_encoder(char *station_logo_path, char *album_art_path);
extern void exit_rds2_encoder();
