/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : INA219 V/I monitor on SSD1306 (I2C1, PB8/PB9) +
  *                   KY-003 Hall sensor RPM on SSD1306 (I2C2, PA9/PA10) +
  *                   One-shot 3 s relay pulse on PA0 at 15 V crossing.
  *                   Target: NUCLEO-F303ZE
  *
  *  Pinout summary
  *  --------------
  *  PB8  = I2C1_SCL  -> Display 1 (V/I)
  *  PB9  = I2C1_SDA  -> Display 1 (V/I)
  *  PA9  = I2C2_SCL  -> Display 2 (RPM)
  *  PA10 = I2C2_SDA  -> Display 2 (RPM)
  *  PB1  = EXTI1     -> KY-003 Hall sensor S
  *  PA0  = GPIO out  -> Songle relay control
  *
  *  Note: ssd1306_b.c v2 fails gracefully if display 2 is absent or its
  *  bus is unresponsive — the first display will keep working either way.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "ssd1306.h"        /* Display 1 - uses hi2c1 internally          */
#include "ssd1306_b.h"      /* Display 2 - independent driver, hi2c2      */
#include "ssd1306_fonts.h"  /* Shared between both displays               */
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef  hi2c1;
I2C_HandleTypeDef  hi2c2;
TIM_HandleTypeDef  htim2;
RTC_HandleTypeDef  hrtc;
UART_HandleTypeDef huart3;
PCD_HandleTypeDef  hpcd_USB_FS;

/* USER CODE BEGIN PV */

/* -- INA219 ------------------------------------------------------------ */
#define INA219_I2C_ADDR        (0x40 << 1)
#define INA219_REG_CONFIG      0x00
#define INA219_REG_SHUNTVOLT   0x01
#define INA219_REG_BUSVOLT     0x02
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIBRATION 0x05

#define INA219_CONFIG_VALUE    0x3FFF      /* 32V FSR, +/-320mV shunt, 12-bit */
#define INA219_CALIBRATION     4096        /* 0.1 ohm shunt, 3.2 A max        */
#define INA219_CURRENT_LSB_mA  0.1f
#define VOLTAGE_OFFSET_V       1.6f

/* -- Relay (one-shot, latches off until reset) ------------------------- */
#define RELAY_TRIP_VOLTAGE_V       15.0f
#define RELAY_PULSE_DURATION_MS    3000U

/* -- Hall sensor / fan RPM --------------------------------------------- */
/* 1 magnet glued to fan hub  ->  MAGNETS_PER_REVOLUTION = 1.
 * Using a 3-pin/4-pin PC fan tach instead of the KY-003? Set it to 2.   */
#define MAGNETS_PER_REVOLUTION     1U

volatile uint32_t hall_pulse_count = 0;    /* incremented in EXTI ISR     */
volatile uint32_t fan_rpm          = 0;    /* updated once per second     */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef INA219_WriteReg(uint8_t reg, uint16_t value);
static HAL_StatusTypeDef INA219_ReadReg (uint8_t reg, uint16_t *value);
static HAL_StatusTypeDef INA219_Init(void);
static HAL_StatusTypeDef INA219_ReadBusVoltage(float *voltage_V);
static HAL_StatusTypeDef INA219_ReadCurrent  (float *current_A);
/* USER CODE END PFP */


/* USER CODE BEGIN 0 */

/* -- Hall sensor ISR (PB1, falling edge) ---------------------------------
 * Keep this MINIMAL. No printf, no I2C, no floating-point.
 * Magnet pole passes sensor -> KY-003 output falls -> this fires.        */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_1)
    {
        hall_pulse_count++;
    }
}

