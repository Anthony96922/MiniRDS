/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2021 Anthony96922
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

extern void msleep(unsigned long ms);

extern int ustrcmp(const unsigned char *s1, const unsigned char *s2);

extern uint8_t get_pty_code(char *pty_str);
extern char *get_pty_str(uint8_t pty_code);
extern uint8_t get_rtp_tag_id(char *rtp_tag_name);
extern char *get_rtp_tag_name(uint8_t rtp_tag);
#ifdef RDS2
extern void add_checkwords(uint16_t *blocks, uint8_t *bits, bool rds2);
#else
extern void add_checkwords(uint16_t *blocks, uint8_t *bits);
#endif
extern uint16_t callsign2pi(unsigned char *callsign);
extern uint8_t add_rds_af(struct rds_af_t *af_list, float freq);
extern char *show_af_list(struct rds_af_t af_list);
extern uint16_t crc16(uint8_t *data, size_t len);
extern unsigned char *xlat(unsigned char *str);

/* TMC */
extern uint16_t tmc_encrypt(uint16_t loc, uint16_t key);
extern uint16_t tmc_decrypt(uint16_t loc, uint16_t key);
