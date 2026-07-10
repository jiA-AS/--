/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "motor_M2006.h"
#include "motor_M3508.h"
#include "can.h"
#include <string.h>

extern M2006_HandleTypeDef hm2006;     /* ID=4, 反馈 0x204 */
extern M3508_HandleTypeDef hm3508_2;   /* ID=2, 反馈 0x202 */
extern M3508_HandleTypeDef hm3508_3;   /* ID=3, 反馈 0x203 */
/* USER CODE END Includes */

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 3000 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

void StartDefaultTask(void *argument);
extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void);

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName) { while(1); }

void MX_FREERTOS_Init(void) {
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
}

void StartDefaultTask(void *argument)
{
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */

  uint32_t wait_start = HAL_GetTick();
  while (HAL_GetTick() - wait_start < 3000) {
      if (M2006_IsConnected(&hm2006) && M3508_IsConnected(&hm3508_2) && M3508_IsConnected(&hm3508_3))
          break;
      osDelay(10);
  }

  TickType_t xLastWakeTime = xTaskGetTickCount();

  /* 所有电机测试通过，稳定控制 */
  int16_t target_2006 = 3000;    /* M2006: 30% 正转 */
  int16_t target_3508_2 = 2000;  /* M3508 #1: ~12% 正转 */
  int16_t target_3508_3 = 0;     /* M3508 #2: 停止 */

  for(;;)
  {
    if (hm2006.state == M2006_IDLE || hm2006.state == M2006_RUNNING)
        M2006_SetCurrent(&hm2006, target_2006);
    else
        hm2006.target_current = 0;

    if (hm3508_2.state == M3508_IDLE || hm3508_2.state == M3508_RUNNING)
        M3508_SetCurrent(&hm3508_2, target_3508_2);
    else
        hm3508_2.target_current = 0;

    if (hm3508_3.state == M3508_IDLE || hm3508_3.state == M3508_RUNNING)
        M3508_SetCurrent(&hm3508_3, target_3508_3);
    else
        hm3508_3.target_current = 0;

    uint8_t can_data[8];
    memset(can_data, 0, 8);

    int16_t c2 = M3508_GetTargetCurrent(&hm3508_2);
    int16_t c3 = M3508_GetTargetCurrent(&hm3508_3);
    int16_t c4 = M2006_GetTargetCurrent(&hm2006);
    can_data[2] = (uint8_t)(c2 >> 8);
    can_data[3] = (uint8_t)(c2);
    can_data[4] = (uint8_t)(c3 >> 8);
    can_data[5] = (uint8_t)(c3);
    can_data[6] = (uint8_t)(c4 >> 8);
    can_data[7] = (uint8_t)(c4);

    CAN_TxHeaderTypeDef tx_header;
    tx_header.StdId = 0x200;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = 8;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t timeout = 0;
    uint32_t tx_mailbox;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
        if (++timeout > 5000) {
            HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            break;
        }
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
        HAL_CAN_AddTxMessage(&hcan1, &tx_header, can_data, &tx_mailbox);
    }

    vTaskDelayUntil(&xLastWakeTime, 1);
  }
  /* USER CODE END StartDefaultTask */
}