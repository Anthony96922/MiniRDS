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
#include <signal.h>
#include <getopt.h>
#include <pthread.h>
#include <ao/ao.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"
#include "resampler.h"

// pthread
static pthread_t control_pipe_thread;

static pthread_mutex_t control_pipe_mutex	= PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t control_pipe_cond;

static uint8_t stop_rds;

static void stop() {
	stop_rds = 1;
}

static inline void float2char2channel(float *inbuf, char *outbuf, size_t frames) {
	float in;
	uint16_t j = 0, k = 0;
	int16_t sample;

	for (uint16_t i = 0; i < frames; i++) {
		in = (inbuf[k+0] + inbuf[k+1]) / 2.0f;
		sample = lroundf(in * 32767.0f);
		outbuf[j+0] = outbuf[j+2] = sample & 255;
		outbuf[j+1] = outbuf[j+3] = sample >> 8;
		j += 4;
		k += 2;
	}
}

// threads
static void *control_pipe_worker() {
	while (!stop_rds) {
		poll_control_pipe();
		usleep(10000);
	}

	close_control_pipe();
	pthread_exit(NULL);
}

static void show_help(char *name, struct rds_params_t def_params) {
	fprintf(stderr,
		"This is MiniRDS, a lightweight RDS encoder.\n"
		"\n"
		"Usage: %s [options]\n"
		"\n"
		"    -m,--volume       RDS volume\n"
		"    -i,--pi           Program Identification code [default: %04X]\n"
		"    -s,--ps           Program Service name [default: \"%s\"]\n"
		"    -r,--rt           Radio Text [default: \"%s\"]\n"
		"    -p,--pty          Program Type [default: %d]\n"
		"    -T,--tp           Traffic Program [default: %d]\n"
		"    -A,--af           Alternative Frequency\n"
		"                        (more than one AF may be passed)\n"
		"    -P,--ptyn         PTY Name\n"
		"    -S,--callsign     Callsign to calculate the PI code from\n"
		"                        (overrides -i/--pi)\n"
		"    -C,--ctl          Control pipe\n"
		"\n",
		name,
		def_params.pi, def_params.ps,
		def_params.rt, def_params.pty,
		def_params.tp
	);
}

// check MPX volume level
static uint8_t check_mpx_vol(uint8_t volume) {
	if (volume < 1 || volume > 100) {
		fprintf(stderr, "RDS volume must be between 1 - 100.\n");
		return 1;
	}
	return 0;
}

