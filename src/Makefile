
# name of executable
name = minirds

# version
VERSION=1.0

# change to "1" to enable RDS2 capabilities
# (don't forget to run "make clean" first)
RDS2 = 1

# Shift RDS2 stream 1, 2 and 3 carriers by 90, 180 and 270 degress
# respectively to reduce the peak amplitude
RDS2_QUADRATURE_CARRIER = 1

# RDS2 symbol shifting to further reduce peak amplitude
RDS2_SYMBOL_SHIFTING = 1

# RDS2 debugging
RDS2_DEBUG = 0

# NRSC RBDS (FCC)
# Set to 1 for NRSC LF/MF AF coding and PTY list
RBDS = 1

# Use a static libsamplerate library (.a)
# Disabled by default
STATIC_LIBSAMPLERATE ?= 0
LIBSAMPLERATE_DIR ?= ./libsamplerate

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c18

CFLAGS += -DVERSION=\"$(VERSION)\"

obj = minirds.o waveforms.o rds.o fm_mpx.o control_pipe.o osc.o \
	resampler.o modulator.o lib.o net.o ascii_cmd.o
libs = -lm -lpthread -lao

ifeq ($(STATIC_LIBSAMPLERATE), 1)
	libs += $(LIBSAMPLERATE_DIR)/lib/libsamplerate.a
	CFLAGS += -I$(LIBSAMPLERATE_DIR)/include
else
	libs += -lsamplerate
endif

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	obj += rds2.o
ifeq ($(RDS2_QUADRATURE_CARRIER), 1)
	CFLAGS += -DRDS2_QUADRATURE_CARRIER
endif
ifeq ($(RDS2_SYMBOL_SHIFTING), 1)
	CFLAGS += -DRDS2_SYMBOL_SHIFTING
endif
endif

ifeq ($(RDS2_DEBUG), 1)
	CFLAGS += -DRDS2_DEBUG
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
