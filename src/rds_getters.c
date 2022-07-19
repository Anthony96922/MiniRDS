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

/* RDS getters */

void show_af_list(struct rds_obj_t *rds_obj) {
	struct rds_af_t af_list = *rds_obj->af_list;
	float freq;
	uint8_t af_is_lf_mf = 0;

	fprintf(stderr, "AF: %u,", af_list.num_afs);

	for (uint8_t i = 0; i < af_list.num_entries; i++) {
		if (af_list.afs[i] == AF_CODE_LFMF_FOLLOWS) {
			// The next AF will be for LF/MF
			af_is_lf_mf = 1;
		} else {
			if (af_is_lf_mf) {
#ifdef RBDS
			// MF (FCC)
			freq = 530.0f + ((float)(af_list.afs[i] - 16) * 10.0f);
				fprintf(stderr, " (MF)%.0f", freq);
#else
				if (af_list.afs[i] >= 1 && af_list.afs[i] <= 15) { // LF
					freq = 153.0f + ((float)(af_list.afs[i] -  1) * 9.0f);
					fprintf(stderr, " (LF)%.0f", freq);
				} else { //if (af_list.afs[i] >= 16 && af_list.afs[i] <= 135) { // MF
					freq = 531.0f + ((float)(af_list.afs[i] - 16) * 9.0f);
					fprintf(stderr, " (MF)%.0f", freq);
				}
#endif
			} else {
				// FM
				freq = (float)((uint16_t)af_list.afs[i] + 875) / 10.0f;
				fprintf(stderr, " %.1f", freq);
			}
			af_is_lf_mf = 0;
		}
	}

	fprintf(stderr, "\n");
}
