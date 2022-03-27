/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
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
#include "osc.h"

/*
 * Code for MPX oscillator
 *
 * This uses lookup tables to speed up the waveform generation
 *
 */

/*
 * DDS function generator
 *
 * Create wave constants for a given frequency
 */
static void create_wave(uint32_t rate, float freq,
		float *sin_wave, float *cos_wave, uint16_t *max_phase) {
	float sin_sample, cos_sample;
	// used to determine if we have completed a cycle
	uint8_t zero_crossings = 0;
	uint16_t i;
	double w = M_2PI * freq;
	double phase;

	// First value of a sine wave is always 0
	*sin_wave++ = 0.0f;
	*cos_wave++ = 1.0f;

	for (i = 1; i < rate; i++) {
		phase = i / (double)rate;
		sin_sample = sin(w * phase);
		cos_sample = cos(w * phase);
		if (sin_sample > -0.1e-4 && sin_sample < 0.1e-4) {
			if (++zero_crossings == 2) break;
			*sin_wave++ = 0.0f;
		} else {
			*sin_wave++ = sin_sample;
		}
		*cos_wave++ = cos_sample;
	}

	*max_phase = i;
}

/*
 * Create waveform lookup tables for frequencies in the array
 *
 */
void osc_init(struct osc_t *osc_ctx, uint32_t sample_rate, const float freq) {

	osc_ctx->sample_rate = sample_rate;

	/*
	 * waveform tables
	 *
	 * cosine and sine
	 */
	osc_ctx->sin_wave = malloc(osc_ctx->sample_rate * sizeof(float));
	osc_ctx->cos_wave = malloc(osc_ctx->sample_rate * sizeof(float));

	/*
	 * phase table
	 *
	 * current and max
	 */
	osc_ctx->phases = malloc(2 * sizeof(uint16_t));

	osc_ctx->phases[CURRENT] = 0;

	/* create waveform data and load into lookup tables */
	create_wave(osc_ctx->sample_rate, freq,
		osc_ctx->sin_wave, osc_ctx->cos_wave,
		&osc_ctx->phases[MAX]);
}

/*
 * Get a single waveform sample
 *
 * Cosine is needed for SSB generation
 *
 */
float osc_get_cos_wave(struct osc_t *osc_ctx) {
	uint16_t cur_phase = osc_ctx->phases[CURRENT];
	return osc_ctx->cos_wave[cur_phase];
}

float osc_get_sin_wave(struct osc_t *osc_ctx) {
	uint16_t cur_phase = osc_ctx->phases[CURRENT];
	return osc_ctx->sin_wave[cur_phase];
}

/*
 * Shift the oscillator to the next phase
 *
 */
void osc_update_phase(struct osc_t *osc_ctx) {
	if (++osc_ctx->phases[CURRENT] == osc_ctx->phases[MAX])
		osc_ctx->phases[CURRENT] = 0;
}

/*
 * Unload waveform and phase tables
 *
 */
void osc_exit(struct osc_t *osc_ctx) {
	free(osc_ctx->sin_wave);
	free(osc_ctx->cos_wave);
	free(osc_ctx->phases);
}