/* -- 1 Hz timer ISR -- snapshot pulse count, compute RPM, reset -------- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        uint32_t pulses   = hall_pulse_count;
        hall_pulse_count  = 0;
        fan_rpm = (pulses * 60U) / MAGNETS_PER_REVOLUTION;
    }
}

/* -- INA219 driver (always uses hi2c1) --------------------------------- */
static HAL_StatusTypeDef INA219_WriteReg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = { reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    return HAL_I2C_Master_Transmit(&hi2c1, INA219_I2C_ADDR, buf, 3, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef INA219_ReadReg(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef s = HAL_I2C_Master_Transmit(&hi2c1, INA219_I2C_ADDR,
                                                  &reg, 1, HAL_MAX_DELAY);
    if (s != HAL_OK) return s;
    s = HAL_I2C_Master_Receive(&hi2c1, INA219_I2C_ADDR, buf, 2, HAL_MAX_DELAY);
    if (s != HAL_OK) return s;
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return HAL_OK;
}

static HAL_StatusTypeDef INA219_Init(void)
{
    HAL_StatusTypeDef s = INA219_WriteReg(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
    if (s != HAL_OK) return s;
    HAL_Delay(5);
    s = INA219_WriteReg(INA219_REG_CALIBRATION, INA219_CALIBRATION);
    HAL_Delay(5);
    return s;
}

static HAL_StatusTypeDef INA219_ReadBusVoltage(float *voltage_V)
{
    uint16_t raw;
    HAL_StatusTypeDef s = INA219_ReadReg(INA219_REG_BUSVOLT, &raw);
    if (s != HAL_OK) return s;
    *voltage_V = (float)(raw >> 3) * 0.004f;
    return HAL_OK;
}

static HAL_StatusTypeDef INA219_ReadCurrent(float *current_A)
{
    uint16_t raw;
    HAL_StatusTypeDef s = INA219_ReadReg(INA219_REG_CURRENT, &raw);
    if (s != HAL_OK) return s;
    int16_t signed_raw = (int16_t)raw;
    *current_A = (float)signed_raw * (INA219_CURRENT_LSB_mA / 1000.0f);
    return HAL_OK;
}
/* USER CODE END 0 */


int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_RTC_Init();
    MX_USART3_UART_Init();
    MX_USB_PCD_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();    /* I2C2 on PA9 (SCL) / PA10 (SDA) */
    MX_TIM2_Init();

    /* USER CODE BEGIN 2 */

    /* Display 1 (V/I) on I2C1 - PB8/PB9 */
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    /* Display 2 (RPM) on I2C2 - PA9/PA10
     * If the second display isn't responding, ssd1306b_Init returns
     * silently and all subsequent ssd1306b_* calls become no-ops.
     * Display 1 keeps working regardless.                              */
    ssd1306b_Init();
    ssd1306b_Fill(Black);
    ssd1306b_UpdateScreen();

    if (INA219_Init() != HAL_OK)
    {
        ssd1306_SetCursor(0, 10);
        ssd1306_WriteString("INA219", Font_11x18, White);
        ssd1306_SetCursor(0, 35);
        ssd1306_WriteString("NOT FOUND", Font_11x18, White);
        ssd1306_UpdateScreen();
        while (1) {}
    }

    /* Start the 1 Hz RPM-window timer (must come AFTER MX_TIM2_Init) */
    HAL_TIM_Base_Start_IT(&htim2);

    uint8_t  relay_state    = 0;
    uint32_t relay_start_ms = 0;
    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1)
    {
        float voltage = 0.0f, current = 0.0f;
        char buf[32];

        /* Defensive re-calibrate every loop (survives I2C glitches) */
        INA219_WriteReg(INA219_REG_CALIBRATION, INA219_CALIBRATION);
        HAL_Delay(2);

        INA219_ReadBusVoltage(&voltage);
        INA219_ReadCurrent(&current);

        voltage -= VOLTAGE_OFFSET_V;
        if (voltage < 0.0f) voltage = 0.0f;

        uint16_t shunt_raw = 0;
        INA219_ReadReg(INA219_REG_SHUNTVOLT, &shunt_raw);
        int16_t shunt_signed = (int16_t)shunt_raw;

        /* -- One-shot relay pulse on first 15V crossing --------------- */
        if (relay_state == 0 && voltage >= RELAY_TRIP_VOLTAGE_V)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
            relay_start_ms = HAL_GetTick();
            relay_state    = 1;
        }
        else if (relay_state == 1 &&
                 (HAL_GetTick() - relay_start_ms >= RELAY_PULSE_DURATION_MS))
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
            relay_state = 2;
        }

        /* -- Display 1 (I2C1, PB8/PB9): V / I / shunt / relay state --- */
        ssd1306_Fill(Black);

        sprintf(buf, "V:%.2fV", voltage);
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(buf, Font_11x18, White);

        sprintf(buf, "I:%.3fA", current);
        ssd1306_SetCursor(0, 22);
        ssd1306_WriteString(buf, Font_11x18, White);

        sprintf(buf, "SH:%d", shunt_signed);
        ssd1306_SetCursor(0, 44);
        ssd1306_WriteString(buf, Font_7x10, White);

        const char *state_str = (relay_state == 0) ? "WAIT" :
                                (relay_state == 1) ? "ON  " : "DONE";
        sprintf(buf, "R:%s", state_str);
        ssd1306_SetCursor(75, 44);
        ssd1306_WriteString(buf, Font_7x10, White);

        ssd1306_UpdateScreen();

        /* -- Display 2 (I2C2, PA9/PA10): Fan RPM ---------------------- *
         * If display 2 is absent, these calls are no-ops at the I2C
         * level (buffer is still updated but nothing is sent).         */
        uint32_t rpm_snapshot = fan_rpm;   /* snapshot the volatile */

        ssd1306b_Fill(Black);

        ssd1306b_SetCursor(0, 0);
        ssd1306b_WriteString("Fan RPM", Font_11x18, White);

        sprintf(buf, "%lu", rpm_snapshot);
        ssd1306b_SetCursor(0, 26);
        ssd1306b_WriteString(buf, Font_16x26, White);

        ssd1306b_UpdateScreen();

        HAL_Delay(500);
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef       RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit     = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI
                                     | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState  = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL    = RCC_PLL_MUL9;
    RCC_OscInitStruct.PLL.PREDIV    = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB | RCC_PERIPHCLK_USART3
                                       | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_I2C2
                                       | RCC_PERIPHCLK_RTC;
    PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    PeriphClkInit.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_HSI;
    PeriphClkInit.I2c2ClockSelection   = RCC_I2C2CLKSOURCE_HSI;
    PeriphClkInit.RTCClockSelection    = RCC_RTCCLKSOURCE_LSI;
    PeriphClkInit.USBClockSelection    = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance              = I2C1;
    hi2c1.Init.Timing           = 0x00201D2B;
    hi2c1.Init.OwnAddress1      = 0;
    hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2      = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter (&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) Error_Handler();
}

