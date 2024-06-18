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

#ifdef RDS2
#define NUM_STREAMS	4
#else
#define NUM_STREAMS	1
#endif

/* RDS signal context */
typedef struct rds_t {
	uint8_t *bit_buffer; /* BITS_PER_GROUP */
	uint8_t bit_pos;
	float *sample_buffer; /* SAMPLE_BUFFER_SIZE */
	uint8_t prev_output;
	uint8_t cur_output;
	uint8_t cur_bit;
	uint8_t sample_count;
	uint16_t in_sample_index;
	uint16_t out_sample_index;
	uint8_t symbol_shift;
	float *symbol_shift_buf;
	uint8_t symbol_shift_buf_idx;
} rds_t;

extern void init_rds_objects();
extern void exit_rds_objects();
