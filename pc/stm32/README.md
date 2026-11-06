# STM32F411CEU6 integration

This folder contains the board-side glue for the portable synth in `../src`.
The core engine is platform independent; only the I2S DMA and USB-MIDI
plumbing live here.

## Resource budget

| Item | Size |
|---|---|
| Soundbank data (`src/data/*.c`) | ~385 KiB |
| Synth engine + MIDI parser | ~20 KiB flash |
| HAL + USB device + startup | ~25-35 KiB flash |
| **Total flash** | **~430-440 KiB / 512 KiB** |
| 24 voices + DMA + USB buffers | < 12 KiB RAM / 128 KiB |

The large tables are `const`, so they stay in flash. Do **not** copy them to
RAM.

## Audio format

- Mono synthesis, 22050 Hz, signed 16-bit.
- I2S is stereo by nature, so `audio_i2s.c` duplicates each mono sample into
  the left and right slots. If your codec uses only one slot, that is fine.
- DMA buffer: 256 stereo frames per half (about 11.6 ms latency per callback).

## CubeMX setup

Create a project for **STM32F411CEU6** and configure:

### Clock (example: 25 MHz HSE)

- HSE: crystal/ceramic resonator, 25 MHz (Black Pill boards usually have 25 MHz;
  use 8 MHz and adjust `PLLM`/`PLLI2SM` if yours differs).
- PLL: `M=25`, `N=384`, `P=4`, `Q=8` -> SYSCLK 96 MHz, USB 48 MHz.
- PLLI2S: `M=25`, `N=271`, `R=6` -> I2S clock ~45.17 MHz.
- AHB /1, APB1 /2 (48 MHz), APB2 /1 (96 MHz), flash latency 3 WS.

### I2S2 (master transmit)

- Mode: Master TX, Philips standard, 16-bit, 22050 Hz.
- Pins:
  - `PB12` -> I2S2_WS
  - `PB13` -> I2S2_CK
  - `PB15` -> I2S2_SD
  - `PC6`  -> I2S2_MCK (only if your DAC/codec needs MCLK)
- DMA: `DMA1 Stream4`, channel 0, **circular**, half-word, memory increment,
  peripheral no increment, high priority.
- Enable the I2S2 global interrupt (needed for the half/full callbacks).

With the PLLI2S values above, CubeMX selects the /64 prescaler for 22050 Hz.
If your board uses a different HSE, let CubeMX recalculate it and verify the
audio frequency.

### USB

- USB_OTG_FS in **Device** mode.
- Middleware: **USB Device** with the **MIDI** class.
- Pins `PA11` (DM) and `PA12` (DP).
- In `usbd_midi_if.c`, forward received packets:

```c
static int8_t MIDI_Receive(uint8_t *pbuf, uint32_t len)
{
    usb_midi_receive(pbuf, len);
    return 0;
}
```

## Adding the code

Add these files to the CubeMX project:

- `src/synth.c`, `src/synth.h`
- `src/data/instruments.c`, `src/data/instruments.h`
- `src/data/wavetables.c`, `src/data/pcm.c`
- `stm32/audio_i2s.c`
- `stm32/usb_midi.c`
- `stm32/app_main.c`

Add `src/` and `src/data/` to the compiler include paths, and link with `-lm`
(the engine uses `sinf`, `expf`, `powf`).

From the generated `main.c`, after `MX_GPIO_Init()`, `MX_DMA_Init()`,
`MX_I2S2_Init()` and `MX_USB_DEVICE_Init()`, call:

```c
app_main();
```

If `main.c` already calls `MX_USB_DEVICE_Init()`, remove the duplicate call
from `app_main.c`.

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
