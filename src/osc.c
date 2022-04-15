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
	double sin_sample, cos_sample;
	/* used to determine if we have completed a cycle */
	uint8_t zero_crossings = 0;
	uint16_t i = 1;
	const double w = M_2PI * freq;
	double phase;

	/* First value of a sine wave is always 0 */
	*sin_wave++ = 0.0f;
	*cos_wave++ = 1.0f;

	while (zero_crossings < 2 && i < rate) {
		phase = (double)i / (double)rate;
		sin_sample = sin(w * phase);
		cos_sample = cos(w * phase);
		if (sin_sample > -0.1e-4 && sin_sample < 0.1e-4) {
			zero_crossings++;
			sin_sample = 0.0f;
		}
		*sin_wave++ = (float)sin_sample;
		*cos_wave++ = (float)cos_sample;
		i++;
	}

	*max_phase = i - 1;
}

/*
 * Oscillator object initialization
 *
 */
void osc_init(struct osc_t *osc, uint32_t sample_rate, float freq) {

	/* sample rate for the objects */
	osc->sample_rate = sample_rate;

	/* waveform tables */
	osc->sin_wave = malloc(osc->sample_rate * sizeof(float));
	osc->cos_wave = malloc(osc->sample_rate * sizeof(float));

	/* set current position to 0 */
	osc->cur = 0;

	/* create waveform data and load into lookup tables */
	create_wave(osc->sample_rate, freq,
		osc->sin_wave, osc->cos_wave,
		&osc->max);
}

/*
 * Get a single waveform sample
 *
 * Cosine is needed for SSB generation
 *
 */
float osc_get_cos(struct osc_t *osc) {
	return osc->cos_wave[osc->cur];
}

float osc_get_sin(struct osc_t *osc) {
	return osc->sin_wave[osc->cur];
}

/*
 * Shift the oscillator to the next position
 *
 */
void osc_update_pos(struct osc_t *osc) {
	if (++osc->cur == osc->max) osc->cur = 0;
}

/*
 * Unload waveform tables
 *
 */
void osc_exit(struct osc_t *osc) {
	free(osc->sin_wave);
	free(osc->cos_wave);
	osc->cur = 0;
	osc->max = 0;
}
