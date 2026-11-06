CC ?= gcc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Isrc
LDFLAGS ?= -lm

SRC = \
	src/main.c \
	src/synth.c \
	src/midi_file.c \
	src/wav.c \
	src/data/instruments.c \
	src/data/wavetables.c \
	src/data/pcm.c

OBJ = $(SRC:.c=.o)
TARGET = gmsynth

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
