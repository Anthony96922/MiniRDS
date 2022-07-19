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

static void register_oda(struct rds_obj_t *rds_obj, uint8_t group, uint16_t aid, uint16_t scb) {
	struct rds_oda_t *odas;
	struct rds_oda_state_t *oda_state = rds_obj->oda_state;

	if (oda_state->count == MAX_ODAS) return; // can't accept more ODAs

	odas = &rds_obj->odas[oda_state->count];
	odas->group = group;
	odas->aid = aid;
	odas->scb = scb;
	oda_state->count++;
}

/* Get the next AF entry
 */
static uint16_t get_next_af(struct rds_obj_t *rds_obj) {
	struct rds_af_t *af = rds_obj->af_list;
	static uint8_t af_state;
	uint16_t out;

	if (af->num_afs) {
		if (af_state == 0) {
			out = (AF_CODE_NUM_AFS_BASE + af->num_afs) << 8;
			out |= af->afs[0];
			af_state += 1;
		} else {
			out = af->afs[af_state] << 8;
			if (af->afs[af_state+1])
				out |= af->afs[af_state+1];
			else
				out |= AF_CODE_FILLER;
			af_state += 2;
		}
		if (af_state >= af->num_entries) af_state = 0;
	} else {
		out = AF_CODE_NO_AF << 8 | AF_CODE_FILLER;
	}

	return out;
}

/* PS group (0A)
 */
static void get_rds_ps_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;
	uint8_t ps_state = rds_obj->state->ps_state;

	if (ps_state == 0 && rds_obj->state->ps_update) {
		memcpy(rds_obj->data->cur_ps, rds_obj->data->ps, PS_LENGTH);
		rds_obj->state->ps_update = 0; // rewind
	}

	// TA
	blocks[1] |= (rds_obj->data->ta & 1) << 4;

	// MS
	blocks[1] |= (rds_obj->data->ms & 1) << 3;

	// DI
	blocks[1] |= ((rds_obj->data->di >> (3 - ps_state)) & 1) << 2;

	// PS segment address
	blocks[1] |= (ps_state & INT8_L2);

	// AF
	blocks[2] = get_next_af(rds_obj);

	// PS
	blocks[3] = rds_obj->data->cur_ps[ps_state*2] << 8;
	blocks[3] |= rds_obj->data->cur_ps[ps_state*2+1];

	ps_state++;
	if (ps_state == 4) ps_state = 0;

	/* update the index */
	rds_obj->state->ps_state = ps_state;
}

/* RT group (2A)
 */
static void get_rds_rt_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;
	uint8_t rt_state = rds_obj->state->rt_state;

	if (rds_obj->state->rt_bursting) rds_obj->state->rt_bursting--;

	if (rds_obj->state->rt_update) {
		memcpy(rds_obj->data->cur_rt, rds_obj->data->rt, RT_LENGTH);
		rds_obj->data->rt_ab ^= 1;
		rds_obj->state->rt_update = 0;
		rt_state = 0; // rewind when new RT arrives
	}

	blocks[1] |= 2 << 12;
	blocks[1] |= (rds_obj->data->rt_ab & 1) << 4;
	blocks[1] |= rt_state & INT8_L4;
	blocks[2] =  rds_obj->data->cur_rt[rt_state*4+0] << 8;
	blocks[2] |= rds_obj->data->cur_rt[rt_state*4+1];
	blocks[3] =  rds_obj->data->cur_rt[rt_state*4+2] << 8;
	blocks[3] |= rds_obj->data->cur_rt[rt_state*4+3];

	rt_state++;
	if (rt_state == rds_obj->state->rt_segments)
		rt_state = 0;

	/* update the index */
	rds_obj->state->rt_state = rt_state;
}

/* ODA group (3A)
 */
static void get_rds_oda_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;

	// select ODA
	uint8_t current_oda = rds_obj->oda_state->current;
	struct rds_oda_t *this_oda = &rds_obj->odas[current_oda];

	blocks[1] |= 3 << 12;
	blocks[1] |= GET_GROUP_TYPE(this_oda->group) << 1;
	blocks[1] |= GET_GROUP_VER(this_oda->group);
	blocks[2] = this_oda->scb;
	blocks[3] = this_oda->aid;

	current_oda++;
	if (current_oda == rds_obj->oda_state->count) current_oda = 0;

	/* update the index */
	rds_obj->oda_state->current = current_oda;
}

/* Generates a CT (clock time) group if the minute has just changed
 * Returns 1 if the CT group was generated, 0 otherwise
 */
