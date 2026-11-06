#include "synth_stm32.h"
#include "synth.h"

/*
 * USB-MIDI event packets are 4 bytes:
 *   byte 0: cable number (high nibble) | code index number (low nibble)
 *   byte 1: MIDI status
 *   byte 2: data 1
 *   byte 3: data 2
 *
 * Call usb_midi_receive() from the MIDI class receive callback, e.g. in
 * usbd_midi_if.c:
 *
 *   static int8_t MIDI_Receive(uint8_t *pbuf, uint32_t len)
 *   {
 *       usb_midi_receive(pbuf, len);
 *       return 0;
 *   }
 */
void usb_midi_receive(const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    for (i = 0; i + 3 < len; i += 4) {
        uint8_t status = buf[i + 1];
        uint8_t d1 = buf[i + 2];
        uint8_t d2 = buf[i + 3];
        uint8_t ch = (uint8_t)(status & 0x0F);

        switch (status & 0xF0) {
        case 0x80:
            synth_note_off(ch, d1);
            break;
        case 0x90:
            if (d2 != 0) {
                synth_note_on(ch, d1, d2);
            } else {
                synth_note_off(ch, d1);
            }
            break;
        case 0xB0:
            synth_control_change(ch, d1, d2);
            break;
        case 0xC0:
            synth_program_change(ch, d1);
            break;
        case 0xE0: {
            int bend = ((int)d2 << 7 | (int)d1) - 8192;
            synth_pitch_bend(ch, (int16_t)bend);
            break;
        }
        default:
            break;
        }
    }
}
