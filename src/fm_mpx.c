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

static float mpx_vol;

// MPX carrier index
enum mpx_carrier_index {
	CARRIER_19K,
	CARRIER_57K,
#ifdef RDS2
	CARRIER_67K,
	CARRIER_71K,
	CARRIER_76K,
#endif
};

static const float carrier_frequencies[] = {
	19000.0, // pilot tone
	57000.0, // RDS

#ifdef RDS2
	// RDS 2
	66500.0, // stream 1
	71250.0, // stream 2
	76000.0,  // stream 2
#endif
	0.0 // terminator
};

/*
 * Local cscillator object
 * this is where the MPX waveforms are stored
 *
 */
static struct osc_t mpx_osc;

void set_output_volume(uint8_t vol) {
	if (vol > 100) vol = 100;
	mpx_vol = (vol / 100.0f);
}

// subcarrier volumes
static float volumes[] = {
	0.09, // pilot tone: 9% modulation
	0.09, // RDS: 4.5% modulation

#ifdef RDS2
	0.09, // RDS 2
	0.09,
	0.09
#endif
};

void set_carrier_volume(uint8_t carrier, uint8_t new_volume) {
	if (carrier > 4) return;
	if (new_volume >= 15) volumes[carrier] = 0.09f;
	volumes[carrier] = new_volume / 100.0f;
}

void fm_mpx_init() {
	init_osc(&mpx_osc, MPX_SAMPLE_RATE, carrier_frequencies);
}

void fm_rds_get_frames(float *outbuf) {
	uint16_t j = 0;
	float out;

	for (size_t i = 0; i < NUM_MPX_FRAMES_IN; i++) {
		out = 0.0f;

		// Pilot tone for calibration
		out += get_wave(&mpx_osc, CARRIER_19K, 1) * volumes[0];

		out += get_wave(&mpx_osc, CARRIER_57K, 1) * get_rds_sample(0) * volumes[1];
#ifdef RDS2
		out += get_wave(&mpx_osc, CARRIER_67K, 1) * get_rds_sample(1) * volumes[2];
		out += get_wave(&mpx_osc, CARRIER_71K, 1) * get_rds_sample(2) * volumes[3];
		out += get_wave(&mpx_osc, CARRIER_76K, 1) * get_rds_sample(3) * volumes[4];
#endif

		update_osc_phase(&mpx_osc);

		outbuf[j+0] = outbuf[j+1] = out * mpx_vol;
		j += 2;

	}
}

void fm_mpx_exit() {
	exit_osc(&mpx_osc);
	exit_rds_encoder();
}
