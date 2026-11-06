#ifndef WAV_H
#define WAV_H

#include <stdint.h>

int wav_write_i16(const char *path, const int16_t *data, uint32_t frames,
                  uint32_t sample_rate);

#endif
