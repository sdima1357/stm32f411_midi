# STM32F411 MIDI synth

A 24-voice General MIDI synthesizer for the STM32F411CEU6, using the same
portable C engine as the PC renderer.

- 128 GM instruments: PCM, 2-operator FM and spectral wavetables.
- 24 voices, ADSR envelopes, voice stealing and the GM drum kit.
- PC tool renders MIDI files to 22050 Hz mono WAV for testing.
- STM32 firmware plays USB-MIDI input through I2S1.

## STM32 modes

The USB role is selected with `USB_MODE` in `Core/Inc/tusb_config.h`:

1. `USB_MODE_DEVICE` - the board appears as a USB-MIDI device. MIDI events
   received from the PC are synthesized and played on I2S1 (PA4/PA5/PA7).
2. `USB_MODE_HOST` - the board acts as a USB host for a MIDI keyboard. Events
   are decoded and printed over UART1; audio playback is not wired in this
   mode.

## Build and flash

```bash
make -j8
./flash.sh
```

The firmware image is `build/stm32f411_midi.hex`.

## PC renderer

```bash
cd pc
make -j8
./gmsynth song.mid song.wav
```

See `pc/README.md` for the soundbank generator and PC details, and
`pc/stm32/README.md` for the board integration notes.
