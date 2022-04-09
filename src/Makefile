
# name of executable
name = minirds

# version
VERSION=1.0

# change to "1" to enable RDS2 capabilities
# (don't forget to run "make clean" first)
RDS2 = 1

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c11

CFLAGS += -DVERSION=\"$(VERSION)\"

obj = minirds.o rds.o fm_mpx.o control_pipe.o osc.o \
	resampler.o modulator.o lib.o
libs = -lm -lsamplerate -lpthread -lao

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	obj += rds2.o rds2_image_data.o
endif

all: $(name)

$(name): $(obj)
	$(CC) $(obj) $(libs) -o $(name) -s

clean:
	rm -f *.o
