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

/* setters */

/*
 * AF stuff
 */
uint8_t add_rds_af(struct rds_obj_t *rds_obj, float freq) {
	struct rds_af_t *af_list = rds_obj->af_list;
	uint16_t af;

	// check if the AF list is full
	if (af_list->num_afs + 1 > MAX_AFS) {
		fprintf(stderr, "Too many AF entries.\n");
		return 1;
	}

	// check if new frequency is valid
	if (freq >= 87.6f && freq <= 107.9f) { // FM
		af = (uint16_t)(freq * 10.0f) - 875;
		af_list->afs[af_list->num_entries] = af;
		af_list->num_entries += 1;
#ifdef RBDS
	} else if (freq >= 530.0f && freq <= 1610.0f) {
		af = ((uint16_t)(freq - 530.0f) / 10) + 16;
		af_list->afs[af_list->num_entries+0] = AF_CODE_LFMF_FOLLOWS;
		af_list->afs[af_list->num_entries+1] = af;
		af_list->num_entries += 2;
	} else {
		fprintf(stderr, "AF must be between 87.6-107.9 MHz "
			"or 530-1610 kHz (with 10 kHz spacing)\n");
#else
	} else if (freq >= 153.0f && freq <= 279.0f) {
		af = ((uint16_t)(freq - 153.0f) / 9) + 1;
		af_list->afs[af_list->num_entries+0] = AF_CODE_LFMF_FOLLOWS;
		af_list->afs[af_list->num_entries+1] = af;
		af_list->num_entries += 2;
	} else if (freq >= 531.0f && freq <= 1602.0f) {
		af = ((uint16_t)(freq - 531.0f) / 9) + 16;
		af_list->afs[af_list->num_entries+0] = AF_CODE_LFMF_FOLLOWS;
		af_list->afs[af_list->num_entries+1] = af;
		af_list->num_entries += 2;
	} else {
		fprintf(stderr, "AF must be between 87.6-107.9 MHz, "
			"153-279 kHz or 531-1602 kHz (with 9 kHz spacing)\n");
#endif
		return 1;
	}

	af_list->num_afs++;

	return 0;
}

void set_rds_pi(struct rds_obj_t *rds_obj, uint16_t pi_code) {
	rds_obj->data->pi = pi_code;
}

void set_rds_rt(struct rds_obj_t *rds_obj, char *rt) {
	uint8_t i = 0, len = 0;

	rds_obj->state->rt_update = 1;
	memset(rds_obj->data->rt, ' ', RT_LENGTH);
	while (*rt != 0 && len <= RT_LENGTH)
		rds_obj->data->rt[len++] = *rt++;

	if (len < RT_LENGTH) {
		rds_obj->state->rt_segments = 0;

		/* Terminate RT with '\r' (carriage return) if RT
		 * is < 64 characters long
		 */
		rds_obj->data->rt[len++] = '\r';

		/* find out how many segments are needed */
		while (i < len) {
			i += 4;
			rds_obj->state->rt_segments++;
		}
	} else {
		// Default to 16 if RT is 64 characters long
		rds_obj->state->rt_segments = 16;
	}

	rds_obj->state->rt_bursting = rds_obj->state->rt_segments;
}

#ifdef RDS2
void set_rds_ert(struct rds_obj_t *rds_obj, char *ert) {
	uint8_t i = 0, len = 0;

	if (!ert[0]) {
		memset(rds_obj->data->ert, 0, ERT_LENGTH);
		return;
	}

	rds_obj->state->ert_update = 1;
	memset(rds_obj->data->ert, '\r', ERT_LENGTH);
	while (*ert != 0 && len <= ERT_LENGTH)
		rds_obj->data->ert[len++] = *ert++;

	if (len < ERT_LENGTH) {
		rds_obj->state->ert_segments = 0;

		/* increment to allow adding an '\r' in all cases */
		len++;

		/* find out how many segments are needed */
		while (i < len) {
			i += 4;
			rds_obj->state->ert_segments++;
		}
	} else {
		/* Default to 32 if eRT is 128 characters long */
		rds_obj->state->ert_segments = 32;
	}

	rds_obj->state->ert_bursting = rds_obj->state->ert_segments;
}
#endif

void set_rds_ps(struct rds_obj_t *rds_obj, char *ps) {
	uint8_t len = 0;

	rds_obj->state->ps_update = 1;
	memset(rds_obj->data->ps, ' ', PS_LENGTH);
	while (*ps != 0 && len <= PS_LENGTH)
		rds_obj->data->ps[len++] = *ps++;
}

