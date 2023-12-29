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

#ifndef RDS_H
#define RDS_H

/* The RDS error-detection code generator polynomial is
 * x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + x^0
 */
#define POLY			0x1B9
#define POLY_DEG		10
#define BLOCK_SIZE		16

#define GROUP_LENGTH		4
#define BITS_PER_GROUP		(GROUP_LENGTH * (BLOCK_SIZE + POLY_DEG))
#define RDS_SAMPLE_RATE		190000
#define SAMPLES_PER_BIT		160
#define FILTER_SIZE		1120
#define SAMPLE_BUFFER_SIZE	(SAMPLES_PER_BIT + FILTER_SIZE)

/* Text items
 *
 */
#define RT_LENGTH	64
#define PS_LENGTH	8
#define PTYN_LENGTH	8
#define LPS_LENGTH	32
#define ERT_LENGTH	128

/* AF list size
 *
 */
#define MAX_AFS 25

typedef struct rds_af_t {
	uint8_t num_entries;
	uint8_t num_afs;
	uint8_t afs[MAX_AFS];
} rds_af_t;

/* AF codes */
#define AF_CODE_FILLER		205
#define AF_CODE_NO_AF		224
#define AF_CODE_NUM_AFS_BASE	AF_CODE_NO_AF
#define AF_CODE_LFMF_FOLLOWS	250

typedef struct rds_params_t {
	uint16_t pi;
	uint8_t ta;
	uint8_t pty;
	uint8_t tp;
	uint8_t ms;
	uint8_t di;
	/* PS */
	char ps[PS_LENGTH];
	/* RT */
	char rt[RT_LENGTH];
	/* PTYN */
	char ptyn[PTYN_LENGTH];

	/* AF */
	struct rds_af_t af;

	uint8_t tx_ctime;

	/* Long PS */
	char lps[LPS_LENGTH];

	/* eRT */
	char ert[ERT_LENGTH];
} rds_params_t;
/* Here, the first member of the struct must be a scalar to avoid a
   warning on -Wmissing-braces with GCC < 4.8.3
   (bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)
*/

/* Group type
 *
 * 0-15
 */
#define GROUP_TYPE_0	( 0 << 4)
#define GROUP_TYPE_1	( 1 << 4)
#define GROUP_TYPE_2	( 2 << 4)
#define GROUP_TYPE_3	( 3 << 4)
#define GROUP_TYPE_4	( 4 << 4)
#define GROUP_TYPE_5	( 5 << 4)
#define GROUP_TYPE_6	( 6 << 4)
#define GROUP_TYPE_7	( 7 << 4)
#define GROUP_TYPE_8	( 8 << 4)
#define GROUP_TYPE_9	( 9 << 4)
#define GROUP_TYPE_10	(10 << 4)
#define GROUP_TYPE_11	(11 << 4)
#define GROUP_TYPE_12	(12 << 4)
#define GROUP_TYPE_13	(13 << 4)
#define GROUP_TYPE_14	(14 << 4)
#define GROUP_TYPE_15	(15 << 4)

/* Group versions
 *
 * The first 4 bits are the group number and the remaining 4 are
 * the group version
 */
#define GROUP_VER_A	0
#define GROUP_VER_B	1

/* Version A groups */
#define GROUP_0A	(GROUP_TYPE_0  | GROUP_VER_A)
#define GROUP_1A	(GROUP_TYPE_1  | GROUP_VER_A)
#define GROUP_2A	(GROUP_TYPE_2  | GROUP_VER_A)
#define GROUP_3A	(GROUP_TYPE_3  | GROUP_VER_A)
#define GROUP_4A	(GROUP_TYPE_4  | GROUP_VER_A)
#define GROUP_5A	(GROUP_TYPE_5  | GROUP_VER_A)
#define GROUP_6A	(GROUP_TYPE_6  | GROUP_VER_A)
#define GROUP_7A	(GROUP_TYPE_7  | GROUP_VER_A)
#define GROUP_8A	(GROUP_TYPE_8  | GROUP_VER_A)
#define GROUP_9A	(GROUP_TYPE_9  | GROUP_VER_A)
#define GROUP_10A	(GROUP_TYPE_10 | GROUP_VER_A)
#define GROUP_11A	(GROUP_TYPE_11 | GROUP_VER_A)
#define GROUP_12A	(GROUP_TYPE_12 | GROUP_VER_A)
#define GROUP_13A	(GROUP_TYPE_13 | GROUP_VER_A)
#define GROUP_14A	(GROUP_TYPE_14 | GROUP_VER_A)
#define GROUP_15A	(GROUP_TYPE_15 | GROUP_VER_A)

/* Version B groups */
#define GROUP_0B	(GROUP_TYPE_0  | GROUP_VER_B)
#define GROUP_1B	(GROUP_TYPE_1  | GROUP_VER_B)
#define GROUP_2B	(GROUP_TYPE_2  | GROUP_VER_B)
#define GROUP_3B	(GROUP_TYPE_3  | GROUP_VER_B)
#define GROUP_4B	(GROUP_TYPE_4  | GROUP_VER_B)
#define GROUP_5B	(GROUP_TYPE_5  | GROUP_VER_B)
#define GROUP_6B	(GROUP_TYPE_6  | GROUP_VER_B)
#define GROUP_7B	(GROUP_TYPE_7  | GROUP_VER_B)
#define GROUP_8B	(GROUP_TYPE_8  | GROUP_VER_B)
#define GROUP_9B	(GROUP_TYPE_9  | GROUP_VER_B)
#define GROUP_10B	(GROUP_TYPE_10 | GROUP_VER_B)
#define GROUP_11B	(GROUP_TYPE_11 | GROUP_VER_B)
#define GROUP_12B	(GROUP_TYPE_12 | GROUP_VER_B)
#define GROUP_13B	(GROUP_TYPE_13 | GROUP_VER_B)
#define GROUP_14B	(GROUP_TYPE_14 | GROUP_VER_B)
#define GROUP_15B	(GROUP_TYPE_15 | GROUP_VER_B)

#define GET_GROUP_TYPE(x)	((x >> 4) & 15)
#define GET_GROUP_VER(x)	(x & 1) /* only check bit 0 */

#define DI_STEREO	(1 << 0) /* 1 - Stereo */
#define DI_AH		(1 << 1) /* 2 - Artificial Head */
#define DI_COMPRESSED	(1 << 2) /* 4 - Compressed */
#define DI_DPTY		(1 << 3) /* 8 - Dynamic PTY */

/* Bit mask */

/* 8 bit */
#define INT8_ALL	0xff
/* Lower */
#define INT8_L1		0x01
#define INT8_L2		0x03
#define INT8_L3		0x07
#define INT8_L4		0x0f
#define INT8_L5		0x1f
#define INT8_L6		0x3f
#define INT8_L7		0x7f
/* Upper */
#define INT8_U7		0xfe
#define INT8_U6		0xfc
#define INT8_U5		0xf8
#define INT8_U4		0xf0
#define INT8_U3		0xe0
#define INT8_U2		0xc0
#define INT8_U1		0x80
/* Single */
#define INT8_0		0x01
#define INT8_1		0x02
#define INT8_2		0x04
#define INT8_3		0x08
#define INT8_4		0x10
#define INT8_5		0x20
#define INT8_6		0x40
#define INT8_7		0x80

/* 16 bit */
#define INT16_ALL	0xffff
/* Lower */
#define INT16_L1	0x0001
#define INT16_L2	0x0003
#define INT16_L3	0x0007
#define INT16_L4	0x000f
#define INT16_L5	0x001f
#define INT16_L6	0x003f
#define INT16_L7	0x007f
#define INT16_L8	0x00ff
#define INT16_L9	0x01ff
#define INT16_L10	0x03ff
#define INT16_L11	0x07ff
#define INT16_L12	0x0fff
#define INT16_L13	0x1fff
#define INT16_L14	0x3fff
#define INT16_L15	0x7fff
/* Upper */
#define INT16_U15	0xfffe
#define INT16_U14	0xfffc
#define INT16_U13	0xfff8
#define INT16_U12	0xfff0
#define INT16_U11	0xffe0
#define INT16_U10	0xffc0
#define INT16_U9	0xff80
#define INT16_U8	0xff00
#define INT16_U7	0xfe00
#define INT16_U6	0xfc00
#define INT16_U5	0xf800
#define INT16_U4	0xf000
#define INT16_U3	0xe000
#define INT16_U2	0xc000
#define INT16_U1	0x8000
/* Single */
#define INT16_0		0x0001
#define INT16_1		0x0002
#define INT16_2		0x0004
#define INT16_3		0x0008
#define INT16_4		0x0010
#define INT16_5		0x0020
#define INT16_6		0x0040
#define INT16_7		0x0080
#define INT16_8		0x0100
#define INT16_9		0x0200
#define INT16_10	0x0400
#define INT16_11	0x0800
#define INT16_12	0x1000
#define INT16_13	0x2000
#define INT16_14	0x4000
#define INT16_15	0x8000

/* 18 bit (for RDS2) */
#define INT18_U2	0x30000
#define INT18_L4	0x0000f

#define IS_TYPE_B(a)	(a[1] & INT16_11)
#ifdef RDS2
#define IS_TUNNELING(a)	(a[0] == 0x0000)
#endif

/*
 * RDS ODA ID group
 *
 * This struct is for defining ODAs that will be transmitted
 *
 * Can signal version A or B data groups
 */
typedef struct rds_oda_t {
	uint8_t group;
	uint16_t aid;
	uint16_t scb;
} rds_oda_t;

#define MAX_ODAS	8


/*
 * ODA AID
 *
 * Extensive list: https://www.nrscstandards.org/committees/dsm/archive/rds-oda-aids.pdf
 */
#define	ODA_AID_RTPLUS	0x4bd7
#define ODA_AID_ERT	0x6552
#define ODA_AID_ERTPLUS	0x4bd8
#define ODA_9BIT_AF	0x6365
/* RDS2 */
#define ODA_AID_RFT	0xff7f
#define ODA_AID_RFTPLUS	0xff80

#ifdef RBDS
extern void init_rds_encoder(struct rds_params_t rds_params, char *call_sign);
#else
extern void init_rds_encoder(struct rds_params_t rds_params);
#endif
extern void exit_rds_encoder();
extern void get_rds_bits(uint8_t *bits);
extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_lps(char *lps);
extern void set_rds_ert(char *ert);
extern void set_rds_rtplus_flags(uint8_t flags);
extern void set_rds_rtplus_tags(uint8_t *tags);
extern void set_rds_ertplus_flags(uint8_t flags);
extern void set_rds_ertplus_tags(uint8_t *tags);
extern void set_rds_ta(uint8_t ta);
extern void set_rds_pty(uint8_t pty);
extern void set_rds_ptyn(char *ptyn);
extern void set_rds_af(struct rds_af_t new_af_list);
extern void set_rds_tp(uint8_t tp);
extern void set_rds_ms(uint8_t ms);
extern void set_rds_ct(uint8_t ct);
extern void set_rds_di(uint8_t di);
extern float get_rds_sample(uint8_t stream_num);

#endif /* RDS_H */
