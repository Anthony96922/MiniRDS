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
#include "net.h"
#include "lib.h"

static uint8_t stop_rds;

static void stop() {
	stop_rds = 1;
}

static inline void float2char2channel(
	float *inbuf, char *outbuf, size_t frames) {
	uint16_t j = 0, k = 0;
	int16_t sample;
	int8_t lower, upper;

	for (uint16_t i = 0; i < frames; i++) {
		sample = lroundf((inbuf[j] + inbuf[j+1]) * 16383.5f);

		/* convert from short to char */
		lower = sample & 255;
		sample >>= 8;
		upper = sample & 255;

		outbuf[k+0] = lower;
		outbuf[k+1] = upper;
		outbuf[k+2] = lower;
		outbuf[k+3] = upper;

		j += 2;
		k += 4;
	}
}

/* threads */
static void *control_pipe_worker() {
	while (!stop_rds) {
		poll_control_pipe();
		sleep(1);
	}

	close_control_pipe();
	pthread_exit(NULL);
}

static void *net_ctl_worker() {
	while (!stop_rds) {
		poll_ctl_socket();
		sleep(1);
	}

	close_ctl_socket();
	pthread_exit(NULL);
}

static void show_help(char *name, struct rds_params_t def_params) {
	printf(
		"This is MiniRDS, a lightweight RDS encoder.\n"
		"Version %s\n"
		"\n"
		"Usage: %s [options]\n"
		"\n"
		"    -m,--volume       Output volume\n"
		"\n"
		"    -i,--pi           Program Identification code\n"
		"                        [default: %04X]\n"
		"    -s,--ps           Program Service name\n"
		"                        [default: \"%s\"]\n"
		"    -r,--rt           Radio Text\n"
		"                        [default: \"%s\"]\n"
		"    -p,--pty          Program Type\n"
		"                        [default: %u]\n"
		"    -T,--tp           Traffic Program\n"
		"                        [default: %u]\n"
#ifdef RBDS
		"    -A,--af           Alternative Frequency (FM/MF)\n"
#else
		"    -A,--af           Alternative Frequency (FM/LF/MF)\n"
#endif
		"                        (more than one AF may be passed)\n"
		"    -P,--ptyn         Program Type Name\n"
		"\n"
#ifdef RBDS
		"    -S,--callsign     FCC callsign to calculate the PI code from\n"
		"                        (overrides -i,--pi)\n"
#endif
		"    -C,--ctl          FIFO control pipe\n"
		"\n"
		"    -h,--help         Show this help text and exit\n"
		"    -v,--version      Show version and exit\n"
		"\n",
		VERSION,
		name,
		def_params.pi, def_params.ps,
		def_params.rt, def_params.pty,
		def_params.tp
	);
}

static void show_version() {
	printf("MiniRDS version %s\n", VERSION);
}

