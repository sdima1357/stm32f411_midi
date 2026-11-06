#include "synth_stm32.h"
#include "synth.h"
#include "usb_device.h"

/*
 * Call this from the CubeMX-generated main() after the clock, GPIO, DMA and
 * I2S2 initialisation.  If your main() already calls MX_USB_DEVICE_Init(),
 * remove the call below.
 */
void app_main(void)
{
    synth_init();
    audio_init();
    MX_USB_DEVICE_Init();
    audio_start();

    for (;;) {
        __WFI();
    }
}
