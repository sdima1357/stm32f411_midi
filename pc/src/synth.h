#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>
#include "data/instruments.h"

#define SYNTH_SAMPLE_RATE 22050
#define SYNTH_MAX_VOICES 24

#ifdef __cplusplus
extern "C" {
#endif

void synth_init(void);
void synth_reset(void);
void synth_set_master_volume(float volume);

void synth_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void synth_note_off(uint8_t channel, uint8_t note);
void synth_program_change(uint8_t channel, uint8_t program);
void synth_control_change(uint8_t channel, uint8_t cc, uint8_t value);
void synth_pitch_bend(uint8_t channel, int16_t bend);
void synth_all_notes_off(uint8_t channel);

/* Render signed 16-bit mono samples. */
void synth_render_i16(int16_t *out, uint32_t frames);

int synth_active_voices(void);

#ifdef __cplusplus
}
#endif

#endif