static uint8_t get_rds_ct_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;

	static uint8_t latest_minutes;
	struct tm utc, local;

	// Check time
	time_t now = time(NULL);
	memcpy(&utc, gmtime(&now), sizeof(struct tm));

	if (utc.tm_min != latest_minutes) {
		// Generate CT group
		latest_minutes = utc.tm_min;

		uint8_t l = utc.tm_mon <= 1 ? 1 : 0;
		uint16_t mjd = 14956 + utc.tm_mday +
			(uint16_t)((utc.tm_year - l) * 365.25f) +
			(uint16_t)((utc.tm_mon + 2 + l*12) * 30.6001f);

		blocks[1] |= 4 << 12 | (mjd>>15);
		blocks[2] = (mjd<<1) | (utc.tm_hour>>4);
		blocks[3] = (utc.tm_hour & 0xF)<<12 | utc.tm_min<<6;

		memcpy(&local, localtime(&now), sizeof(struct tm));

		int8_t offset = local.tm_hour - utc.tm_hour;
		int8_t min_offset = local.tm_min - utc.tm_min;
		int8_t half_offset;

		/* half hour offset */
		if (min_offset < 0) {
			half_offset = -1;
		} else if (min_offset > 0) {
			half_offset = 1;
		} else {
			half_offset = 0;
		}

		/* if local and UTC are on different days */
		if (utc.tm_hour <= 12)
			offset -= 24;

		/* determine negative offset */
		uint8_t negative_offset = (offset + half_offset) < 0 ? 1 : 0;

		blocks[3] |= (negative_offset & 1) << 5;
		blocks[3] |= abs(2 * offset + half_offset) & INT8_L5;

		return 1;
	}

	return 0;
}

/* PTYN group (10A)
 */
static void get_rds_ptyn_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;
	uint8_t ptyn_state = rds_obj->state->ptyn_state;

	if (ptyn_state == 0 && rds_obj->state->ptyn_update) {
		memcpy(rds_obj->data->cur_ptyn, rds_obj->data->ptyn, PTYN_LENGTH);
		rds_obj->state->ptyn_update = 0;
	}

	blocks[1] |= 10 << 12 | (ptyn_state & INT8_L2);
	blocks[2] =  rds_obj->data->cur_ptyn[ptyn_state*4+0] << 8;
	blocks[2] |= rds_obj->data->cur_ptyn[ptyn_state*4+1];
	blocks[3] = rds_obj->data->cur_ptyn[ptyn_state*4+2] << 8;
	blocks[3] |= rds_obj->data->cur_ptyn[ptyn_state*4+3];

	ptyn_state++;
	if (ptyn_state == 2) ptyn_state = 0;

	rds_obj->state->ptyn_state = ptyn_state;
}

#ifdef RDS2
/* Long PS group (15A) */
static void get_rds_lps_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;
	uint8_t lps_state = rds_obj->state->lps_state;

	if (lps_state == 0 && rds_obj->state->lps_update) {
		memcpy(rds_obj->data->cur_lps, rds_obj->data->lps, LPS_LENGTH);
		rds_obj->state->lps_update = 0;
	}

	blocks[1] |= 15 << 12 | (lps_state & INT8_L3);
	blocks[2] =  rds_obj->data->cur_lps[lps_state*4+0] << 8;
	blocks[2] |= rds_obj->data->cur_lps[lps_state*4+1];
	blocks[3] =  rds_obj->data->cur_lps[lps_state*4+2] << 8;
	blocks[3] |= rds_obj->data->cur_lps[lps_state*4+3];

	lps_state++;
	if (lps_state == rds_obj->state->lps_segments)
		lps_state = 0;

	/* update the index */
	rds_obj->state->lps_state = lps_state;
}
#endif

// RT+
static void init_rtplus(struct rds_obj_t *rds_obj, uint8_t group) {
	register_oda(rds_obj, group, 0x4BD7 /* RT+ AID */, 0);
	rds_obj->rtplus_cfg->group = group;
}

#ifdef RDS2
/* eRT */
static void init_ert(struct rds_obj_t *rds_obj, uint8_t group) {
	if (GET_GROUP_VER(group) == 1) {
		/* B version groups cannot be used for eRT */
		return;
	}
	register_oda(rds_obj, group, 0x6552 /* eRT AID */, 0);
	rds_obj->ert_cfg->group = group;
}

/* eRT+ */
static void init_ertp(struct rds_obj_t *rds_obj, uint8_t group) {
	register_oda(rds_obj, group, 0x4BD8 /* eRT+ AID */, 0);
	rds_obj->ertplus_cfg->group = group;
}
#endif