static void MX_I2C2_Init(void)
{
    /* Pins (set in CubeMX, muxed in HAL_I2C_MspInit):
     *   PA9  = I2C2_SCL  (AF4)
     *   PA10 = I2C2_SDA  (AF4)                                          */
    hi2c2.Instance              = I2C2;
    hi2c2.Init.Timing           = 0x00201D2B;       /* 100 kHz @ HSI */
    hi2c2.Init.OwnAddress1      = 0;
    hi2c2.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2      = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter (&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 7199;     /* 72 MHz / 7200  = 10 kHz   */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 9999;     /* 10 kHz / 10000 =  1 Hz    */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_RTC_Init(void)
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance                    = USART3;
    huart3.Init.BaudRate               = 38400;
    huart3.Init.WordLength             = UART_WORDLENGTH_8B;
    huart3.Init.StopBits               = UART_STOPBITS_1;
    huart3.Init.Parity                 = UART_PARITY_NONE;
    huart3.Init.Mode                   = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

static void MX_USB_PCD_Init(void)
{
    hpcd_USB_FS.Instance                     = USB;
    hpcd_USB_FS.Init.dev_endpoints           = 8;
    hpcd_USB_FS.Init.speed                   = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.phy_itface              = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.low_power_enable        = DISABLE;
    hpcd_USB_FS.Init.lpm_enable              = DISABLE;
    hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
    if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, LD1_Pin | LD3_Pin | LD2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

    /* PA0 starts low BEFORE configuring as output (no boot-time relay glitch) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

    /* User button (existing) */
    GPIO_InitStruct.Pin  = USER_Btn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

    /* Onboard LEDs */
    GPIO_InitStruct.Pin   = LD1_Pin | LD3_Pin | LD2_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USB power switch / overcurrent */
    GPIO_InitStruct.Pin   = USB_PowerSwitchOn_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = USB_OverCurrent_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

    /* Relay control on PA0 */
    GPIO_InitStruct.Pin   = GPIO_PIN_0;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* -- Hall sensor input on PB1 (falling-edge interrupt) ----------- */
    GPIO_InitStruct.Pin  = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;          /* KY-003 has its own PU */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