int main(int argc, char **argv) {
	int opt;
	char control_pipe[51];
	struct rds_params_t rds_params = {
		.ps = "MiniRDS",
		.rt = "MiniRDS: Software RDS encoder",
		.pi = 0x1000
	};
	char callsign[5];
	char tmp_ps[PS_LENGTH+1];
	char tmp_rt[RT_LENGTH+1];
	char tmp_ptyn[PTYN_LENGTH+1];
	uint8_t volume = 50;

	// buffers
	float *mpx_buffer;
	float *out_buffer;
	char *dev_out;

	int8_t r;
	size_t frames;

	// SRC
	SRC_STATE *src_state;
	SRC_DATA src_data;

	// AO
	ao_device *device;
	ao_sample_format format;

	// pthread
	pthread_attr_t attr;

	const char	*short_opt = "m:R:i:s:r:p:T:A:P:S:C:h";
	struct option	long_opt[] =
	{
		{"volume",	required_argument, NULL, 'm'},

		{"rds",		required_argument, NULL, 'R'},
		{"pi",		required_argument, NULL, 'i'},
		{"ps",		required_argument, NULL, 's'},
		{"rt",		required_argument, NULL, 'r'},
		{"pty",		required_argument, NULL, 'p'},
		{"tp",		required_argument, NULL, 'T'},
		{"af",		required_argument, NULL, 'A'},
		{"ptyn",	required_argument, NULL, 'P'},
		{"callsign",	required_argument, NULL, 'S'},
		{"ctl",		required_argument, NULL, 'C'},

		{"help",	no_argument, NULL, 'h'},
		{ 0,		0,		0,	0 }
	};

	memset(control_pipe, 0, 51);
	memset(callsign, 0, 5);
	memset(tmp_ps, 0, PS_LENGTH+1);
	memset(tmp_rt, 0, RT_LENGTH+1);
	memset(tmp_ptyn, 0, PTYN_LENGTH+1);

	while ((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
		switch (opt) {
			case 'm': //volume
				volume = strtoul(optarg, NULL, 10);
				if (check_mpx_vol(volume) > 0) return 1;
				break;

			case 'i': //pi
				rds_params.pi = strtoul(optarg, NULL, 16);
				break;

			case 's': //ps
				strncpy(tmp_ps, optarg, 8);
				memcpy(rds_params.ps, tmp_ps, 8);
				break;

			case 'r': //rt
				strncpy(tmp_rt, optarg, 64);
				memcpy(rds_params.rt, tmp_rt, 64);
				break;

			case 'p': //pty
				rds_params.pty = strtoul(optarg, NULL, 10);
				break;

			case 'T': //tp
				rds_params.tp = strtoul(optarg, NULL, 10);
				break;

			case 'A': //af
				if (add_rds_af(&rds_params.af, strtof(optarg, NULL)) < 0) return 1;
				break;

			case 'P': //ptyn
				strncpy(tmp_ptyn, optarg, 8);
				memcpy(rds_params.ptyn, tmp_ptyn, 8);
				break;

			case 'S': //callsign
				strncpy(callsign, optarg, 4);
				break;

			case 'C': //ctl
				strncpy(control_pipe, optarg, 50);
				break;

			case 'h': //help
			case '?':
			default:
				show_help(argv[0], rds_params);
				return 1;
		}
	}

	// Initialize pthread stuff
	pthread_mutex_init(&control_pipe_mutex, NULL);
	pthread_cond_init(&control_pipe_cond, NULL);
	pthread_attr_init(&attr);

	// Setup buffers
	mpx_buffer = malloc(NUM_MPX_FRAMES_IN * 2 * sizeof(float));
	out_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));
	dev_out = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(int16_t) * sizeof(char));

	// Gracefully stop the encoder on SIGINT or SIGTERM
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	memset(&format, 0, sizeof(struct ao_sample_format));
	format.channels = 2;
	format.bits = 16;
	format.rate = OUTPUT_SAMPLE_RATE;
	format.byte_format = AO_FMT_LITTLE;

	ao_initialize();

	if ((device = ao_open_live(ao_default_driver_id(), &format, NULL)) == NULL) {
		fprintf(stderr, "Error: cannot open sound device.\n");
		ao_shutdown();
		return -1;
	}

	// SRC out (MPX -> output)
	src_data.input_frames = NUM_MPX_FRAMES_IN;
	src_data.output_frames = NUM_MPX_FRAMES_OUT;
	src_data.src_ratio = (double)OUTPUT_SAMPLE_RATE / (double)MPX_SAMPLE_RATE;
	src_data.data_in = mpx_buffer;
	src_data.data_out = out_buffer;
	src_data.end_of_input = 0;

	if (resampler_init(&src_state, 2) < 0) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	}

	// Initialize the baseband generator
	fm_mpx_init();
	set_output_volume(volume);

	// Initialize the RDS modulator
	init_rds_encoder(rds_params, callsign);

	// SRC in (input -> MPX)
	r = resampler_init(&src_state, 2);
	if (r < 0) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	}

	pthread_attr_destroy(&attr);

	// Initialize the control pipe reader
	if(control_pipe[0]) {
		if(open_control_pipe(control_pipe) == 0) {
			fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
			// Create control pipe polling worker
			r = pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL);
			if (r < 0) {
				fprintf(stderr, "Could not create control pipe thread.\n");
				goto exit;
			} else {
				fprintf(stderr, "Created control pipe thread.\n");
			}
		} else {
			fprintf(stderr, "Failed to open control pipe: %s.\n", control_pipe);
		}
	}

	for (;;) {
		fm_rds_get_frames(mpx_buffer);

		if (resample(src_state, src_data, &frames) < 0) break;

		float2char2channel(out_buffer, dev_out, frames);

		// num_bytes = audio frames * channels * bytes per sample
		if (!ao_play(device, dev_out, frames * 2 * sizeof(int16_t))) {
			fprintf(stderr, "Error: could not play audio.\n");
			break;
		}

		if (stop_rds) {
			fprintf(stderr, "Stopping...\n");
			break;
		}
	}

exit:
	if(control_pipe[0]) {
		// shut down threads
		fprintf(stderr, "Waiting for threads to shut down.\n");
		pthread_cond_signal(&control_pipe_cond);
		pthread_join(control_pipe_thread, NULL);
	}

	resampler_exit(src_state);

	fm_mpx_exit();

	free(mpx_buffer);
	free(out_buffer);
	free(dev_out);

	return 0;
}
