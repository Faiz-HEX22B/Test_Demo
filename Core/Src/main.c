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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SEGGER_RTT.h" // SEGGER RTT 实时传输库
#include "arm_math.h"   // CMSIS DSP 数学库（三角函数等）
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// RTT 数据传输结构体：4个float通道用于JScope波形显示
typedef struct
{
    float rtt_ch1; // 通道1：角度值（度）
    float rtt_ch2; // 通道2：弧度值
    float rtt_ch3; // 通道3：sin值
    float rtt_ch4; // 通道4：cos值
} RTT_Update_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define JSCOPE_CH 1           // JScope 使用的 RTT 通道号
#define DEG_TO_RAD 0.0174533f // 角度转弧度系数 (π/180)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
RTT_Update_t rtt_update; // RTT 数据缓冲区

static float angle = 0.0f;     // 当前角度值（度），范围 0~360
static float angle_rad = 0.0f; // 当前角度值（弧度），范围 0~2π

static uint8_t jscope_buffer[2048]; // JScope RTT 环形缓冲区

// LED 异步闪烁控制结构体
typedef struct
{
    GPIO_TypeDef *port;   // GPIO 端口
    uint16_t pin;         // GPIO 引脚
    uint32_t period_ms;   // 闪烁周期 (ms)
    uint32_t last_toggle; // 上次翻转时的系统 tick
} LED_Blink_t;

// 红色 LED (PC5): 500ms 闪烁; 蓝色 LED (PB2): 1000ms 闪烁
static LED_Blink_t led_red = {LED_R_GPIO_Port, LED_R_Pin, 500, 0};
static LED_Blink_t led_blue = {LED_B_GPIO_Port, LED_B_Pin, 1000, 0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  SysTick 回调函数 (每 1ms 执行一次)
 *         功能：计算三角函数并通过 RTT 发送数据到 JScope
 * @retval None
 */
void HAL_SYSTICK_Callback(void)
{
    // 角度转弧度
    angle_rad = angle * DEG_TO_RAD;

    // 填充 RTT 数据
    rtt_update.rtt_ch1 = angle;                  // 原始角度（度）
    rtt_update.rtt_ch2 = angle_rad;              // 弧度值
    rtt_update.rtt_ch3 = arm_sin_f32(angle_rad); // sin 值（CMSIS DSP 库）
    rtt_update.rtt_ch4 = arm_cos_f32(angle_rad); // cos 值（CMSIS DSP 库）

    // 通过 RTT 发送数据到 JScope
    SEGGER_RTT_Write(JSCOPE_CH, &rtt_update, sizeof(rtt_update));

    // 角度递增 0.01 度，到达 360 度后归零
    angle += 0.01f;
    if (angle >= 360.0f)
    {
        angle = 0.0f;
    }
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
    /* USER CODE BEGIN 2 */
    // 初始化 SEGGER RTT
    SEGGER_RTT_Init();
    // 配置 RTT 上行缓冲区，格式 "JScope_f4f4f4f4" 表示 4 个 float 通道
    SEGGER_RTT_ConfigUpBuffer(JSCOPE_CH, "JScope_f4f4f4f4", jscope_buffer, sizeof(jscope_buffer),
                              SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        uint32_t now = HAL_GetTick();

        // 红色 LED 闪烁控制（500ms 周期）
        if (now - led_red.last_toggle >= led_red.period_ms)
        {
            HAL_GPIO_TogglePin(led_red.port, led_red.pin);
            led_red.last_toggle = now;
            SEGGER_RTT_printf(0, "LED Red Toggled!\r\n");
        }

        // 蓝色 LED 闪烁控制（1000ms 周期）
        if (now - led_blue.last_toggle >= led_blue.period_ms)
        {
            HAL_GPIO_TogglePin(led_blue.port, led_blue.pin);
            led_blue.last_toggle = now;
            SEGGER_RTT_printf(0, "LED Blue Toggled!\r\n");
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

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
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
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */