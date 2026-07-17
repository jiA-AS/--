#ifndef DBUS_H
#define DBUS_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdint.h"

/* DBUS 帧长度 */
#define RC_FRAME_LEN       18U

/* 摇杆中位值 */
#define RC_CH_VALUE_OFFSET 1024

/* 三段开关值 */
#define RC_SW_UP   1
#define RC_SW_MID  3
#define RC_SW_DOWN 2

/* 遥控器数据结构 */
typedef struct {
    uint16_t ch0;       // 右摇杆 X
    uint16_t ch1;       // 右摇杆 Y
    uint16_t ch2;       // 左摇杆 X
    uint16_t ch3;       // 左摇杆 Y
    uint16_t ch4_wheel; // 拨轮
    uint8_t  Switch_Right;
    uint8_t  Switch_Left;
    TickType_t last_update_time;
} RC_DataType;

extern RC_DataType RC_Data;
extern uint8_t RC_rx_buffer[RC_FRAME_LEN];

/* DBUS UART 配置 */
#define RC_UART_HANDLE       &huart1
#define RC_UART_RXBUFFER     RC_rx_buffer
#define RC_UART_BUFFER_LEN   RC_FRAME_LEN

void DT7_Init(void);
void DT7_Decode(void);

#endif // DBUS_H
