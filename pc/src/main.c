#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "synth.h"
#include "midi_file.h"
#include "wav.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [-g gain] input.mid output.wav\n"
            "Renders a Standard MIDI File through the GM synth to a mono\n"
            "22050 Hz 16-bit WAV file.  Default gain is 0.55.\n",
            argv0);
}

int main(int argc, char **argv)
{
    MidiFile mf;
    int16_t *buf;
    uint32_t total;
    uint32_t pos = 0;
    size_t i;
    double duration;
    uint32_t last;
    int peak = 0;
    float gain = 0.55f;
    int argi = 1;

    if (argc >= 4 && (strcmp(argv[1], "-g") == 0 || strcmp(argv[1], "--gain") == 0)) {
        gain = (float)atof(argv[2]);
        argi = 3;
    }
    if (argc - argi < 2) {
        usage(argv[0]);
        return 1;
    }

    if (midi_file_load(argv[argi], &mf) != 0) {
        fprintf(stderr, "error: cannot read MIDI file '%s'\n", argv[argi]);
        return 1;
    }

    duration = mf.duration_sec + 5.0;
    if (duration < 1.0) {
        duration = 1.0;
    }
    /* Guard against malformed tempo maps causing huge allocations. */
    if (duration > 7200.0) {
        fprintf(stderr, "warning: MIDI duration capped at 2 hours\n");
        duration = 7200.0;
    }
    total = (uint32_t)(duration * (double)SYNTH_SAMPLE_RATE);
    buf = (int16_t *)calloc(total ? total : 1, sizeof(int16_t));
    if (!buf) {
        fprintf(stderr, "error: out of memory (%u samples)\n", total);
        midi_file_free(&mf);
        return 1;
    }

    synth_init();
    synth_set_master_volume(gain);

    for (i = 0; i < mf.count; i++) {
        const MidiEvent *ev = &mf.events[i];
        double target_d = ev->time_sec * (double)SYNTH_SAMPLE_RATE;
        uint32_t target = (target_d > (double)total) ? total : (uint32_t)target_d;
        while (pos < target) {
            uint32_t n = target - pos;
            if (n > 256) {
                n = 256;
            }
            synth_render_i16(buf + pos, n);
            pos += n;
        }

        switch (ev->type) {
        case MIDI_EV_NOTE_ON:
            synth_note_on(ev->channel, ev->a, ev->b);
            break;
        case MIDI_EV_NOTE_OFF:
            synth_note_off(ev->channel, ev->a);
            break;
        case MIDI_EV_PROGRAM:
            synth_program_change(ev->channel, ev->a);
            break;
        case MIDI_EV_CC:
            synth_control_change(ev->channel, ev->a, ev->b);
            break;
        case MIDI_EV_PITCH_BEND: {
            int bend = ((int)ev->b << 7 | (int)ev->a) - 8192;
            synth_pitch_bend(ev->channel, (int16_t)bend);
            break;
        }
        default:
            break;
        }
    }

    while (pos < total) {
        uint32_t n = total - pos;
        if (n > 256) {
            n = 256;
        }
        synth_render_i16(buf + pos, n);
        pos += n;
    }

    last = total;
    while (last > 0 && abs((int)buf[last - 1]) < 8) {
        last--;
    }
    last += SYNTH_SAMPLE_RATE / 10u;
    if (last > total) {
        last = total;
    }

    for (i = 0; i < last; i++) {
        int v = abs((int)buf[i]);
        if (v > peak) {
            peak = v;
        }
    }

    if (wav_write_i16(argv[argi + 1], buf, last, SYNTH_SAMPLE_RATE) != 0) {
        fprintf(stderr, "error: cannot write WAV file '%s'\n", argv[argi + 1]);
        free(buf);
        midi_file_free(&mf);
        return 1;
    }

    printf("Rendered %s -> %s\n", argv[argi], argv[argi + 1]);
    printf("  MIDI events : %zu\n", mf.count);
    printf("  duration    : %.2f s\n", (double)last / (double)SYNTH_SAMPLE_RATE);
    printf("  gain        : %.2f\n", gain);
    printf("  peak        : %d / 32767\n", peak);
    printf("  voices left : %d\n", synth_active_voices());

    free(buf);
    midi_file_free(&mf);
    return 0;
}
