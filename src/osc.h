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

/* context for MPX oscillator */
typedef struct osc_t {
	/* the sample rate at which the oscillator operates */
	uint32_t sample_rate;

	/*
	 * frequency for this instance
	 *
	 */
	float freq;

	/*
	 * Arrays of carrier wave constants
	 *
	 */
	float *sin_wave;
	float *cos_wave;

	/*
	 * Wave phase
	 *
	 */
	uint16_t cur;
	uint16_t max;
} osc_t;

extern void osc_init(struct osc_t *osc, uint32_t sample_rate,
	const float freq);
extern float osc_get_sin(struct osc_t *osc);
extern float osc_get_cos(struct osc_t *osc);
extern void osc_update_pos(struct osc_t *osc);
extern void osc_exit(struct osc_t *osc);