#ifdef RDS2
void set_rds_lps(struct rds_obj_t *rds_obj, char *lps) {
	uint8_t i = 0, len = 0;

	rds_obj->state->lps_update = 1;
	memset(rds_obj->data->lps, '\r', LPS_LENGTH);
	while (*lps != 0 && len <= LPS_LENGTH)
		rds_obj->data->lps[len++] = *lps++;

	if (len < LPS_LENGTH) {
		rds_obj->state->lps_segments = 0;

		/* find out how many segments are needed */
		while (i < len) {
			i += 4;
			rds_obj->state->lps_segments++;
		}
	} else {
		/* default to 8 if LPS is 32 characters long */
		rds_obj->state->lps_segments = 8;
	}
}
#endif

void set_rds_rtplus_flags(struct rds_obj_t *rds_obj, uint8_t *flags) {
	rds_obj->rtplus_cfg->running	= flags[0] & 1;
	rds_obj->rtplus_cfg->toggle	= flags[1] & 1;
}

void set_rds_rtplus_tags(struct rds_obj_t *rds_obj, uint8_t *tags) {
	rds_obj->rtplus_cfg->type[0]	= tags[0] & INT8_L6;
	rds_obj->rtplus_cfg->start[0]	= tags[1] & INT8_L6;
	rds_obj->rtplus_cfg->len[0]	= tags[2] & INT8_L6;
	rds_obj->rtplus_cfg->type[1]	= tags[3] & INT8_L6;
	rds_obj->rtplus_cfg->start[1]	= tags[4] & INT8_L6;
	rds_obj->rtplus_cfg->len[1]	= tags[5] & INT8_L5;
}

#ifdef RDS2
/* eRT+ */
void set_rds_ertplus_flags(struct rds_obj_t *rds_obj, uint8_t *flags) {
	rds_obj->ertplus_cfg->running	= flags[0] & 1;
	rds_obj->ertplus_cfg->toggle	= flags[1] & 1;
}

void set_rds_ertplus_tags(struct rds_obj_t *rds_obj, uint8_t *tags) {
	rds_obj->ertplus_cfg->type[0]	= tags[0] & INT8_L6;
	rds_obj->ertplus_cfg->start[0]	= tags[1] & INT8_L6;
	rds_obj->ertplus_cfg->len[0]	= tags[2] & INT8_L6;
	rds_obj->ertplus_cfg->type[1]	= tags[3] & INT8_L6;
	rds_obj->ertplus_cfg->start[1]	= tags[4] & INT8_L6;
	rds_obj->ertplus_cfg->len[1]	= tags[5] & INT8_L5;
}
#endif

void set_rds_af(struct rds_obj_t *rds_obj, struct rds_af_t new_af_list) {
	memcpy(&rds_obj->data->af, &new_af_list, sizeof(struct rds_af_t));
}

void clear_rds_af(struct rds_obj_t *rds_obj) {
	memset(&rds_obj->data->af, 0, sizeof(struct rds_af_t));
}

void set_rds_pty(struct rds_obj_t *rds_obj, uint8_t pty) {
	rds_obj->data->pty = pty & 31;
}

void set_rds_ptyn(struct rds_obj_t *rds_obj, char *ptyn) {
	uint8_t len = 0;

	if (!ptyn[0]) {
		memset(rds_obj->data->ptyn, 0, PTYN_LENGTH);
		return;
	}

	rds_obj->state->ptyn_update = 1;
	memset(rds_obj->data->ptyn, ' ', PTYN_LENGTH);
	while (*ptyn != 0 && len <= PTYN_LENGTH)
		rds_obj->data->ptyn[len++] = *ptyn++;
}

void set_rds_ta(struct rds_obj_t *rds_obj, uint8_t ta) {
	rds_obj->data->ta = ta & 1;
}

void set_rds_tp(struct rds_obj_t *rds_obj, uint8_t tp) {
	rds_obj->data->tp = tp & 1;
}

void set_rds_ms(struct rds_obj_t *rds_obj, uint8_t ms) {
	rds_obj->data->ms = ms & 1;
}

void set_rds_di(struct rds_obj_t *rds_obj, uint8_t di) {
	rds_obj->data->di = di & 15;
}

void set_rds_ct(struct rds_obj_t *rds_obj, uint8_t ct) {
	rds_obj->data->tx_ctime = ct & 1;
}
