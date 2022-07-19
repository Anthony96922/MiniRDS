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

void set_output_volume(struct mpx_t *mpx_ctx, uint8_t vol) {
        if (vol > 100) vol = 100;
        mpx_ctx->output_vol = ((float)vol / 100.0f);
}

void set_carrier_volume(struct mpx_t *mpx_ctx,
	uint8_t carrier, uint8_t new_volume) {

	/* check for valid index */
	if (carrier > NUM_SUBCARRIERS) return;

	/* don't allow levels over 15% */
	if (new_volume >= 15) new_volume = 9;

	mpx_ctx->sc_vol[carrier] = (float)new_volume / 100.0f;
}
