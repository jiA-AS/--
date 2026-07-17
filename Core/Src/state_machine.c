/*
 * state_machine.c
 * 简化版状态机：BOOT → PROTECT ⇄ REMOTE
 *
 * 状态切换逻辑：
 *   BOOT     : 等待所有电机连接 → 自动切 PROTECT
 *   PROTECT  : 所有电机锁定。Switch_Right ≠ UP 且所有电机在线 → REMOTE
 *   REMOTE   : PID 闭环控制。Switch_Right = UP 或任意电机断开 → PROTECT
 */

#include "state_machine.h"
#include "motor_rm.h"
#include "motor_M4310.h"
#include "dbus.h"
#include <stdint.h>

int g_dart_state = STATE_BOOT;

/* 外部声明 M4310 */
extern M4310_HandleTypeDef hm4310_12;
extern M4310_HandleTypeDef hm4310_13;

/* 检查所有电机是否在线 */
static int all_motors_online(void)
{
    return motor_rm_c_is_connected(motor_rm_gm6020) &&
           motor_rm_c_is_connected(motor_rm_m2006) &&
           motor_rm_c_is_connected(motor_rm_m3508_2) &&
           motor_rm_c_is_connected(motor_rm_m3508_3) &&
           M4310_IsConnected(&hm4310_12) &&
           M4310_IsConnected(&hm4310_13);
}

/* 检查是否有电机断开 */
static int any_motor_disconnected(void)
{
    return !all_motors_online();
}

/* 遥控器是否在线（1秒内有更新） */
static int is_remote_online(void)
{
    return (xTaskGetTickCount() - RC_Data.last_update_time) < pdMS_TO_TICKS(1000);
}

void state_machine_init(void)
{
    g_dart_state = STATE_BOOT;
}

void state_machine_update(void)
{
    switch (g_dart_state)
    {
    case STATE_BOOT:
        /* 等所有电机上线后，进入 PROTECT（安全起见不直接进 REMOTE） */
        if (all_motors_online()) {
            g_dart_state = STATE_PROTECT;
        }
        break;

    case STATE_PROTECT:
        /* 遥控器不在线 → 保持 PROTECT */
        if (!is_remote_online()) {
            break;
        }
        /* 三段开关不在 UP 且所有电机在线 → REMOTE */
        if (RC_Data.Switch_Right != RC_SW_UP && !any_motor_disconnected()) {
            g_dart_state = STATE_REMOTE;
        }
        break;

    case STATE_REMOTE:
        /* 三段开关打到 UP 或电机断开 → 切回 PROTECT */
        if (RC_Data.Switch_Right == RC_SW_UP || any_motor_disconnected()) {
            g_dart_state = STATE_PROTECT;
        }
        break;
    }
}