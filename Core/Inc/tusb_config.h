#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

//----- USB ROLE SWITCH -----------------------------------------------------
// USB_MODE_HOST   = act as USB host for a MIDI keyboard and play it on I2S1
// USB_MODE_DEVICE = appear as a USB MIDI instrument and play PC MIDI on I2S1
// Change the value below to switch roles.
//-----------------------------------------------------------------------------
#ifndef USB_MODE
#define USB_MODE            USB_MODE_HOST
#endif

#ifndef USB_MODE_HOST
#define USB_MODE_HOST       0
#endif

#ifndef USB_MODE_DEVICE
#define USB_MODE_DEVICE     1
#endif

#define CFG_TUSB_OS           OPT_OS_NONE

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#ifndef CFG_TUD_MEM_SECTION
#define CFG_TUD_MEM_SECTION
#endif

#ifndef CFG_TUD_MEM_ALIGN
#define CFG_TUD_MEM_ALIGN     __attribute__ ((aligned(4)))
#endif

#ifndef CFG_TUH_MEM_SECTION
#define CFG_TUH_MEM_SECTION
#endif

#ifndef CFG_TUH_MEM_ALIGN
#define CFG_TUH_MEM_ALIGN     __attribute__ ((aligned(4)))
#endif

#if (USB_MODE == USB_MODE_HOST)

//--------------------------------------------------------------------
// Host Configuration
//--------------------------------------------------------------------

#define CFG_TUH_ENABLED       1
#define CFG_TUD_ENABLED       0

// Roothub port: 0 = OTG_FS on STM32F4
#define CFG_TUH_RHPORT        0

// OTG_FS on STM32F411 is full-speed only
#define CFG_TUH_MAX_SPEED     OPT_MODE_FULL_SPEED

// Size of buffer to hold descriptors and other data used for enumeration
#define CFG_TUH_ENUMERATION_BUFSIZE 256

#define CFG_TUH_HUB                 1
// max device support (excluding hub device)
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)

#define CFG_TUH_MIDI                CFG_TUH_DEVICE_MAX

#else /* USB_MODE == USB_MODE_DEVICE */

//--------------------------------------------------------------------
// Device Configuration
//--------------------------------------------------------------------

#define CFG_TUH_ENABLED       0
#define CFG_TUD_ENABLED       1

// Roothub port: 0 = OTG_FS on STM32F4
#define CFG_TUD_RHPORT        0

// OTG_FS on STM32F411 is full-speed only
#define CFG_TUD_MAX_SPEED     OPT_MODE_FULL_SPEED

// Control endpoint size
#define CFG_TUD_ENDPOINT0_SIZE    64

// number of MIDI streaming interfaces
#define CFG_TUD_MIDI              1

// MIDI bulk endpoint buffer size
#define CFG_TUD_MIDI_EP_BUFSIZE   64

// MIDI stream FIFO sizes
#define CFG_TUD_MIDI_RX_BUFSIZE   64
#define CFG_TUD_MIDI_TX_BUFSIZE   64

#endif

#ifdef __cplusplus
}
#endif

#endif
