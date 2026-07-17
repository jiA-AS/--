/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "can.h"
#include "dma.h"
#include "rng.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "motor_M4310.h"
#include "motor_rm.h"
#include "dbus.h"
#include "pid_controller.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
M4310_HandleTypeDef hm4310_12;   /* M4310: CAN2 ID=12, 反馈 0x20C */
M4310_HandleTypeDef hm4310_13;   /* M4310: CAN2 ID=13, 反馈 0x20D */
/* USER CODE END PV */

void SystemClock_Config(void);
void MX_FREERTOS_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM12_Init();
  MX_TIM6_Init();
  MX_CAN1_Init();
  MX_CAN2_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM8_Init();
  MX_RNG_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();

  /* USER CODE BEGIN 2 */
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

  /* 配置 CAN2 滤波器 */
  sFilterConfig.FilterBank = 14;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig);
  HAL_CAN_Start(&hcan2);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);

  /* 初始化电机:
   * RM 系列 (M2006/M3508/GM6020) 通过 C++ 基类统一初始化
   * M4310 × 2 保持原有 C 接口
   */
  motor_rm_c_init_all();
  M4310_Init(&hm4310_12,  12,  10000);
  M4310_Init(&hm4310_13,  13,  10000);

  /* 初始化 PID 控制器（关联电机对象指针） */
  pid_controller_init_all();

  /* 初始化 DBUS 遥控器 */
  DT7_Init();
  HAL_UARTEx_ReceiveToIdle_DMA(RC_UART_HANDLE, RC_UART_RXBUFFER, RC_UART_BUFFER_LEN);

  uint32_t motor_wait_start = HAL_GetTick();
  while (!motor_rm_c_is_connected(motor_rm_gm6020) ||
         !motor_rm_c_is_connected(motor_rm_m2006) ||
         !motor_rm_c_is_connected(motor_rm_m3508_2) ||
         !motor_rm_c_is_connected(motor_rm_m3508_3) ||
         !M4310_IsConnected(&hm4310_12) ||
         !M4310_IsConnected(&hm4310_13)) {
      if (HAL_GetTick() - motor_wait_start > 3000) break;
  }
  /* USER CODE END 2 */

  osKernelInitialize();
  MX_FREERTOS_Init();
  osKernelStart();

  while (1) { }
}

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
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM1) HAL_IncTick();
}

void Error_Handler(void) { __disable_irq(); while(1); }

/* DBUS UART 空闲中断回调 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == RC_UART_HANDLE && huart->RxEventType == HAL_UART_RXEVENT_IDLE)
    {
        if (Size == RC_FRAME_LEN)
        {
            DT7_Decode();
        }
        memset(RC_UART_RXBUFFER, 0, RC_UART_BUFFER_LEN);
        HAL_UARTEx_ReceiveToIdle_DMA(RC_UART_HANDLE, RC_UART_RXBUFFER, RC_UART_BUFFER_LEN);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == RC_UART_HANDLE)
    {
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        HAL_UART_AbortReceive(huart);
        memset(RC_UART_RXBUFFER, 0, RC_UART_BUFFER_LEN);
        HAL_UARTEx_ReceiveToIdle_DMA(RC_UART_HANDLE, RC_UART_RXBUFFER, RC_UART_BUFFER_LEN);
    }
}
