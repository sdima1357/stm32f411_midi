#ifndef SYNTH_STM32_H
#define SYNTH_STM32_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_DMA_FRAMES 256

void audio_init(void);
void audio_start(void);

/* Feed a USB-MIDI event packet buffer (4-byte packets). */
void usb_midi_receive(const uint8_t *buf, uint32_t len);

/* Application entry: call after CubeMX peripheral init. */
void app_main(void);

#endif