/* RT+ group
 */
static void get_rds_rtplus_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;

	// RT+ block format
	blocks[1] |= GET_GROUP_TYPE(rds_obj->rtplus_cfg->group) << 12;
	blocks[1] |= GET_GROUP_VER(rds_obj->rtplus_cfg->group) << 11;
	blocks[1] |= rds_obj->rtplus_cfg->toggle << 4;
	blocks[1] |= rds_obj->rtplus_cfg->running << 3;
	blocks[1] |= (rds_obj->rtplus_cfg->type[0] & INT8_U5) >> 3;

	blocks[2] = (rds_obj->rtplus_cfg->type[0] & INT8_L3) << 13;
	blocks[2] |= (rds_obj->rtplus_cfg->start[0] & INT8_L6) << 7;
	blocks[2] |= (rds_obj->rtplus_cfg->len[0] & INT8_L6) << 1;
	blocks[2] |= (rds_obj->rtplus_cfg->type[1] & INT8_U3) >> 5;

	blocks[3] = (rds_obj->rtplus_cfg->type[1] & INT8_L5) << 11;
	blocks[3] |= (rds_obj->rtplus_cfg->start[1] & INT8_L6) << 5;
	blocks[3] |= rds_obj->rtplus_cfg->len[1] & INT8_L5;
}

#ifdef RDS2
/* eRT group */
static void get_rds_ert_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;
	uint8_t ert_state = rds_obj->state->ert_state;

	if (rds_obj->state->ert_bursting) rds_obj->state->ert_bursting--;

	if (rds_obj->state->ert_update) {
		memcpy(rds_obj->data->cur_ert, rds_obj->data->ert, ERT_LENGTH);
		rds_obj->state->ert_update = 0;
		ert_state = 0; /* rewind when new eRT arrives */
	}

	/* eRT block format */
	blocks[1] |= GET_GROUP_TYPE(rds_obj->ert_cfg->group) << 12;
	blocks[1] |= ert_state & INT8_L5;
	blocks[2] =  rds_obj->data->cur_ert[ert_state*4+0] << 8;
	blocks[2] |= rds_obj->data->cur_ert[ert_state*4+1];
	blocks[3] =  rds_obj->data->cur_ert[ert_state*4+2] << 8;
	blocks[3] |= rds_obj->data->cur_ert[ert_state*4+3];

	ert_state++;
	if (ert_state == rds_obj->state->ert_segments) ert_state = 0;

	/* update the index */
	rds_obj->state->ert_state = ert_state;
}

/* eRT+ group */
static void get_rds_ertplus_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;

	// RT+ block format
	blocks[1] |= GET_GROUP_TYPE(rds_obj->ertplus_cfg->group) << 12;
	blocks[1] |= GET_GROUP_VER(rds_obj->ertplus_cfg->group) << 11;
	blocks[1] |= rds_obj->ertplus_cfg->toggle << 4;
	blocks[1] |= rds_obj->ertplus_cfg->running << 3;
	blocks[1] |= (rds_obj->ertplus_cfg->type[0] & INT8_U5) >> 3;

	blocks[2] = (rds_obj->ertplus_cfg->type[0] & INT8_L3) << 13;
	blocks[2] |= (rds_obj->ertplus_cfg->start[0] & INT8_L6) << 7;
	blocks[2] |= (rds_obj->ertplus_cfg->len[0] & INT8_L6) << 1;
	blocks[2] |= (rds_obj->ertplus_cfg->type[1] & INT8_U3) >> 5;

	blocks[3] = (rds_obj->ertplus_cfg->type[1] & INT8_L5) << 11;
	blocks[3] |= (rds_obj->ertplus_cfg->start[1] & INT8_L6) << 5;
	blocks[3] |= rds_obj->ertplus_cfg->len[1] & INT8_L5;
}
#endif

/* Lower priority groups are placed in a subsequence
 */
