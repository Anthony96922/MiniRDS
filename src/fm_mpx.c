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

#include "rds.h"
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "osc.h"

/*
 * Local oscillator objects
 * this is where the MPX waveforms are stored
 *
 */
static struct osc_t osc_19k;
static struct osc_t osc_57k;
#ifdef RDS2
static struct osc_t osc_67k;
static struct osc_t osc_71k;
static struct osc_t osc_76k;
#endif

static float mpx_vol;

void set_output_volume(uint8_t vol) {
	if (vol > 100) vol = 100;
	mpx_vol = ((float)vol / 100.0f);
}

/* subcarrier volumes */
static float volumes[] = {
	0.09f, /* pilot tone: 9% */
	0.09f, /* RDS: 4.5% modulation */
#ifdef RDS2
	/* RDS2 */
	0.09f,
	0.09f,
	0.09f
#endif
};

void set_carrier_volume(uint8_t carrier, uint8_t new_volume) {
	/* check for valid index */
	if (carrier > NUM_SUBCARRIERS) return;

	/* don't allow levels over 15% */
	if (new_volume >= 15) volumes[carrier] = 0.09f;

	volumes[carrier] = (float)new_volume / 100.0f;
}

void fm_mpx_init(uint32_t sample_rate) {
	/* initialize the subcarrier oscillators */
	osc_init(&osc_19k, sample_rate, 19000.0f);
	osc_init(&osc_57k, sample_rate, 57000.0f);
#ifdef RDS2
	osc_init(&osc_67k, sample_rate, 66500.0f);
	osc_init(&osc_71k, sample_rate, 71250.0f);
	osc_init(&osc_76k, sample_rate, 76000.0f);
#endif
}

void fm_rds_get_frames(float *outbuf, size_t num_frames) {
	uint16_t j = 0;
	float out;

	for (size_t i = 0; i < num_frames; i++) {
		out = 0.0f;

		/* Pilot tone for calibration */
		out += osc_get_cos(&osc_19k) * volumes[0];

		out += osc_get_cos(&osc_57k) * get_rds_sample(0) * volumes[1];
#ifdef RDS2
		out += osc_get_cos(&osc_67k) * get_rds_sample(1) * volumes[2];
#if 0
		out += osc_get_cos(&osc_71k) * get_rds_sample(2) * volumes[3];
		out += osc_get_cos(&osc_76k) * get_rds_sample(3) * volumes[4];
#endif
#endif

		/* update oscillator */
		osc_update_pos(&osc_19k);
		osc_update_pos(&osc_57k);
#ifdef RDS2
		osc_update_pos(&osc_67k);
		osc_update_pos(&osc_71k);
		osc_update_pos(&osc_76k);
#endif

		/* clipper */
		out = fminf(+1.0f, out);
		out = fmaxf(-1.0f, out);

		/* adjust volume and put into both channels */
		outbuf[j+0] = outbuf[j+1] = out * mpx_vol;
		j += 2;

	}
}

void fm_mpx_exit() {
	osc_exit(&osc_19k);
	osc_exit(&osc_57k);
#ifdef RDS2
	osc_exit(&osc_67k);
	osc_exit(&osc_71k);
	osc_exit(&osc_76k);
#endif
}
