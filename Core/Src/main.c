/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <stdio.h>

//----- USB role switch (USB_MODE_HOST / USB_MODE_DEVICE) is set in
//----- Core/Inc/tusb_config.h, it applies to the whole TinyUSB stack.
#include "tusb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// USB role switch (USB_MODE) is defined in the Includes section above,
// it must appear before tusb.h is included.
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USB_OTG_FS_USB_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
//~ void retarget_put_char(char p)
//~ {
	//~ HAL_UART_Transmit(&huart1, (uint8_t*)&p, 1, 0xffffff); // send message via UART
//~ }
#define PTNF_SIZE 0x1000
int8_t circ_buff[PTNF_SIZE];
int c_write = 0;
int c_read  = 0;

uint8_t crsize()
{
	return (c_write+PTNF_SIZE-c_read)&(PTNF_SIZE-1);
}

int  crget(char* r)
{
	if(!crsize()) return 0;
	*r = circ_buff[c_read];
	c_read ++;
	c_read &=(PTNF_SIZE-1);
	return 1;

}

void cr_flush()
{
	char rr;
	while(crsize())
	{
		crget(&rr),HAL_UART_Transmit(&huart1, (uint8_t*)&rr, 1, 0xffffff); // send message via UART
	}
}

void crput(char r)
{
	if(crsize()<0xff)
	{
		circ_buff[c_write] = r;
		c_write++;
		c_write&=(PTNF_SIZE-1);
	}
}


int _write(int fd, char* ptr, int len)
{
    (void)fd;
    int i = 0;
    while (ptr[i] && (i < len))
    {
    	if (ptr[i] == '\r')
    	{

    	}
    	else
    	{
    		crput(ptr[i]);
			//retarget_put_char((int)ptr[i]);
			if (ptr[i] == '\n')
			{
				crput('\r');
				//retarget_put_char((int)'\r');
			}
    	}
        i++;
    }
    return len;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_OTG_FS_USB_Init();
  /* USER CODE BEGIN 2 */
  int k = 0;
  uint32_t blink_ms = 0;

#if (USB_MODE == USB_MODE_HOST)
  printf("STM32F411 TinyUSB MIDI Host\r\n");
#else
  printf("STM32F411 TinyUSB MIDI Device\r\n");
#endif
  cr_flush();

#if (USB_MODE == USB_MODE_HOST)
  tusb_rhport_init_t usb_init = {
    .role  = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(CFG_TUH_RHPORT, &usb_init);
#else
  tusb_rhport_init_t usb_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(CFG_TUD_RHPORT, &usb_init);
#endif
  printf("inited ok\n");
 cr_flush();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if (USB_MODE == USB_MODE_HOST)
    tuh_task();
#else
    tud_task();
#endif
    cr_flush();

    // non-blocking LED blink every 500ms
    if (tusb_time_millis_api() - blink_ms >= 500) {
      blink_ms += 500;
      k++;
      HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,(k&1)?GPIO_PIN_RESET:GPIO_PIN_SET);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_USB_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */
  // Enable USB OTG FS peripheral clock, TinyUSB does not enable it itself.
  // OTG_FS on STM32F4 is clocked from PLLQ, which must be 48MHz.
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA10 PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
//----- printf retarget over UART1 -----//
//~ int __io_putchar(int ch) {
  //~ HAL_UART_Transmit(&huart1, (uint8_t *) &ch, 1, HAL_MAX_DELAY);
  //~ return ch;
//~ }

//----- TinyUSB millisecond API (OPT_OS_NONE) -----//
uint32_t tusb_time_millis_api(void) {
  return HAL_GetTick();
}

//----- decode and print a stream of classic MIDI1 bytes -----//
static void midi_print_bytes(uint8_t cable, const uint8_t *data, uint32_t len) {
  static uint8_t running_status = 0;

  for (uint32_t i = 0; i < len; i++) {
    uint8_t b = data[i];

    if (b & 0x80) {
      // status byte (ignore system real-time for simplicity of running status)
      if (b < 0xF0) {
        running_status = b;
      } else {
        running_status = 0;
      }

      switch (b & 0xF0) {
        case 0x80: // Note Off
          printf("cable %u: Note OFF  ch%u note %u vel %u\r\n", cable, (b & 0x0F) + 1, data[i+1], data[i+2]);
          i += 2;
          break;
        case 0x90: // Note On
          if (data[i+2] == 0) {
            printf("cable %u: Note OFF  ch%u note %u (vel 0)\r\n", cable, (b & 0x0F) + 1, data[i+1]);
          } else {
            printf("cable %u: Note ON   ch%u note %u vel %u\r\n", cable, (b & 0x0F) + 1, data[i+1], data[i+2]);
          }
          i += 2;
          break;
        case 0xA0: // Polyphonic Key Pressure
          printf("cable %u: PolyPressure ch%u note %u val %u\r\n", cable, (b & 0x0F) + 1, data[i+1], data[i+2]);
          i += 2;
          break;
        case 0xB0: // Control Change
          printf("cable %u: CC  ch%u ctrl %u val %u\r\n", cable, (b & 0x0F) + 1, data[i+1], data[i+2]);
          i += 2;
          break;
        case 0xC0: // Program Change
          printf("cable %u: ProgramChange ch%u prog %u\r\n", cable, (b & 0x0F) + 1, data[i+1]);
          i += 1;
          break;
        case 0xD0: // Channel Pressure
          printf("cable %u: ChannelPressure ch%u val %u\r\n", cable, (b & 0x0F) + 1, data[i+1]);
          i += 1;
          break;
        case 0xE0: { // Pitch Bend
          uint16_t pb = (uint16_t) data[i+1] | ((uint16_t) data[i+2] << 7);
          printf("cable %u: PitchBend ch%u val %d\r\n", cable, (b & 0x0F) + 1, (int) pb - 8192);
          i += 2;
          break;
        }
        case 0xF0:
          switch (b) {
            case 0xF0: { // SysEx
              uint32_t j = i + 1;
              printf("cable %u: SysEx:", cable);
              while (j < len && data[j] < 0xF7) {
                printf(" %02X", data[j]);
                j++;
              }
              if (j < len) {
                printf(" F7");
              }
              printf("\r\n");
              i = j;
              break;
            }
            case 0xF1: // MTC Quarter Frame (1 data byte)
              printf("cable %u: MTC quarter frame %02X\r\n", cable, data[i+1]);
              i += 1;
              break;
            case 0xF2: { // Song Position (2 data bytes)
              uint16_t sp = (uint16_t) data[i+1] | ((uint16_t) data[i+2] << 7);
              printf("cable %u: SongPosition %u\r\n", cable, sp);
              i += 2;
              break;
            }
            case 0xF3: // Song Select (1 data byte)
              printf("cable %u: SongSelect %u\r\n", cable, data[i+1]);
              i += 1;
              break;
            case 0xF6: // Tune Request
              printf("cable %u: TuneRequest\r\n", cable);
              break;
            default:   // 0xF7 EOX, 0xF8..0xFF real-time
              printf("cable %u: RT/EOX %02X\r\n", cable, b);
              break;
          }
          break;
        default:
          break;
      }
    } else {
      // data byte without status: use running status
      if (running_status == 0) {
        printf("cable %u: orphan data %02X\r\n", cable, b);
        continue;
      }
      // reprocess as if preceded by its status byte
      uint8_t with_status[3] = { running_status, b, 0 };
      if ((running_status & 0xF0) != 0xC0 && (running_status & 0xF0) != 0xD0 && (running_status & 0xF0) < 0xF0) {
        with_status[2] = data[i+1];
      }
      uint32_t start = i;
      // consume the data byte(s) belonging to this message
      if ((running_status & 0xF0) == 0xC0 || (running_status & 0xF0) == 0xD0) {
        i += 1;
      } else {
        i += 2;
      }
      (void) start;
      midi_print_bytes(cable, with_status, 3);
    }
  }
}

#if (USB_MODE == USB_MODE_HOST)
//----- TinyUSB Host callbacks -----//
void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_cb_data) {
  printf("MIDI device mounted: idx %u, daddr %u, RX cables %u, TX cables %u\r\n",
         idx, mount_cb_data->daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);
}

void tuh_midi_umount_cb(uint8_t idx) {
  printf("MIDI device unmounted: idx %u\r\n", idx);
}

void tuh_midi_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
  (void) xferred_bytes;
  uint8_t buffer[64];
  uint8_t cable_num = 0;
  uint32_t bytes_read;

  // drain the FIFO, a single read stops at a cable change or when the buffer is full
  while ((bytes_read = tuh_midi_stream_read(idx, &cable_num, buffer, sizeof(buffer))) > 0) {
    midi_print_bytes(cable_num, buffer, bytes_read);
  }
}

