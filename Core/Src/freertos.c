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
#include "motor_M4310.h"
#include "motor_GM6020.h"
#include "can.h"
#include <string.h>

extern M2006_HandleTypeDef hm2006;     /* CAN1 ID=4, 反馈 0x204 */
extern M3508_HandleTypeDef hm3508_2;   /* CAN1 ID=2, 反馈 0x202 */
extern M3508_HandleTypeDef hm3508_3;   /* CAN1 ID=3, 反馈 0x203 */
extern GM6020_HandleTypeDef hgm6020;   /* CAN1 ID=1, 反馈 0x205, 控制 0x1FF */
extern M4310_HandleTypeDef hm4310_12;  /* CAN2 ID=12, 反馈 0x20C */
extern M4310_HandleTypeDef hm4310_13;  /* CAN2 ID=13, 反馈 0x20D */
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
      if (GM6020_IsConnected(&hgm6020) &&
          M2006_IsConnected(&hm2006) &&
          M3508_IsConnected(&hm3508_2) &&
          M3508_IsConnected(&hm3508_3) &&
          M4310_IsConnected(&hm4310_12) &&
          M4310_IsConnected(&hm4310_13))
          break;
      osDelay(10);
  }

  TickType_t xLastWakeTime = xTaskGetTickCount();

  /* 目标值设定 */
  int16_t target_2006 =0;     /* M2006: 30% 正转 */
  int16_t target_3508_2 = 0;   /* M3508 #1: ~12% 正转 */
  int16_t target_3508_3 = 0;      /* M3508 #2: 停止 */
  int16_t target_gm6020 = -30000;  /* GM6020: 100% 反向电压（测试用） */
  int16_t target_4310_12 = 3000;  /* M4310 #1: 30% 正转 */
  int16_t target_4310_13 = 3000;  /* M4310 #2: 30% 正转 */

  for(;;)
  {
    /* ==== CAN1 电机状态管理 ==== */
    if (hgm6020.state == GM6020_IDLE || hgm6020.state == GM6020_RUNNING)
        GM6020_SetVoltage(&hgm6020, target_gm6020);
    else
        hgm6020.target_voltage = 0;

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

    /* ==== CAN2 电机状态管理 ==== */
    if (hm4310_12.state == M4310_IDLE || hm4310_12.state == M4310_RUNNING)
        M4310_SetCurrent(&hm4310_12, target_4310_12);
    else
        hm4310_12.target_current = 0;

    if (hm4310_13.state == M4310_IDLE || hm4310_13.state == M4310_RUNNING)
        M4310_SetCurrent(&hm4310_13, target_4310_13);
    else
        hm4310_13.target_current = 0;

    /* ==== CAN1 发送: 0x200 帧（M3508+M2006 电流控制） ==== */
    CAN_TxHeaderTypeDef tx_header;
    uint8_t can1_data[8];
    uint32_t tx_mailbox, timeout;
    memset(can1_data, 0, 8);

    int16_t c2 = M3508_GetTargetCurrent(&hm3508_2);
    int16_t c3 = M3508_GetTargetCurrent(&hm3508_3);
    int16_t c4 = M2006_GetTargetCurrent(&hm2006);
    can1_data[0] = 0;  /* ID=1 保留给 GM6020（用 0x1FF 发送） */
    can1_data[1] = 0;
    can1_data[2] = (uint8_t)(c2 >> 8);
    can1_data[3] = (uint8_t)(c2);
    can1_data[4] = (uint8_t)(c3 >> 8);
    can1_data[5] = (uint8_t)(c3);
    can1_data[6] = (uint8_t)(c4 >> 8);
    can1_data[7] = (uint8_t)(c4);

    tx_header.StdId = 0x200;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = 8;
    tx_header.TransmitGlobalTime = DISABLE;

    timeout = 0;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
        if (++timeout > 5000) {
            HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            break;
        }
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
        HAL_CAN_AddTxMessage(&hcan1, &tx_header, can1_data, &tx_mailbox);

    /* ==== CAN1 发送: 0x1FF 帧（GM6020 电压控制） ==== */
    uint8_t gm6020_data[8];
    memset(gm6020_data, 0, 8);

    int16_t v1 = GM6020_GetTargetVoltage(&hgm6020);
    gm6020_data[0] = (uint8_t)(v1 >> 8);
    gm6020_data[1] = (uint8_t)(v1);

    tx_header.StdId = 0x1FF;
    tx_header.DLC = 8;

    timeout = 0;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
        if (++timeout > 5000) {
            HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            break;
        }
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
        HAL_CAN_AddTxMessage(&hcan1, &tx_header, gm6020_data, &tx_mailbox);

    /* ==== CAN2 发送: 0x200 帧（M4310×2 电流控制） ==== */
    uint8_t can2_data[8];
    memset(can2_data, 0, 8);

    int16_t c12 = M4310_GetTargetCurrent(&hm4310_12);  /* ID=12 → bytes 0-1 (组1电机0) */
    int16_t c13 = M4310_GetTargetCurrent(&hm4310_13);  /* ID=13 → bytes 2-3 (组1电机1) */
    can2_data[0] = (uint8_t)(c12 >> 8);
    can2_data[1] = (uint8_t)(c12);
    can2_data[2] = (uint8_t)(c13 >> 8);
    can2_data[3] = (uint8_t)(c13);

    tx_header.StdId = 0x200;
    tx_header.DLC = 8;

    timeout = 0;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) == 0) {
        if (++timeout > 5000) {
            HAL_CAN_AbortTxRequest(&hcan2, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            break;
        }
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) > 0)
        HAL_CAN_AddTxMessage(&hcan2, &tx_header, can2_data, &tx_mailbox);

    vTaskDelayUntil(&xLastWakeTime, 1);  /* 1KHz */
  }
  /* USER CODE END StartDefaultTask */
}