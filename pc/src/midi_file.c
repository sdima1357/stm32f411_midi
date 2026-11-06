#include "midi_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t read_be32(FILE *f)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) {
        return 0;
    }
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static uint16_t read_be16(FILE *f)
{
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) {
        return 0;
    }
    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

static uint32_t read_vlq(FILE *f)
{
    uint32_t value = 0;
    int c;
    int n = 0;
    do {
        c = fgetc(f);
        if (c == EOF) {
            return value;
        }
        value = (value << 7) | (uint32_t)(c & 0x7F);
        n++;
    } while ((c & 0x80) && n < 4);
    return value;
}

static int event_push(MidiFile *mf, const MidiEvent *ev)
{
    if (mf->count == mf->capacity) {
        size_t cap = mf->capacity ? mf->capacity * 2 : 1024;
        MidiEvent *p = (MidiEvent *)realloc(mf->events, cap * sizeof(MidiEvent));
        if (!p) {
            return -1;
        }
        mf->events = p;
        mf->capacity = cap;
    }
    mf->events[mf->count] = *ev;
    mf->count++;
    return 0;
}

static int event_cmp(const void *pa, const void *pb)
{
    const MidiEvent *a = (const MidiEvent *)pa;
    const MidiEvent *b = (const MidiEvent *)pb;
    if (a->tick < b->tick) {
        return -1;
    }
    if (a->tick > b->tick) {
        return 1;
    }
    if (a->order < b->order) {
        return -1;
    }
    if (a->order > b->order) {
        return 1;
    }
    return 0;
}

int midi_file_load(const char *path, MidiFile *mf)
{
    FILE *f;
    char id[4];
    uint32_t hlen;
    uint16_t format;
    uint16_t ntrks;
    uint16_t division;
    uint16_t tr;
    uint32_t order = 0;

    memset(mf, 0, sizeof(*mf));

    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }

    if (fread(id, 1, 4, f) != 4 || memcmp(id, "MThd", 4) != 0) {
        fclose(f);
        return -1;
    }
    hlen = read_be32(f);
    if (hlen < 6) {
        fclose(f);
        return -1;
    }
    format = read_be16(f);
    ntrks = read_be16(f);
    division = read_be16(f);
    (void)format;
    if (hlen > 6) {
        fseek(f, (long)(hlen - 6), SEEK_CUR);
    }

    if (division & 0x8000u) {
        /* SMPTE timing is not supported. */
        fclose(f);
        return -1;
    }
    mf->division = division ? division : 480;

    for (tr = 0; tr < ntrks; tr++) {
        uint32_t tlen;
        long track_end;
        uint32_t tick = 0;
        uint8_t running = 0;

        if (fread(id, 1, 4, f) != 4) {
            break;
        }
        tlen = read_be32(f);
        if (memcmp(id, "MTrk", 4) != 0) {
            fseek(f, (long)tlen, SEEK_CUR);
            continue;
        }
        track_end = ftell(f) + (long)tlen;

        while (ftell(f) < track_end) {
            uint32_t delta = read_vlq(f);
            int c;
            tick += delta;

            c = fgetc(f);
            if (c == EOF) {
                break;
            }

            if (c < 0x80) {
                if (!running) {
                    break;
                }
                ungetc(c, f);
                c = running;
            } else {
                running = (uint8_t)c;
            }

            if (c == 0xFF) {
                int meta = fgetc(f);
                uint32_t mlen = read_vlq(f);
                if (meta == 0x51 && mlen == 3) {
                    int b0 = fgetc(f);
                    int b1 = fgetc(f);
                    int b2 = fgetc(f);
                    MidiEvent ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.tick = tick;
                    ev.order = order++;
                    ev.type = MIDI_EV_TEMPO;
                    ev.tempo = ((uint32_t)(b0 & 0xFF) << 16) |
                               ((uint32_t)(b1 & 0xFF) << 8) |
                               (uint32_t)(b2 & 0xFF);
                    event_push(mf, &ev);
                } else {
                    fseek(f, (long)mlen, SEEK_CUR);
                }
                if (meta == 0x2F) {
                    break;
                }
            } else if (c == 0xF0 || c == 0xF7) {
                uint32_t slen = read_vlq(f);
                fseek(f, (long)slen, SEEK_CUR);
                running = 0;
            } else {
                uint8_t type = (uint8_t)(c & 0xF0);
                uint8_t ch = (uint8_t)(c & 0x0F);
                MidiEvent ev;

                if (type == 0x80 || type == 0x90) {
                    int note = fgetc(f);
                    int vel = fgetc(f);
                    memset(&ev, 0, sizeof(ev));
                    ev.tick = tick;
                    ev.order = order++;
                    ev.channel = ch;
                    ev.a = (uint8_t)(note & 0x7F);
                    ev.b = (uint8_t)(vel & 0x7F);
                    if (type == 0x90 && vel > 0) {
                        ev.type = MIDI_EV_NOTE_ON;
                    } else {
                        ev.type = MIDI_EV_NOTE_OFF;
                    }
                    event_push(mf, &ev);
                } else if (type == 0xA0) {
                    (void)fgetc(f);
                    (void)fgetc(f);
                } else if (type == 0xB0) {
                    int cc = fgetc(f);
                    int val = fgetc(f);
                    memset(&ev, 0, sizeof(ev));
                    ev.tick = tick;
                    ev.order = order++;
                    ev.type = MIDI_EV_CC;
                    ev.channel = ch;
                    ev.a = (uint8_t)(cc & 0x7F);
                    ev.b = (uint8_t)(val & 0x7F);
                    event_push(mf, &ev);
                } else if (type == 0xC0) {
                    int prog = fgetc(f);
                    memset(&ev, 0, sizeof(ev));
                    ev.tick = tick;
                    ev.order = order++;
                    ev.type = MIDI_EV_PROGRAM;
                    ev.channel = ch;
                    ev.a = (uint8_t)(prog & 0x7F);
                    event_push(mf, &ev);
                } else if (type == 0xD0) {
                    (void)fgetc(f);
                } else if (type == 0xE0) {
                    int lsb = fgetc(f);
                    int msb = fgetc(f);
                    memset(&ev, 0, sizeof(ev));
                    ev.tick = tick;
                    ev.order = order++;
                    ev.type = MIDI_EV_PITCH_BEND;
                    ev.channel = ch;
                    ev.a = (uint8_t)(lsb & 0x7F);
                    ev.b = (uint8_t)(msb & 0x7F);
                    event_push(mf, &ev);
                } else {
                    break;
                }
            }
        }
        fseek(f, track_end, SEEK_SET);
    }

    fclose(f);

    if (mf->count == 0) {
        return 0;
    }

    qsort(mf->events, mf->count, sizeof(MidiEvent), event_cmp);

    {
        double t = 0.0;
        uint32_t last_tick = 0;
        uint32_t tempo = 500000;
        size_t i;
        double ticks_per_sec = (double)mf->division;

        for (i = 0; i < mf->count; i++) {
            MidiEvent *ev = &mf->events[i];
            double dt = (double)(ev->tick - last_tick) / ticks_per_sec;
            t += dt * ((double)tempo / 1000000.0);
            ev->time_sec = t;
            last_tick = ev->tick;
            if (ev->type == MIDI_EV_TEMPO && ev->tempo > 0) {
                tempo = ev->tempo;
            }
        }
        mf->duration_sec = t;
    }

    return 0;
}

void midi_file_free(MidiFile *mf)
{
    free(mf->events);
    mf->events = NULL;
    mf->count = 0;
    mf->capacity = 0;
}