#else /* USB_MODE == USB_MODE_DEVICE */
//----- TinyUSB Device: MIDI instrument descriptors -----//
enum {
  ITF_NUM_MIDI = 0,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_TOTAL
};

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL
};

#define EPNUM_MIDI_OUT  0x01
#define EPNUM_MIDI_IN   0x81

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

static uint8_t const desc_fs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, (0x80 | EPNUM_MIDI_IN), 64)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_fs_configuration;
}

static char const *string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 },   // 0: supported language is English (0x0409)
  "STMicroelectronics",            // 1: Manufacturer
  "STM32 MIDI",                    // 2: Product
  "000001",                        // 3: Serial
};

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  static uint16_t desc_str[32];
  uint8_t chr_count;

  switch (index) {
    case 0:
      memcpy(&desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;
    default:
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
        return NULL;
      }
      char const *str = string_desc_arr[index];
      chr_count = (uint8_t) strlen(str);
      if (chr_count > 31) chr_count = 31;
      for (uint8_t i = 0; i < chr_count; i++) {
        desc_str[1 + i] = str[i];
      }
      break;
  }

  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return desc_str;
}

void tud_mount_cb(void) {
  printf("USB mounted\r\n");
}

void tud_umount_cb(void) {
  printf("USB unmounted\r\n");
}

void tud_midi_rx_cb(uint8_t itf) {
  (void) itf;
  uint8_t buffer[64];
  uint32_t bytes_read;

  // read everything available and print it, then echo back to the host
  while ((bytes_read = tud_midi_stream_read(buffer, sizeof(buffer))) > 0) {
    midi_print_bytes(0, buffer, bytes_read);
   // tud_midi_stream_write(0, buffer, bytes_read);
  }
}
#endif /* USB_MODE */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
