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

extern uint8_t add_rds_af(struct rds_obj_t *rds_obj, float freq);
extern void set_rds_pi(struct rds_obj_t *rds_obj, uint16_t pi_code);
extern void set_rds_rt(struct rds_obj_t *rds_obj, char *rt);
extern void set_rds_ps(struct rds_obj_t *rds_obj, char *ps);
extern void set_rds_lps(struct rds_obj_t *rds_obj, char *lps);
extern void set_rds_ert(struct rds_obj_t *rds_obj, char *ert);
extern void set_rds_rtplus_flags(struct rds_obj_t *rds_obj, uint8_t *flags);
extern void set_rds_rtplus_tags(struct rds_obj_t *rds_obj, uint8_t *tags);
extern void set_rds_ertplus_flags(struct rds_obj_t *rds_obj, uint8_t *flags);
extern void set_rds_ertplus_tags(struct rds_obj_t *rds_obj, uint8_t *tags);
extern void set_rds_ta(struct rds_obj_t *rds_obj, uint8_t ta);
extern void set_rds_pty(struct rds_obj_t *rds_obj, uint8_t pty);
extern void set_rds_ptyn(struct rds_obj_t *rds_obj, char *ptyn);
extern void set_rds_af(struct rds_obj_t *rds_obj, struct rds_af_t new_af_list);
extern void set_rds_tp(struct rds_obj_t *rds_obj, uint8_t tp);
extern void set_rds_ms(struct rds_obj_t *rds_obj, uint8_t ms);
extern void set_rds_ct(struct rds_obj_t *rds_obj, uint8_t ct);
extern void set_rds_di(struct rds_obj_t *rds_obj, uint8_t di);
