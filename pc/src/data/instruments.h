#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H
#include <stdint.h>

#define GM_NUM_PROGRAMS 128
#define GM_NUM_DRUMS 47
#define GM_DRUM_FIRST 35
#define PCM_SAMPLE_RATE 11025
#define WT_FRAME_LEN 128

/* method: 0=PCM, 1=FM, 2=WT */
typedef struct {
    uint8_t  method;
    uint8_t  root;
    uint8_t  frames;
    uint8_t  flags;       /* bit0 PCM ignore note-off, bit1 PCM no pitch, bit2 WT 2 mips */
    uint16_t frame_len;
    uint32_t data_off;    /* WT: sample index, PCM: byte index */
    uint32_t data_len;    /* WT: samples, PCM: bytes */
    uint16_t attack_ms;
    uint16_t decay_ms;
    uint16_t release_ms;
    uint8_t  sustain;     /* 0..255 */
    uint8_t  gain;        /* 0..255 */
    uint8_t  fm_ratio;    /* ratio * 16 */
    uint8_t  fm_index;    /* index * 16 */
    uint8_t  fm_feedback; /* 0..255 */
    uint8_t  reserved;
} Instrument;

extern const Instrument gm_instruments[GM_NUM_PROGRAMS];
extern const Instrument gm_drums[GM_NUM_DRUMS];
extern const int16_t wt_pool[];
extern const uint32_t wt_pool_len;
extern const int8_t pcm_pool[];
extern const uint32_t pcm_pool_len;
#endif
