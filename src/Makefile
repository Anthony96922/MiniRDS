
# name of executable
name = minirds

# version
VERSION=1.0

# change to "1" to enable RDS2 capabilities
# (don't forget to run "make clean" first)
RDS2 = 1

# NRSC RBDS (FCC)
# Uncomment for NRSC LF/MF AF coding and PTY list
RBDS = 1

# Set to 1 for control pipe return messages
CONTROL_PIPE_MESSAGES = 0

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c11

CFLAGS += -DVERSION=\"$(VERSION)\"

obj = minirds.o waveforms.o rds.o fm_mpx.o control_pipe.o osc.o \
	resampler.o modulator.o lib.o
libs = -lm -lsamplerate -lpthread -lao

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	obj += rds2.o rds2_image_data.o
endif

ifeq ($(RBDS), 1)
	CFLAGS += -DRBDS
endif

ifeq ($(CONTROL_PIPE_MESSAGES), 1)
	CFLAGS += -DCONTROL_PIPE_MESSAGES
endif

all: $(name)

$(name): $(obj)
	$(CC) $(obj) $(libs) -o $(name) -s

clean:
	rm -f *.o
