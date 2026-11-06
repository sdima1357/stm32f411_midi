#include "synth_stm32.h"
#include "synth.h"

#include <string.h>

extern I2S_HandleTypeDef hi2s1;

/*
 * I2S is inherently stereo.  The synth is mono, so each sample is duplicated
 * into the left and right slots.  If your codec uses only one slot, this still
 * works; the unused slot is simply ignored.
 */
static int16_t dma_buf[2 * AUDIO_DMA_FRAMES * 2]; /* two halves, interleaved L/R */
static int16_t mono_buf[AUDIO_DMA_FRAMES];

static void fill_half(uint32_t half)
{
    int16_t *dst = &dma_buf[half * AUDIO_DMA_FRAMES * 2];
    uint32_t i;

    synth_render_i16(mono_buf, AUDIO_DMA_FRAMES);
    for (i = 0; i < AUDIO_DMA_FRAMES; i++) {
        dst[2 * i] = mono_buf[i];
        dst[2 * i + 1] = mono_buf[i];
    }
}

void audio_init(void)
{
    memset(dma_buf, 0, sizeof(dma_buf));
    fill_half(0);
    fill_half(1);
}

void audio_start(void)
{
    /*
     * Size is the number of 16-bit transfers: two halves, two per stereo frame.
     * Configure the DMA stream in circular mode.
     */
    HAL_I2S_Transmit_DMA(&hi2s1, (uint16_t *)dma_buf,
                         2 * AUDIO_DMA_FRAMES * 2);
}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI1) {
        fill_half(0);
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI1) {
        fill_half(1);
    }
}
