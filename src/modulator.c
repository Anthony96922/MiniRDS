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
#include "waveforms.h"
#include "modulator.h"

static float **waveform;

/*
 * Create the RDS objects
 *
 */
void init_rds_generator(struct rds_obj_t *rds_obj) {
	uint8_t stream_count = rds_obj->stream_count;
	struct rds_gen_t **rds_gen_ctx = rds_obj->rds_gen;

	rds_gen_ctx = malloc(stream_count * sizeof(struct rds_gen_t));

	for (uint8_t i = 0; i < stream_count; i++) {
		rds_gen_ctx[i] = malloc(sizeof(struct rds_gen_t));
		rds_gen_ctx[i]->bit_buffer = malloc(BITS_PER_GROUP);
		rds_gen_ctx[i]->sample_buffer =
			malloc(SAMPLE_BUFFER_SIZE * sizeof(float));
	}

	waveform = malloc(2 * sizeof(float));

	for (uint8_t i = 0; i < 2; i++) {
		waveform[i] = malloc(FILTER_SIZE * sizeof(float));
		for (uint16_t j = 0; j < FILTER_SIZE; j++) {
			waveform[i][j] = i ?
				+waveform_biphase[j] : -waveform_biphase[j];
		}
	}
}

void exit_rds_generator(struct rds_obj_t *rds_obj) {
	uint8_t stream_count = rds_obj->stream_count;
	struct rds_gen_t **rds_gen_ctx = rds_obj->rds_gen;

	for (uint8_t i = 0; i < stream_count; i++) {
		free(rds_gen_ctx[i]->sample_buffer);
		free(rds_gen_ctx[i]->bit_buffer);
		free(rds_gen_ctx[i]);
	}

	free(rds_gen_ctx);

	for (uint8_t i = 0; i < 2; i++) {
		free(waveform[i]);
	}

	free(waveform);
}

/* Get an RDS sample. This generates the envelope of the waveform using
 * pre-generated elementary waveform samples.
 */
float get_rds_sample(struct rds_obj_t *rds_obj, uint8_t stream_num) {
	struct rds_gen_t *gen = rds_obj->rds_gen[stream_num];
	uint16_t idx;
	float *cur_waveform;
	float sample;

	if (gen->sample_count == SAMPLES_PER_BIT) {
		if (gen->bit_pos == BITS_PER_GROUP) {
#ifdef RDS2
			if (stream_num > 0) {
				get_rds2_bits(stream_num, gen->bit_buffer);
			} else {
				get_rds_bits(gen->bit_buffer);
			}
#else
			get_rds_bits(gen->bit_buffer);
#endif
			gen->bit_pos = 0;
		}

		/* do differential encoding */
		gen->cur_bit = gen->bit_buffer[gen->bit_pos++];
		gen->prev_output = gen->cur_output;
		gen->cur_output = gen->prev_output ^ gen->cur_bit;

		idx = gen->in_sample_index;
		cur_waveform = waveform[gen->cur_output];

		for (uint16_t i = 0; i < FILTER_SIZE; i++) {
			gen->sample_buffer[idx++] += *cur_waveform++;
			if (idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		gen->in_sample_index += SAMPLES_PER_BIT;
		if (gen->in_sample_index == SAMPLE_BUFFER_SIZE)
			gen->in_sample_index = 0;

		gen->sample_count = 0;
	}
	gen->sample_count++;

	sample = gen->sample_buffer[gen->out_sample_index];
	gen->sample_buffer[gen->out_sample_index++] = 0;
	if (gen->out_sample_index == SAMPLE_BUFFER_SIZE)
		gen->out_sample_index = 0;

	return sample;
}
