/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2021 Anthony96922
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

/*
 * Stuff common for both RDS and RDS2
 *
 */

// RDS PTY list
static char *ptys[2][32] = {
	// NRSC RBDS
	{
		"None", "News", "Information", "Sports",
		"Talk", "Rock", "Classic rock", "Adult hits",
		"Soft rock" , "Top 40", "Country", "Oldies",
		"Soft music", "Nostalgia", "Jazz", "Classical",
		"R&B", "Soft R&B", "Language", "Religious music",
		"Religious talk", "Personality", "Public", "College",
		"Spanish talk", "Spanish music", "Hip-Hop", "Unassigned",
		"Unassigned", "Weather", "Emergency test", "Emergency"
	},
	// ETSI
	{
		"None", "News", "Current affairs", "Information",
		"Sport", "Education", "Drama", "Culture", "Science",
		"Varied", "Pop music", "Rock music", "Easy listening",
		"Light classical", "Serious classical", "Other music",
		"Weather", "Finance", "Children's programs",
		"Social affairs", "Religion", "Phone-in", "Travel",
		"Leisure", "Jazz music", "Country music",
		"National music", "Oldies music", "Folk music",
		"Documentary", "Alarm test", "Alarm"
	}
};

char *get_pty(uint8_t region, uint8_t pty) {
	return ptys[region][pty];
}

static uint16_t offset_words[] = {
	0x0FC, /*  A  */
	0x198, /*  B  */
	0x168, /*  C  */
	0x1B4, /*  D  */
	0x350  /*  C' */
};

/* Classical CRC computation */
static inline uint16_t crc(uint16_t block) {
	uint16_t crc = 0;

	for (int j = 0; j < BLOCK_SIZE; j++) {
		uint8_t bit = (block & MSB_BIT) != 0;
		block <<= 1;

		uint8_t msb = (crc >> (POLY_DEG-1)) & 1;
		crc <<= 1;
		if ((msb ^ bit) != 0) crc ^= POLY;
	}

	return crc;
}

uint16_t crc16(uint8_t *data, size_t len) {
	uint16_t crc = 0xffff;

	for (size_t i = 0; i < len; i++) {
		crc = (crc >> 8) | (crc << 8);
		crc ^= data[i];
		crc ^= (crc & 0xff) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xff) << 4) << 1;
	}

	return crc ^ 0xffff;
}

/* Calculate the checkword for each block and emit the bits */
void add_checkwords(uint16_t *blocks, uint8_t *bits) {
	uint16_t block, check, offset_word;

	for (int i = 0; i < GROUP_LENGTH; i++) {
		block = blocks[i];
		offset_word = offset_words[i];

		/* Group version B needs C' */
		if (i == 3 && ((blocks[1] >> 11) & 1))
			offset_word = offset_words[4];

		check = crc(block) ^ offset_word;
		for (int j = 0; j < BLOCK_SIZE; j++) {
			*bits++ = ((block & (1 << (BLOCK_SIZE - 1))) != 0);
			block <<= 1;
		}
		for (int j = 0; j < POLY_DEG; j++) {
			*bits++ = ((check & (1 << (POLY_DEG - 1))) != 0);
			check <<= 1;
		}
	}
}

/*
 * PI code calculator
 *
 * Calculates the PI code from a station's callsign.
 *
 * See
 * https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-4-b.pdf
 * for more information.
 *
 */
uint16_t callsign2pi(char *callsign) {
	uint16_t pi_code = 0;

	if (callsign[0] == 'K' || callsign[0] == 'k') {
		pi_code += 4096;
	} else if (callsign[0] == 'W' || callsign[0] == 'w') {
		pi_code += 21672;
	} else {
		return 0;
	}

	/* Change nibbles to base-26 decimal */
	pi_code +=
		(callsign[1] - (callsign[1] >= 'a' ? 0x61 : 0x41)) * 676 +
		(callsign[2] - (callsign[2] >= 'a' ? 0x61 : 0x41)) * 26 +
		(callsign[3] - (callsign[3] >= 'a' ? 0x61 : 0x41));

	/* Call letter exceptions */

	/* When 3rd char is 0 */
	if ((pi_code & 0x0F00) == 0) {
		pi_code = 0xA000 +
			((pi_code & 0xF000) >> 4) + (pi_code & 0x00FF);
	}

	/* When 1st & 2nd chars are 0 */
	if ((pi_code & 0x00FF) == 0) {
		pi_code = 0xAF00 + ((pi_code & 0xFF00) >> 8);
	}

	return pi_code;
}

/*
 * TMC stuff (for future use)
 *
 * Based on the the implementation described here:
 * http://www.windytan.com/2013/05/a-determined-hacker-decrypts-rds-tmc.html
 */

/*
 * bit rotation operations
 *
 */
static inline uint16_t rotr16(uint16_t value, uint8_t count) {
	return value >> (count & 15) | value << (16 - (count & 15));
}

static inline uint16_t rotl16(uint16_t value, uint8_t count) {
	return value << (count & 15) | value >> (16 - (count & 15));
}

uint16_t tmc_encrypt(uint16_t loc, uint16_t key) {
	uint16_t enc_loc;
	uint16_t p1, p2;

	p1 = rotr16(loc, key >> 12);
	p2 = (key & 0xff) << ((key >> 8) & 0xf);
	enc_loc = p1 ^ p2;

	return enc_loc;
}

uint16_t tmc_decrypt(uint16_t loc, uint16_t key) {
	uint16_t dec_loc;
	uint16_t p1, p2;

	p1 = (key & 0xff) << ((key >> 8) & 0xf);
	p2 = loc ^ p1;
	dec_loc = rotl16(p2, key >> 12);

	return dec_loc;
}

/*
 * get the timezone difference
 *
 */
int8_t get_tzoffset(uint8_t utc_h, uint8_t loc_h) {
	int8_t tz_off = 0;
	uint8_t is_neg = 0;
	uint8_t hours = loc_h;

	while (hours != utc_h) {
		hours++;
		tz_off++;
		if (hours > 23) {
			hours = 0;
			is_neg = 1;
		}
	}

	return is_neg ? -tz_off : tz_off;
}
