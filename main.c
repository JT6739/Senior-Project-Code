/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : INA219 Voltage/Current monitor on SSD1306 OLED
  *                   Target: NUCLEO-F303ZE
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart3;
PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */

#define INA219_I2C_ADDR        (0x40 << 1)
#define INA219_REG_CONFIG      0x00
#define INA219_REG_SHUNTVOLT   0x01
#define INA219_REG_BUSVOLT     0x02
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIBRATION 0x05

#define INA219_CONFIG_VALUE    0x399F

/* Shunt = 0.188Ω (measured), Max current = 3.2A
 * Current LSB = 0.1mA
 * CAL = trunc(0.04096 / (0.0001 × 0.188)) = 2178  */
#define INA219_CALIBRATION     2178
#define INA219_CURRENT_LSB_mA  0.1f

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef INA219_WriteReg(uint8_t reg, uint16_t value);
static HAL_StatusTypeDef INA219_ReadReg(uint8_t reg, uint16_t *value);
static HAL_StatusTypeDef INA219_Init(void);
static HAL_StatusTypeDef INA219_ReadBusVoltage(float *voltage_V);
static HAL_StatusTypeDef INA219_ReadCurrent(float *current_A);
/* USER CODE END PFP */


/* USER CODE BEGIN 0 */

static HAL_StatusTypeDef INA219_WriteReg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value & 0xFF);
    return HAL_I2C_Master_Transmit(&hi2c1, INA219_I2C_ADDR, buf, 3, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef INA219_ReadReg(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef status;
    status = HAL_I2C_Master_Transmit(&hi2c1, INA219_I2C_ADDR, &reg, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;
    status = HAL_I2C_Master_Receive(&hi2c1, INA219_I2C_ADDR, buf, 2, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return HAL_OK;
}

static HAL_StatusTypeDef INA219_Init(void)
{
    HAL_StatusTypeDef status;
    status = INA219_WriteReg(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
    if (status != HAL_OK) return status;
    HAL_Delay(10);
    status = INA219_WriteReg(INA219_REG_CALIBRATION, INA219_CALIBRATION);
    HAL_Delay(100);
    return status;
}

static HAL_StatusTypeDef INA219_ReadBusVoltage(float *voltage_V)
{
    uint16_t raw;
    HAL_StatusTypeDef status = INA219_ReadReg(INA219_REG_BUSVOLT, &raw);
    if (status != HAL_OK) return status;
    *voltage_V = (float)(raw >> 3) * 0.004f;
    return HAL_OK;
}

static HAL_StatusTypeDef INA219_ReadCurrent(float *current_A)
{
    uint16_t raw;
    HAL_StatusTypeDef status = INA219_ReadReg(INA219_REG_CURRENT, &raw);
    if (status != HAL_OK) return status;
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

    /* USER CODE BEGIN 2 */
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    if (INA219_Init() != HAL_OK)
    {
        ssd1306_SetCursor(0, 10);
        ssd1306_WriteString("INA219", Font_11x18, White);
        ssd1306_SetCursor(0, 35);
        ssd1306_WriteString("NOT FOUND", Font_11x18, White);
        ssd1306_UpdateScreen();
        while (1) {}
    }
    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1)
    {
        float voltage = 0.0f;
        float current = 0.0f;
        char buf[32];

        /* Re-write calibration every cycle to prevent it being lost */
        INA219_WriteReg(INA219_REG_CALIBRATION, INA219_CALIBRATION);
        HAL_Delay(50);

        INA219_ReadBusVoltage(&voltage);
        INA219_ReadCurrent(&current);

        ssd1306_Fill(Black);

        /* Row 1 – Voltage */
        sprintf(buf, "V: %.2f V", voltage);
        ssd1306_SetCursor(0, 10);
        ssd1306_WriteString(buf, Font_11x18, White);

        /* Row 2 – Current */
        sprintf(buf, "I: %.3f A", current);
        ssd1306_SetCursor(0, 40);
        ssd1306_WriteString(buf, Font_11x18, White);

        ssd1306_UpdateScreen();
        HAL_Delay(500);
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI
                                     | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB | RCC_PERIPHCLK_USART3
                                       | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_RTC;
    PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    PeriphClkInit.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_HSI;
    PeriphClkInit.RTCClockSelection    = RCC_RTCCLKSOURCE_LSI;
    PeriphClkInit.USBClockSelection    = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00201D2B;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) Error_Handler();
}

static void MX_RTC_Init(void)
{
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 38400;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

static void MX_USB_PCD_Init(void)
{
    hpcd_USB_FS.Instance = USB;
    hpcd_USB_FS.Init.dev_endpoints = 8;
    hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_FS.Init.lpm_enable = DISABLE;
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

    GPIO_InitStruct.Pin  = USER_Btn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = LD1_Pin | LD3_Pin | LD2_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = USB_PowerSwitchOn_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = USB_OverCurrent_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
