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
#include "resampler.h"

int8_t resampler_init(SRC_STATE **src_state, uint8_t channels) {
	int src_error;

	*src_state = src_new(CONVERTER_TYPE, channels, &src_error);

	if (*src_state == NULL) {
		fprintf(stderr, "Error: src_new failed: %s\n", src_strerror(src_error));
		return -1;
	}

	return 0;
}

int8_t resample(SRC_STATE *src_state, SRC_DATA src_data, size_t *frames_generated) {
	int src_error;

	src_error = src_process(src_state, &src_data);

	if (src_error) {
		fprintf(stderr, "Error: src_process failed: %s\n", src_strerror(src_error));
		return -1;
	}

	*frames_generated = src_data.output_frames_gen;

	return 0;
}

void resampler_exit(SRC_STATE *src_state) {
	src_delete(src_state);
}
