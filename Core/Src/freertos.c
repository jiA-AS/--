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
#include "motor_M4310.h"
#include "motor_rm.h"
#include "dbus.h"
#include "pid_controller.h"
#include "state_machine.h"
#include "can.h"
#include <string.h>

// RM 系列电机状态值（与旧枚举 M*_IDLE=0, M*_RUNNING=1, M*_DISCONNECTED=2 保持一致）
#define RM_STATE_IDLE          0
#define RM_STATE_RUNNING       1
#define RM_STATE_DISCONNECTED  2

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
      if (motor_rm_c_is_connected(motor_rm_gm6020) &&
          motor_rm_c_is_connected(motor_rm_m2006) &&
          motor_rm_c_is_connected(motor_rm_m3508_2) &&
          motor_rm_c_is_connected(motor_rm_m3508_3) &&
          M4310_IsConnected(&hm4310_12) &&
          M4310_IsConnected(&hm4310_13))
          break;
      osDelay(10);
  }

  TickType_t xLastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    /* 状态机更新：根据遥控器和电机连接状态切换 BOOT/PROTECT/REMOTE */
    state_machine_update();

    if (g_dart_state == STATE_REMOTE)
    {
        /* 遥控器 → 目标速度 (RPM)
         *   ch0 (右摇杆 X) → GM6020 目标速度
         *   ch2 (左摇杆 X) → M3508 #1 目标速度
         *   ch3 (左摇杆 Y) → M3508 #2 目标速度
         *   ch4_wheel (拨轮) → M2006 目标速度
         */
        int16_t scale = (RC_Data.Switch_Right == RC_SW_MID) ? 1 : 5;

        pid_controller_set_target_gm6020((RC_Data.ch0 - 1024) * scale);
        pid_controller_set_target_m2006((RC_Data.ch4_wheel - 1024) * scale);
        pid_controller_set_target_m3508_2((RC_Data.ch2 - 1024) * scale);
        pid_controller_set_target_m3508_3((RC_Data.ch3 - 1024) * scale);

        /* PID 计算输出 */
        motor_rm_c_set_output(motor_rm_gm6020, pid_controller_update_gm6020());
        motor_rm_c_set_output(motor_rm_m2006,  pid_controller_update_m2006());
        motor_rm_c_set_output(motor_rm_m3508_2, pid_controller_update_m3508_2());
        motor_rm_c_set_output(motor_rm_m3508_3, pid_controller_update_m3508_3());
    }
    else
    {
        /* PROTECT/BOOT: 全部锁定，PID 回零 */
        pid_controller_set_target_gm6020(0);
        pid_controller_set_target_m2006(0);
        pid_controller_set_target_m3508_2(0);
        pid_controller_set_target_m3508_3(0);

        motor_rm_c_set_output(motor_rm_gm6020, 0);
        motor_rm_c_set_output(motor_rm_m2006, 0);
        motor_rm_c_set_output(motor_rm_m3508_2, 0);
        motor_rm_c_set_output(motor_rm_m3508_3, 0);
    }

    /* CAN2 M4310 始终锁定（暂时不控） */
    M4310_SetCurrent(&hm4310_12, 0);
    M4310_SetCurrent(&hm4310_13, 0);

    /* ==== CAN1 发送: 0x200 帧（M3508+M2006 电流控制） ==== */
    CAN_TxHeaderTypeDef tx_header;
    uint8_t can1_data[8];
    uint32_t tx_mailbox, timeout;
    memset(can1_data, 0, 8);

    /* 槽位: [0]=ID1(保留), [1]=ID2, [2]=ID3, [3]=ID4 */
    /* ID=1 留给 GM6020，用 0x1FF 单独发 */
    can1_data[0] = 0;
    can1_data[1] = 0;
    // ID=2: M3508 #1 → 槽位 1
    packCanArray(can1_data, 1, motor_rm_c_get_output(motor_rm_m3508_2));
    // ID=3: M3508 #2 → 槽位 2
    packCanArray(can1_data, 2, motor_rm_c_get_output(motor_rm_m3508_3));
    // ID=4: M2006 → 槽位 3
    packCanArray(can1_data, 3, motor_rm_c_get_output(motor_rm_m2006));

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

    /* GM6020: ID=1 → 槽位 0 */
    packCanArray(gm6020_data, 0, motor_rm_c_get_output(motor_rm_gm6020));

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