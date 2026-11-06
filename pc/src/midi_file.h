#ifndef MIDI_FILE_H
#define MIDI_FILE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MIDI_EV_NOTE_ON = 0,
    MIDI_EV_NOTE_OFF = 1,
    MIDI_EV_PROGRAM = 2,
    MIDI_EV_CC = 3,
    MIDI_EV_PITCH_BEND = 4,
    MIDI_EV_TEMPO = 5
} MidiEventType;

typedef struct {
    uint32_t tick;
    uint32_t order;
    uint32_t tempo;
    double time_sec;
    uint8_t type;
    uint8_t channel;
    uint8_t a;
    uint8_t b;
} MidiEvent;

typedef struct {
    MidiEvent *events;
    size_t count;
    size_t capacity;
    uint16_t division;
    double duration_sec;
} MidiFile;

int midi_file_load(const char *path, MidiFile *mf);
void midi_file_free(MidiFile *mf);

#endif
