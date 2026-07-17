/*
 * dbus.c
 * DJI DT7 遥控器 DBUS 协议解析
 *
 * DBUS 帧格式 (18 字节 @ 100kbps, 9bit, Even Parity):
 *   data[0:1]   = ch0 (右摇杆 X, 364~1684)
 *   data[1:2]   = ch1 (右摇杆 Y)
 *   data[2:4]   = ch2 (左摇杆 X)
 *   data[4:5]   = ch3 (左摇杆 Y)
 *   data[5]      = 开关 (bit6:左, bit4:右)
 *   data[6:15]   = 鼠标/键盘 (忽略)
 *   data[16:17]  = ch4 拨轮
 */

#include "dbus.h"
#include <string.h>

RC_DataType RC_Data;
uint8_t RC_rx_buffer[RC_FRAME_LEN];

#define DEBOUNCE_THRESHOLD 5

void DT7_Init(void)
{
    memset(&RC_Data, 0, sizeof(RC_Data));
    RC_Data.ch0 = RC_CH_VALUE_OFFSET;
    RC_Data.ch1 = RC_CH_VALUE_OFFSET;
    RC_Data.ch2 = RC_CH_VALUE_OFFSET;
    RC_Data.ch3 = RC_CH_VALUE_OFFSET;
    RC_Data.ch4_wheel = RC_CH_VALUE_OFFSET;
    RC_Data.Switch_Left  = RC_SW_UP;
    RC_Data.Switch_Right = RC_SW_UP;
    RC_Data.last_update_time = 0;
}

void DT7_Decode(void)
{
    static uint8_t last_sw_right = RC_SW_UP;
    static uint8_t last_sw_left  = RC_SW_UP;
    static uint8_t debounce_right = 0;
    static uint8_t debounce_left  = 0;

    RC_Data.last_update_time = xTaskGetTickCount();

    // --- 解析通道 ---
    RC_Data.ch0 = ((RC_rx_buffer[0] | (RC_rx_buffer[1] << 8)) & 0x07FF);
    RC_Data.ch1 = (((RC_rx_buffer[1] >> 3) | (RC_rx_buffer[2] << 5)) & 0x07FF);
    RC_Data.ch2 = (((RC_rx_buffer[2] >> 6) | (RC_rx_buffer[3] << 2) |
                    (RC_rx_buffer[4] << 10)) & 0x07FF);
    RC_Data.ch3 = (((RC_rx_buffer[4] >> 1) | (RC_rx_buffer[5] << 7)) & 0x07FF);
    RC_Data.ch4_wheel = ((RC_rx_buffer[16] | (RC_rx_buffer[17] << 8)) & 0x07FF);

    // --- 三段开关（带消抖） ---
    uint8_t sw_right = (RC_rx_buffer[5] >> 4) & 0x03;
    uint8_t sw_left  = (RC_rx_buffer[5] >> 6) & 0x03;

    if (sw_right == last_sw_right) {
        if (debounce_right < DEBOUNCE_THRESHOLD) debounce_right++;
    } else {
        debounce_right = 0;
    }
    if (debounce_right >= DEBOUNCE_THRESHOLD) {
        RC_Data.Switch_Right = sw_right;
    }
    last_sw_right = sw_right;

    if (sw_left == last_sw_left) {
        if (debounce_left < DEBOUNCE_THRESHOLD) debounce_left++;
    } else {
        debounce_left = 0;
    }
    if (debounce_left >= DEBOUNCE_THRESHOLD) {
        RC_Data.Switch_Left = sw_left;
    }
    last_sw_left = sw_left;
}