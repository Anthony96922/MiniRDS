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
#include "modulator.h"
#ifdef RDS2
#include "rds2.h"
#endif
#include "lib.h"

static struct rds_params_t rds_data;

// RDS data controls
static struct {
	uint8_t ps_update;
	uint8_t rt_update;
	uint8_t ab;
	uint8_t rt_segments;
	uint8_t rt_bursting;
	uint8_t ptyn_update;

#ifdef RDS2
	/* Long PS */
	uint8_t lps_update;
	uint8_t lps_segments;
#endif

	/* eRT */
	uint8_t ert_update;
	uint8_t ert_segments;
	uint8_t ert_bursting;
} rds_state;

// ODA
static struct rds_oda_t odas[MAX_ODAS];
static struct {
	uint8_t current;
	uint8_t count;
} oda_state;

// RT+
static struct {
	uint8_t group;
	uint8_t running;
	uint8_t toggle;
	uint8_t type[2];
	uint8_t start[2];
	uint8_t len[2];
} rtplus_cfg;

#ifdef RDS2
/* eRT */
static struct {
	uint8_t group;
} ert_cfg;

/* eRT+ */
static struct {
	uint8_t group;
	uint8_t running;
	uint8_t toggle;
	uint8_t type[2];
	uint8_t start[2];
	uint8_t len[2];
} ertplus_cfg;
#endif

static void register_oda(uint8_t group, uint16_t aid, uint16_t scb) {

	if (oda_state.count == MAX_ODAS) return; // can't accept more ODAs

	odas[oda_state.count].group = group;
	odas[oda_state.count].aid = aid;
	odas[oda_state.count].scb = scb;
	oda_state.count++;
}

/* Get the next AF entry
 */
static uint16_t get_next_af() {
	static uint8_t af_state;
	uint16_t out;

	if (rds_data.af.num_afs) {
		if (af_state == 0) {
			out = (AF_CODE_NUM_AFS_BASE + rds_data.af.num_afs) << 8;
			out |= rds_data.af.afs[0];
			af_state += 1;
		} else {
			out = rds_data.af.afs[af_state] << 8;
			if (rds_data.af.afs[af_state+1])
				out |= rds_data.af.afs[af_state+1];
			else
				out |= AF_CODE_FILLER;
			af_state += 2;
		}
		if (af_state >= rds_data.af.num_entries) af_state = 0;
	} else {
		out = AF_CODE_NO_AF << 8 | AF_CODE_FILLER;
	}

	return out;
}

/* PS group (0A)
 */
static void get_rds_ps_group(uint16_t *blocks) {
	static char ps_text[PS_LENGTH];
	static uint8_t ps_state;

	if (ps_state == 0 && rds_state.ps_update) {
		strncpy(ps_text, rds_data.ps, PS_LENGTH);
		rds_state.ps_update = 0; // rewind
	}

	// TA
	blocks[1] |= (rds_data.ta & 1) << 4;

	// MS
	blocks[1] |= (rds_data.ms & 1) << 3;

	// DI
	blocks[1] |= ((rds_data.di >> (3 - ps_state)) & 1) << 2;

	// PS segment address
	blocks[1] |= (ps_state & INT8_L2);

	// AF
	blocks[2] = get_next_af();

	// PS
	blocks[3] = ps_text[ps_state*2] << 8 | ps_text[ps_state*2+1];

	ps_state++;
	if (ps_state == 4) ps_state = 0;
}

/* RT group (2A)
 */
static void get_rds_rt_group(uint16_t *blocks) {
	static char rt_text[RT_LENGTH];
	static uint8_t rt_state;

	if (rds_state.rt_bursting) rds_state.rt_bursting--;

	if (rds_state.rt_update) {
		strncpy(rt_text, rds_data.rt, RT_LENGTH);
		rds_state.ab ^= 1;
		rds_state.rt_update = 0;
		rt_state = 0; // rewind when new RT arrives
	}

	blocks[1] |= 2 << 12;
	blocks[1] |= (rds_state.ab & 1) << 4;
	blocks[1] |= rt_state & INT8_L4;
	blocks[2] = rt_text[rt_state*4+0] << 8 | rt_text[rt_state*4+1];
	blocks[3] = rt_text[rt_state*4+2] << 8 | rt_text[rt_state*4+3];

	rt_state++;
	if (rt_state == rds_state.rt_segments) rt_state = 0;
}