static uint8_t get_rds_other_groups(struct rds_obj_t *rds_obj) {
	static uint8_t group[GROUP_15B];

	// Type 3A groups
	if (++group[GROUP_3A] >= 20) {
		group[GROUP_3A] = 0;
		get_rds_oda_group(rds_obj);
		return 1;
	}

	// Type 10A groups
	if (rds_obj->data->ptyn[0]) {
		if (++group[GROUP_10A] >= 10) {
			group[GROUP_10A] = 0;
			// Do not generate a 10A group if PTYN is off
			get_rds_ptyn_group(rds_obj);
			return 1;
		}
	}

	// Type 11A groups
	if (++group[rds_obj->rtplus_cfg->group] >= 30) {
		group[rds_obj->rtplus_cfg->group] = 0;
		get_rds_rtplus_group(rds_obj);
		return 1;
	}

#ifdef RDS2
	/* Type 12A groups */
	if (rds_obj->data->ert[0]) {
		if (++group[rds_obj->ert_cfg->group] >= 3) {
			group[rds_obj->ert_cfg->group] = 0;
			get_rds_ert_group(rds_obj);
			return 1;
		}
	}

	/* Type 13A groups */
	if (++group[rds_obj->ertplus_cfg->group] >= 30) {
		group[rds_obj->ertplus_cfg->group] = 0;
		get_rds_ertplus_group(rds_obj);
		return 1;
	}

	/* Type 15A groups */
	if (rds_obj->data->lps[0]) {
		if (++group[GROUP_15A] >= 15) {
			group[GROUP_15A] = 0;
			get_rds_lps_group(rds_obj);
			return 1;
		}
	}
#endif

	return 0;
}

/* Creates an RDS group.
 * This generates sequences of the form 0A, 2A, 0A, 2A, 0A, 2A, etc.
 */
static void get_rds_group(struct rds_obj_t *rds_obj) {
	uint16_t *blocks = rds_obj->blocks;

	// Basic block data
	blocks[0] = rds_obj->data->pi;
	blocks[1] = (rds_obj->data->tp & 1) << 10 | (rds_obj->data->pty & INT8_L5) << 5;
	blocks[2] = 0;
	blocks[3] = 0;

	// Generate block content
	// CT (clock time) has priority on other group types
	if (!(rds_obj->data->tx_ctime && get_rds_ct_group(rds_obj))) {
		if (!get_rds_other_groups(rds_obj)) { // Other groups
			// These are always transmitted
			if (!rds_obj->state->gs_count) { // Type 0A groups
				get_rds_ps_group(rds_obj);
				rds_obj->state->gs_count++;
			} else { // Type 2A groups
				get_rds_rt_group(rds_obj);
				if (!rds_obj->state->rt_bursting) rds_obj->state->gs_count++;
			}
			if (rds_obj->state->gs_count == 2) rds_obj->state->gs_count = 0;
		}
	}

	/* for version B groups */
	if ((blocks[1] >> 11) & 1) {
		blocks[2] = rds_obj->data->pi;
	}
}

void get_rds_bits(struct rds_obj_t *rds_obj) {
	memset(rds_obj->blocks, 0, 1);
	get_rds_group(rds_obj);
	add_checkwords(rds_obj->blocks, rds_obj->bits);
}

void init_rds_encoder(struct rds_obj_t *rds_obj, struct rds_params_t rds_params) {

	init_rds_objects(rds_obj);

	if (rds_params.call_sign[3]) {
#ifdef RBDS
		uint16_t new_pi = callsign2pi(rds_params.call_sign);
		if (new_pi) rds_params.pi = new_pi;
#endif
	}

	// AF
	if (rds_params.af.num_afs) {
		set_rds_af(rds_obj, rds_params.af);
	}

	set_rds_pi(rds_obj, rds_params.pi);
	set_rds_ps(rds_obj, rds_params.ps);
	rds_obj->data->rt_ab = 1;
	set_rds_rt(rds_obj, rds_params.rt);
	set_rds_pty(rds_obj, rds_params.pty);
	if (rds_params.ptyn[0])
		set_rds_ptyn(rds_obj, rds_params.ptyn);
	set_rds_tp(rds_obj, rds_params.tp);
	set_rds_ct(rds_obj, 1);
	set_rds_ms(rds_obj, 1);
	set_rds_di(rds_obj, DI_STEREO);

	// Assign the RT+ AID to group 11A
	init_rtplus(rds_obj, GROUP_11A);

#ifdef RDS2
	/* Assign the eRT AID to group 12A */
	init_ert(rds_obj, GROUP_12A);

	/* Assign the eRT+ AID to group 13A */
	init_ertp(rds_obj, GROUP_13A);
#endif

	/* initialize modulator objects */
	init_rds_generator(rds_obj);

#ifdef RDS2
	init_rds2_encoder(rds_obj->rds2_obj);
#endif
}

void exit_rds_encoder(struct rds_obj_t *rds_obj) {
	exit_rds_objects(rds_obj);
	exit_rds_generator(rds_obj);
#ifdef RDS2
	exit_rds2_encoder(rds_obj->rds2_obj);
#endif
}
