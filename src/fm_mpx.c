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

void fm_mpx_init(struct mpx_t *mpx_ctx,
	struct rds_obj_t *my_rds, uint32_t sample_rate, size_t frames) {

	/* initialize the subcarrier oscillators */
	osc_init(mpx_ctx->osc_19k, sample_rate, 19000.0f);
	osc_init(mpx_ctx->osc_57k, sample_rate, 57000.0f);
#ifdef RDS2
	osc_init(mpx_ctx->osc_67k, sample_rate, 66500.0f);
	osc_init(mpx_ctx->osc_71k, sample_rate, 71250.0f);
	osc_init(mpx_ctx->osc_76k, sample_rate, 76000.0f);
#endif
	mpx_ctx->sample_rate = sample_rate;
	mpx_ctx->num_frames = frames;
	mpx_ctx->out_frames = malloc(frames * sample_rate * sizeof(float));
	mpx_ctx->sc_vol = malloc(NUM_SUBCARRIERS * sizeof(float));
	mpx_ctx->output_vol = 1.0f;

	/* subcarrier volumes */
	mpx_ctx->sc_vol[0] = 0.09f; /* pilot tone: 9% */
	mpx_ctx->sc_vol[1] = 0.09f; /* RDS: 4.5% modulation */
#ifdef RDS2
	/* RDS2 */
	mpx_ctx->sc_vol[2] = 0.09f;
	mpx_ctx->sc_vol[3] = 0.09f;
	mpx_ctx->sc_vol[4] = 0.09f;
#endif

	/* source RDS context */
	mpx_ctx->rds = my_rds;
}

void fm_rds_get_frames(struct mpx_t *mpx_ctx) {
	float *outbuf = mpx_ctx->out_frames;
	size_t j = 0;
	float out;

	for (size_t i = 0; i < mpx_ctx->num_frames; i++) {
		out = 0.0f;

		/* Pilot tone for calibration */
		out += osc_get_cos(mpx_ctx->osc_19k) * mpx_ctx->sc_vol[0];

		out += osc_get_cos(mpx_ctx->osc_57k) *
			get_rds_sample(mpx_ctx->rds, 0) * mpx_ctx->sc_vol[1];
#ifdef RDS2
		out += osc_get_cos(mpx_ctx->osc_67k) *
			get_rds_sample(mpx_ctx->rds, 1) * mpx_ctx->sc_vol[2];
#if 0
		out += osc_get_cos(mpx_mpx->osc_71k) *
			get_rds_sample(mpx_ctx->rds, 2) * mpx_ctx->sc_vol[3];
		out += osc_get_cos(mpx_ctx->osc_76k) *
			get_rds_sample(mpx_ctx->rds, 3) * mpx_ctx->sc_vol[4];
#endif
#endif

		/* update oscillator */
		osc_update_pos(mpx_ctx->osc_19k);
		osc_update_pos(mpx_ctx->osc_57k);
#ifdef RDS2
		osc_update_pos(mpx_ctx->osc_67k);
		osc_update_pos(mpx_ctx->osc_71k);
		osc_update_pos(mpx_ctx->osc_76k);
#endif

		/* clipper */
		out = fminf(+1.0f, out);
		out = fmaxf(-1.0f, out);

		/* adjust volume and put into both channels */
		outbuf[j+0] = outbuf[j+1] = out * mpx_ctx->output_vol;
		j += 2;

	}
}

void fm_mpx_exit(struct mpx_t *mpx_ctx) {
	osc_exit(mpx_ctx->osc_19k);
	osc_exit(mpx_ctx->osc_57k);
#ifdef RDS2
	osc_exit(mpx_ctx->osc_67k);
	osc_exit(mpx_ctx->osc_71k);
	osc_exit(mpx_ctx->osc_76k);
#endif
	free(mpx_ctx->out_frames);
	free(mpx_ctx->sc_vol);
}
