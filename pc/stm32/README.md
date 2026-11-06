# STM32F411CEU6 integration

This folder contains the board-side glue for the portable synth in `../src`.
The core engine is platform independent; only the I2S DMA and USB-MIDI
plumbing live here.

## Resource budget

| Item | Size |
|---|---|
| Soundbank data (`../src/data/*.c`) | ~385 KiB |
| Synth engine + MIDI parser | ~20 KiB flash |
| HAL + USB device + startup | ~25-35 KiB flash |
| **Total flash** | **~426 KiB / 512 KiB** |
| 24 voices + DMA + USB buffers | ~17 KiB RAM / 128 KiB |

The large tables are `const`, so they stay in flash. Do **not** copy them to
RAM.

## Audio format

- Mono synthesis, 22050 Hz, signed 16-bit.
- I2S is stereo by nature, so `audio_i2s.c` duplicates each mono sample into
  the left and right slots. If your codec uses only one slot, that is fine.
- DMA buffer: 256 stereo frames per half (about 11.6 ms latency per callback).

## CubeMX setup

Create a project for **STM32F411CEU6** and configure:

### Clock (25 MHz HSE)

- HSE: crystal/ceramic resonator, 25 MHz (Black Pill boards usually have 25 MHz;
  use 8 MHz and adjust `PLLM`/`PLLI2SM` if yours differs).
- PLL: `M=25`, `N=192`, `P=2`, `Q=4` -> SYSCLK 96 MHz, USB 48 MHz.
- PLLI2S: `M=16`, `N=192`, `R=2` -> I2S clock 150 MHz.
- AHB /1, APB1 /2 (48 MHz), APB2 /1 (96 MHz), flash latency 3 WS.

### I2S1 (master transmit)

- Mode: Master TX, Philips standard, 16-bit, 22050 Hz.
- Pins:
  - `PA4` -> I2S1_WS
  - `PA5` -> I2S1_CK
  - `PA7` -> I2S1_SD
- DMA: `DMA2 Stream2`, channel 2, **circular**, half-word, memory increment,
  peripheral no increment, low priority.
- Enable the `DMA2_Stream2_IRQn` interrupt; the half/full callbacks are driven
  by the DMA interrupt.

With the PLLI2S values above, CubeMX reports an actual audio frequency of
about 22.007 kHz. If your board uses a different HSE, let CubeMX recalculate it
and verify the audio frequency.

### USB

- USB_OTG_FS, pins `PA11` (DM) and `PA12` (DP); `PA10` (ID) is also configured.
- The USB stack is **TinyUSB**, not the STM32 USB Device middleware. The role is
  selected with `USB_MODE` in `Core/Inc/tusb_config.h`:
  `USB_MODE_HOST` for a MIDI keyboard, `USB_MODE_DEVICE` to appear as a MIDI
  instrument to a PC.
- In device mode, `tud_midi_rx_cb()` in `Core/Src/main.c` reads 4-byte USB-MIDI
  event packets with `tud_midi_packet_read()` and forwards them to
  `usb_midi_receive()`.
- In host mode, `tuh_midi_rx_cb()` reads 4-byte USB-MIDI event packets with
  `tuh_midi_packet_read()`, forwards them to `usb_midi_receive()`, and prints
  them over UART1.
- Host mode needs a USB OTG adapter and external/powered-hub 5 V for the
  keyboard; the board's USB connector is normally wired for device mode.

## Adding the code

The repository Makefile already builds these files:

- `../src/synth.c`, `../src/synth.h`
- `../src/data/instruments.c`, `../src/data/instruments.h`
- `../src/data/wavetables.c`, `../src/data/pcm.c`
- `audio_i2s.c`
- `usb_midi.c`

Add `../src` and `../src/data` to the compiler include paths, and link with
`-lm` (the engine uses `sinf`, `expf`, `powf`).

From the generated `main.c`, after `MX_I2S1_Init()` and `tusb_init()`, call:

```c
synth_init();
audio_init();
audio_start();
```

`app_main.c` is not used by this project; it targets the STM32 USB Device
middleware and calls `MX_USB_DEVICE_Init()`, which does not exist here.

## Notes

- The synth uses single-precision floats. The Cortex-M4F FPU handles this well;
  the audio callback uses only table lookups and adds/multiplies.
- 24 voices at 22050 Hz fits a 96-100 MHz F411: the WT/FM path costs roughly
  150 cycles per voice per sample, so 24 voices use about 80% of a 100 MHz
  core.  The wavetable frame morph is O(1) per sample, and sustained notes read
  a single wavetable frame.  The audio callback must stay well under the
  11.6 ms DMA half-buffer period.
- `powf`/`expf` are called only on note-on and pitch-bend, never per sample.
- Voice stealing is automatic: 24 voices, released/quietest voices are stolen
  first, then the oldest.  Drums are capped at 8 voices so they cannot starve
  the melodic voices.
- Channel 10 (MIDI channel 9) uses the standard GM drum kit.
- Program changes on channel 10 are ignored (standard kit is always used).
- Sustain pedal (CC64), volume (CC7), expression (CC11), all-notes-off (CC123)
  and reset-controllers (CC121) are supported.
- Pitch bend range is +/- 2 semitones.
- Both USB device and host modes feed received MIDI into the synth and play it
  on I2S1. Host mode additionally prints keyboard events over UART1.

## Regenerating the soundbank

On the PC:

```bash
python3 tools/build_soundbank.py          # render + analyse + emit C tables
python3 tools/refit_fm.py                 # optional FM/envelope tuning pass
make -j8                                  # build PC renderer
./gmsynth build/demo.mid build/demo.wav   # test render
```

The generator caches fluidsynth renders in `build/render/`, so re-runs are
fast unless you pass `--force`.
