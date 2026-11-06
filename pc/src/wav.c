#include "wav.h"

#include <stdio.h>
#include <string.h>

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

int wav_write_i16(const char *path, const int16_t *data, uint32_t frames,
                  uint32_t sample_rate)
{
    FILE *f;
    uint8_t hdr[44];
    uint32_t data_bytes = frames * 2u;
    uint32_t i;

    f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    memcpy(hdr + 0, "RIFF", 4);
    put_u32le(hdr + 4, 36u + data_bytes);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    put_u32le(hdr + 16, 16u);
    put_u16le(hdr + 20, 1u);              /* PCM */
    put_u16le(hdr + 22, 1u);              /* mono */
    put_u32le(hdr + 24, sample_rate);
    put_u32le(hdr + 28, sample_rate * 2u); /* byte rate */
    put_u16le(hdr + 32, 2u);              /* block align */
    put_u16le(hdr + 34, 16u);             /* bits */
    memcpy(hdr + 36, "data", 4);
    put_u32le(hdr + 40, data_bytes);

    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return -1;
    }

    for (i = 0; i < frames; i++) {
        uint8_t b[2];
        put_u16le(b, (uint16_t)data[i]);
        if (fwrite(b, 1, 2, f) != 2) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}
