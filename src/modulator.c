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
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "rds_waveform.h"
#include "modulator.h"

static struct rds_t **rds_ctx;

/*
 * Create the RDS objects
 *
 */
void init_rds_objects() {
	rds_ctx = malloc(4 * sizeof(struct rds_t));

	for (uint8_t i = 0; i < 4; i++) {
		rds_ctx[i] = malloc(sizeof(struct rds_t));
		rds_ctx[i]->bit_buffer = malloc(BITS_PER_GROUP);
		rds_ctx[i]->sample_buffer =
			malloc(SAMPLE_BUFFER_SIZE * sizeof(float));
	}
}

void exit_rds_objects() {
	for (uint8_t i = 0; i < 4; i++) {
		free(rds_ctx[i]->sample_buffer);
		free(rds_ctx[i]->bit_buffer);
		free(rds_ctx[i]);
	}

	free(rds_ctx);
}

/* Get an RDS sample. This generates the envelope of the waveform using
 * pre-generated elementary waveform samples.
 */
float get_rds_sample(uint8_t stream_num) {
	struct rds_t *rds = rds_ctx[stream_num];
	uint16_t idx;
	float *cur_waveform;
	float sample;

	if (rds->sample_count == SAMPLES_PER_BIT) {
		if (rds->bit_pos == BITS_PER_GROUP) {
#ifdef RDS2
			if (stream_num > 0) {
				get_rds2_bits(stream_num, rds->bit_buffer);
			} else {
				get_rds_bits(rds->bit_buffer);
			}
#else
			get_rds_bits(rds->bit_buffer);
#endif
			rds->bit_pos = 0;
		}

		/* do differential encoding */
		rds->cur_bit = rds->bit_buffer[rds->bit_pos++];
		rds->prev_output = rds->cur_output;
		rds->cur_output = rds->prev_output ^ rds->cur_bit;

		idx = rds->in_sample_index;
		cur_waveform = biphase_waveform[rds->cur_output];

		for (uint16_t i = 0; i < FILTER_SIZE; i++) {
			rds->sample_buffer[idx++] += *cur_waveform++;
			if (idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		rds->in_sample_index += SAMPLES_PER_BIT;
		if (rds->in_sample_index == SAMPLE_BUFFER_SIZE)
			rds->in_sample_index = 0;

		rds->sample_count = 0;
	}
	rds->sample_count++;

	sample = rds->sample_buffer[rds->out_sample_index];
	rds->sample_buffer[rds->out_sample_index++] = 0;
	if (rds->out_sample_index == SAMPLE_BUFFER_SIZE)
		rds->out_sample_index = 0;

	return sample;
}