/* ODA group (3A)
 */
static void get_rds_oda_group(uint16_t *blocks) {
	blocks[1] |= 3 << 12;

	// select ODA
	struct rds_oda_t this_oda = odas[oda_state.current];

	blocks[1] |= GET_GROUP_TYPE(this_oda.group) << 1;
	blocks[1] |= GET_GROUP_VER(this_oda.group);
	blocks[2] = this_oda.scb;
	blocks[3] = this_oda.aid;

	oda_state.current++;
	if (oda_state.current == oda_state.count) oda_state.current = 0;
}

/* Generates a CT (clock time) group if the minute has just changed
 * Returns 1 if the CT group was generated, 0 otherwise
 */
static uint8_t get_rds_ct_group(uint16_t *blocks) {
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
static void get_rds_ptyn_group(uint16_t *blocks) {
	static char ptyn_text[PTYN_LENGTH];
	static uint8_t ptyn_state;

	if (ptyn_state == 0 && rds_state.ptyn_update) {
		strncpy(ptyn_text, rds_data.ptyn, PTYN_LENGTH);
		rds_state.ptyn_update = 0;
	}

	blocks[1] |= 10 << 12 | (ptyn_state & INT8_L2);
	blocks[2] = ptyn_text[ptyn_state*4+0] << 8 | ptyn_text[ptyn_state*4+1];
	blocks[3] = ptyn_text[ptyn_state*4+2] << 8 | ptyn_text[ptyn_state*4+3];

	ptyn_state++;
	if (ptyn_state == 2) ptyn_state = 0;
}

#ifdef RDS2
/* Long PS group (15A) */
static void get_rds_lps_group(uint16_t *blocks) {
	static char lps_text[LPS_LENGTH];
	static uint8_t lps_state;

	if (lps_state == 0 && rds_state.lps_update) {
		strncpy(lps_text, rds_data.lps, LPS_LENGTH);
		rds_state.lps_update = 0;
	}

	blocks[1] |= 15 << 12 | (lps_state & INT8_L3);
	blocks[2] = lps_text[lps_state*4+0] << 8 | lps_text[lps_state*4+1];
	blocks[3] = lps_text[lps_state*4+2] << 8 | lps_text[lps_state*4+3];

	lps_state++;
	if (lps_state == rds_state.lps_segments) lps_state = 0;
}
#endif

// RT+
static void init_rtplus(uint8_t group) {
	register_oda(group, 0x4BD7 /* RT+ AID */, 0);
	rtplus_cfg.group = group;
}

#ifdef RDS2
/* eRT */
static void init_ert(uint8_t group) {
	if (GET_GROUP_VER(group) == 1) {
		/* B version groups cannot be used for eRT */
		return;
	}
	register_oda(group, 0x6552 /* eRT AID */, 0);
	ert_cfg.group = group;
}

/* eRT+ */
static void init_ertp(uint8_t group) {
	register_oda(group, 0x4BD8 /* eRT+ AID */, 0);
	ertplus_cfg.group = group;
}
#endif

/* RT+ group
 */
static void get_rds_rtplus_group(uint16_t *blocks) {
	// RT+ block format
	blocks[1] |= GET_GROUP_TYPE(rtplus_cfg.group) << 12;
	blocks[1] |= GET_GROUP_VER(rtplus_cfg.group) << 11;
	blocks[1] |= rtplus_cfg.toggle << 4 | rtplus_cfg.running << 3;
	blocks[1] |= (rtplus_cfg.type[0] & INT8_U5) >> 3;

	blocks[2] = (rtplus_cfg.type[0] & INT8_L3) << 13;
	blocks[2] |= (rtplus_cfg.start[0] & INT8_L6) << 7;
	blocks[2] |= (rtplus_cfg.len[0] & INT8_L6) << 1;
	blocks[2] |= (rtplus_cfg.type[1] & INT8_U3) >> 5;

	blocks[3] = (rtplus_cfg.type[1] & INT8_L5) << 11;
	blocks[3] |= (rtplus_cfg.start[1] & INT8_L6) << 5;
	blocks[3] |= rtplus_cfg.len[1] & INT8_L5;
}

#ifdef RDS2
/* eRT group */
static void get_rds_ert_group(uint16_t *blocks) {
	static char ert_text[ERT_LENGTH];
	static uint8_t ert_state;

	if (rds_state.ert_bursting) rds_state.ert_bursting--;

	if (rds_state.ert_update) {
		strncpy(ert_text, rds_data.ert, ERT_LENGTH);
		rds_state.ert_update = 0;
		ert_state = 0; /* rewind when new eRT arrives */
	}

	/* eRT block format */
	blocks[1] |= GET_GROUP_TYPE(ert_cfg.group) << 12;
	blocks[1] |= ert_state & INT8_L5;
	blocks[2] = ert_text[ert_state*4+0] << 8 | ert_text[ert_state*4+1];
	blocks[3] = ert_text[ert_state*4+2] << 8 | ert_text[ert_state*4+3];

	ert_state++;
	if (ert_state == rds_state.ert_segments) ert_state = 0;
}

/* eRT+ group */
static void get_rds_ertplus_group(uint16_t *blocks) {
	// RT+ block format
	blocks[1] |= GET_GROUP_TYPE(ertplus_cfg.group) << 12;
	blocks[1] |= GET_GROUP_VER(ertplus_cfg.group) << 11;
	blocks[1] |= ertplus_cfg.toggle << 4 | ertplus_cfg.running << 3;
	blocks[1] |= (ertplus_cfg.type[0] & INT8_U5) >> 3;

	blocks[2] = (ertplus_cfg.type[0] & INT8_L3) << 13;
	blocks[2] |= (ertplus_cfg.start[0] & INT8_L6) << 7;
	blocks[2] |= (ertplus_cfg.len[0] & INT8_L6) << 1;
	blocks[2] |= (ertplus_cfg.type[1] & INT8_U3) >> 5;

	blocks[3] = (ertplus_cfg.type[1] & INT8_L5) << 11;
	blocks[3] |= (ertplus_cfg.start[1] & INT8_L6) << 5;
	blocks[3] |= ertplus_cfg.len[1] & INT8_L5;
}
#endif

/* Lower priority groups are placed in a subsequence
 */
static uint8_t get_rds_other_groups(uint16_t *blocks) {
	static uint8_t group[GROUP_15B];

	// Type 3A groups
	if (++group[GROUP_3A] >= 20) {
		group[GROUP_3A] = 0;
		get_rds_oda_group(blocks);
		return 1;
	}

	// Type 10A groups
	if (rds_data.ptyn[0]) {
		if (++group[GROUP_10A] >= 10) {
			group[GROUP_10A] = 0;
			// Do not generate a 10A group if PTYN is off
			get_rds_ptyn_group(blocks);
			return 1;
		}
	}

	// Type 11A groups
	if (++group[rtplus_cfg.group] >= 30) {
		group[rtplus_cfg.group] = 0;
		get_rds_rtplus_group(blocks);
		return 1;
	}

#ifdef RDS2
	/* Type 12A groups */
	if (rds_data.ert[0]) {
		if (++group[ert_cfg.group] >= 3) {
			group[ert_cfg.group] = 0;
			get_rds_ert_group(blocks);
			return 1;
		}
	}

	/* Type 13A groups */
	if (++group[ertplus_cfg.group] >= 30) {
		group[ertplus_cfg.group] = 0;
		get_rds_ertplus_group(blocks);
		return 1;
	}

	/* Type 15A groups */
	if (rds_data.lps[0]) {
		if (++group[GROUP_15A] >= 15) {
			group[GROUP_15A] = 0;
			get_rds_lps_group(blocks);
			return 1;
		}
	}
#endif

	return 0;
}

/* Creates an RDS group.
 * This generates sequences of the form 0A, 2A, 0A, 2A, 0A, 2A, etc.
 */
static void get_rds_group(uint16_t *blocks) {
	static uint8_t state;

	// Basic block data
	blocks[0] = rds_data.pi;
	blocks[1] = (rds_data.tp & 1) << 10 | (rds_data.pty & INT8_L5) << 5;
	blocks[2] = 0;
	blocks[3] = 0;

	// Generate block content
	// CT (clock time) has priority on other group types
	if (!(rds_data.tx_ctime && get_rds_ct_group(blocks))) {
		if (!get_rds_other_groups(blocks)) { // Other groups
			// These are always transmitted
			if (!state) { // Type 0A groups
				get_rds_ps_group(blocks);
				state++;
			} else { // Type 2A groups
				get_rds_rt_group(blocks);
				if (!rds_state.rt_bursting) state++;
			}
			if (state == 2) state = 0;
		}
	}

	/* for version B groups */
	if ((blocks[1] >> 11) & 1) {
		blocks[2] = rds_data.pi;
	}
}

void get_rds_bits(uint8_t *bits) {
	static uint16_t out_blocks[GROUP_LENGTH];
	get_rds_group(out_blocks);
	add_checkwords(out_blocks, bits);
}

void init_rds_encoder(struct rds_params_t rds_params, char *call_sign) {

	if (call_sign[3]) {
#ifdef RBDS
		uint16_t new_pi = callsign2pi(call_sign);
		if (new_pi) rds_params.pi = new_pi;
#endif
	}

	// AF
	if (rds_params.af.num_afs) {
		set_rds_af(rds_params.af);
		show_af_list(rds_params.af);
	}

	set_rds_pi(rds_params.pi);
	set_rds_ps(rds_params.ps);
	rds_state.ab = 1;
	set_rds_rt(rds_params.rt);
	set_rds_pty(rds_params.pty);
	if (rds_params.ptyn[0])
		set_rds_ptyn(rds_params.ptyn);
	set_rds_tp(rds_params.tp);
	set_rds_ct(1);
	set_rds_ms(1);
	set_rds_di(DI_STEREO);

	// Assign the RT+ AID to group 11A
	init_rtplus(GROUP_11A);

#ifdef RDS2
	/* Assign the eRT AID to group 12A */
	init_ert(GROUP_12A);

	/* Assign the eRT+ AID to group 13A */
	init_ertp(GROUP_13A);
#endif

	/* initialize modulator objects */
	init_rds_objects();

#ifdef RDS2
	init_rds2_encoder();
#endif
}

void exit_rds_encoder() {
	exit_rds_objects();
#ifdef RDS2
	exit_rds2_encoder();
#endif
}

void set_rds_pi(uint16_t pi_code) {
	rds_data.pi = pi_code;
}

void set_rds_rt(char *rt) {
	uint8_t i = 0, len = 0;

	rds_state.rt_update = 1;
	memset(rds_data.rt, ' ', RT_LENGTH);
	while (*rt != 0 && len <= RT_LENGTH)
		rds_data.rt[len++] = *rt++;

	if (len < RT_LENGTH) {
		rds_state.rt_segments = 0;

		/* Terminate RT with '\r' (carriage return) if RT
		 * is < 64 characters long
		 */
		rds_data.rt[len++] = '\r';

		/* find out how many segments are needed */
		while (i < len) {
			i += 4;
			rds_state.rt_segments++;
		}
	} else {
		// Default to 16 if RT is 64 characters long
		rds_state.rt_segments = 16;
	}

	rds_state.rt_bursting = rds_state.rt_segments;
}

#ifdef RDS2
void set_rds_ert(char *ert) {
	uint8_t i = 0, len = 0;

	if (!ert[0]) {
		memset(rds_data.ert, 0, ERT_LENGTH);
		return;
	}

	rds_state.ert_update = 1;
	memset(rds_data.ert, '\r', ERT_LENGTH);
	while (*ert != 0 && len <= ERT_LENGTH)
		rds_data.ert[len++] = *ert++;

	if (len < ERT_LENGTH) {
		rds_state.ert_segments = 0;

		/* increment to allow adding an '\r' in all cases */
		len++;

		/* find out how many segments are needed */
		while (i < len) {
			i += 4;
			rds_state.ert_segments++;
		}
	} else {
		/* Default to 32 if eRT is 128 characters long */
		rds_state.ert_segments = 32;
	}

	rds_state.ert_bursting = rds_state.ert_segments;
}
#endif

void set_rds_ps(char *ps) {
	uint8_t len = 0;

	rds_state.ps_update = 1;
	memset(rds_data.ps, ' ', PS_LENGTH);
	while (*ps != 0 && len <= PS_LENGTH)
		rds_data.ps[len++] = *ps++;
}

#ifdef RDS2
void set_rds_lps(char *lps) {
	uint8_t i = 0, len = 0;

	rds_state.lps_update = 1;
	memset(rds_data.lps, '\r', LPS_LENGTH);
	while (*lps != 0 && len <= LPS_LENGTH)
		rds_data.lps[len++] = *lps++;

	if (len < LPS_LENGTH) {
		rds_state.lps_segments = 0;

		/* find out how many segments are needed */
		while (i < len) {
			i += 4;
			rds_state.lps_segments++;
		}
	} else {
		/* default to 8 if LPS is 32 characters long */
		rds_state.lps_segments = 8;
	}
}
#endif

void set_rds_rtplus_flags(uint8_t running, uint8_t toggle) {
	rtplus_cfg.running	= running & 1;
	rtplus_cfg.toggle	= toggle & 1;
}

void set_rds_rtplus_tags(uint8_t *tags) {
	rtplus_cfg.type[0]	= tags[0] & INT8_L6;
	rtplus_cfg.start[0]	= tags[1] & INT8_L6;
	rtplus_cfg.len[0]	= tags[2] & INT8_L6;
	rtplus_cfg.type[1]	= tags[3] & INT8_L6;
	rtplus_cfg.start[1]	= tags[4] & INT8_L6;
	rtplus_cfg.len[1]	= tags[5] & INT8_L5;
}

#ifdef RDS2
/* eRT+ */
void set_rds_ertplus_flags(uint8_t running, uint8_t toggle) {
	ertplus_cfg.running	= running & 1;
	ertplus_cfg.toggle	= toggle & 1;
}

void set_rds_ertplus_tags(uint8_t *tags) {
	ertplus_cfg.type[0]	= tags[0] & INT8_L6;
	ertplus_cfg.start[0]	= tags[1] & INT8_L6;
	ertplus_cfg.len[0]	= tags[2] & INT8_L6;
	ertplus_cfg.type[1]	= tags[3] & INT8_L6;
	ertplus_cfg.start[1]	= tags[4] & INT8_L6;
	ertplus_cfg.len[1]	= tags[5] & INT8_L5;
}
#endif

void set_rds_af(struct rds_af_t new_af_list) {
	memcpy(&rds_data.af, &new_af_list, sizeof(struct rds_af_t));
}

void clear_rds_af() {
	memset(&rds_data.af, 0, sizeof(struct rds_af_t));
}

void set_rds_pty(uint8_t pty) {
	rds_data.pty = pty & 31;
}

void set_rds_ptyn(char *ptyn) {
	uint8_t len = 0;

	if (!ptyn[0]) {
		memset(rds_data.ptyn, 0, PTYN_LENGTH);
		return;
	}

	rds_state.ptyn_update = 1;
	memset(rds_data.ptyn, ' ', PTYN_LENGTH);
	while (*ptyn != 0 && len <= PTYN_LENGTH)
		rds_data.ptyn[len++] = *ptyn++;
}

void set_rds_ta(uint8_t ta) {
	rds_data.ta = ta & 1;
}

void set_rds_tp(uint8_t tp) {
	rds_data.tp = tp & 1;
}

void set_rds_ms(uint8_t ms) {
	rds_data.ms = ms & 1;
}

void set_rds_di(uint8_t di) {
	rds_data.di = di & 15;
}

void set_rds_ct(uint8_t ct) {
	rds_data.tx_ctime = ct & 1;
}