/* check MPX volume level */
static uint8_t check_mpx_vol(uint8_t volume) {
	if (volume < 1 || volume > 100) {
		fprintf(stderr, "Output volume must be between 1-100.\n");
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
	char ps[PS_LENGTH+1];
	char rt[RT_LENGTH+1];
	char ptyn[PTYN_LENGTH+1];
	uint8_t volume = 50;

	/* buffers */
	float *mpx_buffer;
	float *out_buffer;
	char *dev_out;

	uint16_t port = 0;
	uint8_t proto = 1;

	int8_t r;
	size_t frames;

	/* SRC */
	SRC_STATE *src_state;
	SRC_DATA src_data;

	/* AO */
	ao_device *device;
	ao_sample_format format;

	/* pthread */
	pthread_attr_t attr;
	pthread_t control_pipe_thread;
	pthread_mutex_t control_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t control_pipe_cond;

	/* network control socket */
	pthread_t net_ctl_thread;
	pthread_mutex_t net_ctl_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t net_ctl_cond;

	const char	*short_opt = "m:R:i:s:r:p:T:A:P:"
#ifdef RBDS
	"S:"
#endif
	"C:hv";

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
#ifdef RBDS
		{"callsign",	required_argument, NULL, 'S'},
#endif
		{"ctl",		required_argument, NULL, 'C'},

		{"help",	no_argument, NULL, 'h'},
		{"version",	no_argument, NULL, 'v'},
		{ 0,		0,		0,	0 }
	};

	memset(control_pipe, 0, 51);
	memset(callsign, 0, 5);
	memset(ps, 0, PS_LENGTH + 1);
	memset(rt, 0, RT_LENGTH + 1);
	memset(ptyn, 0, PTYN_LENGTH + 1);

keep_parsing_opts:

	opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
	if (opt == -1) goto done_parsing_opts;

	switch (opt) {
		case 'm': /* volume */
			volume = strtoul(optarg, NULL, 10);
			if (check_mpx_vol(volume) > 0) return 1;
			break;

		case 'i': /* pi */
			rds_params.pi = strtoul(optarg, NULL, 16);
			break;

		case 's': /* ps */
			strncpy(ps, optarg, PS_LENGTH);
			memcpy(rds_params.ps, ps, PS_LENGTH);
			break;

		case 'r': /* rt */
			strncpy(rt, optarg, RT_LENGTH);
			memcpy(rds_params.rt, rt, RT_LENGTH);
			break;

		case 'p': /* pty */
			rds_params.pty = strtoul(optarg, NULL, 10);
			break;

		case 'T': /* tp */
			rds_params.tp = strtoul(optarg, NULL, 10);
			break;

		case 'A': /* af */
			if (add_rds_af(&rds_params.af, strtof(optarg, NULL)) == 1) return 1;
			break;

		case 'P': /* ptyn */
			strncpy(ptyn, optarg, PTYN_LENGTH);
			memcpy(rds_params.ptyn, ptyn, PTYN_LENGTH);
			break;

#ifdef RBDS
		case 'S': /* callsign */
			strncpy(callsign, optarg, 4);
			break;
#endif

		case 'C': /* ctl */
			strncpy(control_pipe, optarg, 50);
			break;

		case 'v': /* version */
			show_version();
			return 0;

		case 'h': /* help */
		case '?':
		default:
			show_help(argv[0], rds_params);
			return 1;
	}

	goto keep_parsing_opts;

done_parsing_opts:

	/* Initialize pthread stuff */
	pthread_mutex_init(&control_pipe_mutex, NULL);
	pthread_cond_init(&control_pipe_cond, NULL);
	pthread_mutex_init(&net_ctl_mutex, NULL);
	pthread_cond_init(&net_ctl_cond, NULL);
	pthread_attr_init(&attr);

	/* Setup buffers */
	mpx_buffer = malloc(NUM_MPX_FRAMES_IN * 2 * sizeof(float));
	out_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));
	dev_out = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(int16_t) * sizeof(char));

	/* Gracefully stop the encoder on SIGINT or SIGTERM */
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	/* Initialize the baseband generator */
	fm_mpx_init(MPX_SAMPLE_RATE);
	set_output_volume(volume);

	/* Initialize the RDS modulator */
	init_rds_encoder(rds_params, callsign);

	/* AO format */
	memset(&format, 0, sizeof(struct ao_sample_format));
	format.channels = 2;
	format.bits = 16;
	format.rate = OUTPUT_SAMPLE_RATE;
	format.byte_format = AO_FMT_LITTLE;

	ao_initialize();

	device = ao_open_live(ao_default_driver_id(), &format, NULL);
	if (device == NULL) {
		fprintf(stderr, "Error: cannot open sound device.\n");
		ao_shutdown();
		goto exit;
	}

	/* SRC out (MPX -> output) */
	memset(&src_data, 0, sizeof(SRC_DATA));
	src_data.input_frames = NUM_MPX_FRAMES_IN;
	src_data.output_frames = NUM_MPX_FRAMES_OUT;
	src_data.src_ratio =
		(double)OUTPUT_SAMPLE_RATE / (double)MPX_SAMPLE_RATE;
	src_data.data_in = mpx_buffer;
	src_data.data_out = out_buffer;

	r = resampler_init(&src_state, 2);
	if (r < 0) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	}

	/* Initialize the control pipe reader */
	if (control_pipe[0]) {
		if (open_control_pipe(control_pipe) == 0) {
			fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
			/* Create control pipe polling worker */
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

	/* ASCII control over network socket */
	if (port) {
		if (open_ctl_socket(port, proto) == 0) {
			fprintf(stderr, "Reading control commands on port %d.\n", port);
			r = pthread_create(&net_ctl_thread, &attr, net_ctl_worker, NULL);
			if (r < 0) {
				fprintf(stderr, "Could not create network control thread.\n");
				goto exit;
			} else {
				fprintf(stderr, "Created network control thread.\n");
			}
		} else {
			fprintf(stderr, "Failed to open port %d.\n", port);
		}
	}

	for (;;) {
		fm_rds_get_frames(mpx_buffer, NUM_MPX_FRAMES_IN);

		if (resample(src_state, src_data, &frames) < 0) break;

		float2char2channel(out_buffer, dev_out, frames);

		/* num_bytes = audio frames * channels * bytes per sample */
		if (!ao_play(device, dev_out, frames * 2 * sizeof(int16_t))) {
			fprintf(stderr, "Error: could not play audio.\n");
			break;
		}

		if (stop_rds) {
			fprintf(stderr, "Stopping...\n");
			break;
		}
	}

	resampler_exit(src_state);

exit:
	if (control_pipe[0]) {
		/* shut down threads */
		fprintf(stderr, "Waiting for pipe thread to shut down.\n");
		pthread_cond_signal(&control_pipe_cond);
		pthread_join(control_pipe_thread, NULL);
	}

	if (port) {
		fprintf(stderr, "Waiting for net socket thread to shut down.\n");
		pthread_cond_signal(&net_ctl_cond);
		pthread_join(net_ctl_thread, NULL);
	}

	pthread_attr_destroy(&attr);

	fm_mpx_exit();
	exit_rds_encoder();

	free(mpx_buffer);
	free(out_buffer);
	free(dev_out);

	return 0;
}
