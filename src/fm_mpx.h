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

enum mpx_subcarriers {
	MPX_SUBCARRIER_ST_PILOT,
	MPX_SUBCARRIER_RDS_STREAM_0,
#ifdef RDS2
	MPX_SUBCARRIER_RDS2_STREAM_1,
	MPX_SUBCARRIER_RDS2_STREAM_2,
	MPX_SUBCARRIER_RDS2_STREAM_3,
#endif
	MPX_SUBCARRIER_END
};

extern void fm_mpx_init(uint32_t sample_rate);
extern void fm_rds_get_frames(float *outbuf, size_t num_frames);
extern void fm_mpx_exit();
extern void set_output_volume(float vol);
extern void set_carrier_volume(uint8_t carrier, float new_volume);
