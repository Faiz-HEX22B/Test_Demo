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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// LED 异步闪烁控制结构体
typedef struct {
  GPIO_TypeDef *port;   // GPIO 端口
  uint16_t pin;         // GPIO 引脚
  uint32_t period_ms;   // 闪烁周期 (ms)
  uint32_t last_toggle; // 上次翻转时的系统时间戳
  uint8_t enable;       // 总开关: 1=允许输出, 0=强制熄灭
  uint8_t blink_enable; // 闪烁模式: 1=闪烁, 0=常亮 (仅 enable=1 时有效)
} LED_Blink_t;

// 红色 LED (PC5): 500ms 周期, 默认开启且闪烁
static LED_Blink_t led_red = {LED_R_GPIO_Port, LED_R_Pin, 500, 0, 1, 1};
// 蓝色 LED (PB2): 1000ms 周期, 默认开启且闪烁
static LED_Blink_t led_blue = {LED_B_GPIO_Port, LED_B_Pin, 1000, 0, 1, 1};

// 按键上一次的电平状态 (用于下降沿检测, 硬件已消抖)
static GPIO_PinState key2_last = GPIO_PIN_SET;
static GPIO_PinState key3_last = GPIO_PIN_SET;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void LED_AsyncBlink(LED_Blink_t *led);
static void KEY_Scan(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
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

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    KEY_Scan();                // 扫描按键, 刷新 LED 控制状态
    LED_AsyncBlink(&led_red);  // 红色 LED: 根据状态执行开关/闪烁
    LED_AsyncBlink(&led_blue); // 蓝色 LED: 根据状态执行开关/闪烁
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
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
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * @brief  非阻塞式 LED 闪烁处理
 *         需在 while(1) 中持续调用
 *         内部根据 enable / blink_enable 决定行为:
 *           enable=0                 -> 强制熄灭
 *           enable=1, blink_enable=0 -> 常亮
 *           enable=1, blink_enable=1 -> 按 period_ms 周期闪烁
 * @param  led: 指向 LED_Blink_t 结构体的指针
 * @retval None
 */
static void LED_AsyncBlink(LED_Blink_t *led) {
  if (led->enable == 0) {
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_RESET);
    return;
  }

  if (led->blink_enable == 0) {
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_SET);
    return;
  }

  uint32_t now = HAL_GetTick();
  if (now - led->last_toggle >= led->period_ms) {
    HAL_GPIO_TogglePin(led->port, led->pin);
    led->last_toggle = now;
  }
}

/**
 * @brief  按键扫描 (下降沿触发, 硬件已消抖)
 *         KEY_2 (PA1) -> 切换 led_red 的 enable (开/关)
 *         KEY_3 (PA4) -> 切换 led_blue 的 blink_enable (常亮/闪烁)
 * @retval None
 */
static void KEY_Scan(void) {
  GPIO_PinState key2_now = HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin);
  GPIO_PinState key3_now = HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin);

  // KEY_2: 下降沿 (上拉输入, 按下时为 LOW) -> 切换 led_red 总开关
  if (key2_last == GPIO_PIN_SET && key2_now == GPIO_PIN_RESET) {
    led_red.enable = !led_red.enable;
  }
  key2_last = key2_now;

  // KEY_3: 下降沿 -> led_blue 已开启时, 切换其常亮/闪烁模式
  if (key3_last == GPIO_PIN_SET && key3_now == GPIO_PIN_RESET) {
    if (led_blue.enable) {
      led_blue.blink_enable = !led_blue.blink_enable;
    }
  }
  key3_last = key3_now;
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */