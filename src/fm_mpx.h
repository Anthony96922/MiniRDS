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

/* MPX */
#define NUM_MPX_FRAMES_IN	1024
#define NUM_MPX_FRAMES_OUT	(NUM_MPX_FRAMES_IN * 2)

/*
 * The sample rate at which the MPX generation runs at
 */
#define MPX_SAMPLE_RATE		RDS_SAMPLE_RATE

#define OUTPUT_SAMPLE_RATE	192000

#ifdef RDS2
#define NUM_SUBCARRIERS		5
#else
#define NUM_SUBCARRIERS		1
#endif

/* FM MPX object */
typedef struct mpx_t {

	uint32_t sample_rate;

	/*
	 * Local oscillator objects
	 * this is where the MPX waveforms are stored
	 *
	 */
	struct osc_t *osc_19k;
	struct osc_t *osc_57k;
#ifdef RDS2
	struct osc_t *osc_67k;
	struct osc_t *osc_71k;
	struct osc_t *osc_76k;
#endif

	float *sc_vol;

	float output_vol;

	size_t num_frames;
	float *out_frames;

	/* the RDS object to reference */
	struct rds_obj_t *rds;
} mpx_t;

extern void fm_mpx_init(struct mpx_t *mpx_ctx,
	struct rds_obj_t *my_rds, uint32_t sample_rate, size_t frames);
extern void fm_mpx_get_frames(struct mpx_t *mpx_ctx);
extern void fm_mpx_exit(struct mpx_t *mpx_ctx);
