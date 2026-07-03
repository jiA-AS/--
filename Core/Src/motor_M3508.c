/*
 * motor_M3508.c
 * M3508 电机驱动实现
 */

#include "motor_M3508.h"
#include <string.h>

#define M3508_DISCONNECT_TIMEOUT    5000

void M3508_Init(M3508_HandleTypeDef *hm3508, uint8_t id, int16_t max_current)
{
    hm3508->motor_id = id;
    hm3508->max_current = max_current;
    hm3508->target_current = 0;
    hm3508->current_current = 0;
    hm3508->current_velocity = 0;
    hm3508->current_angle = 0;
    hm3508->current_round = 0;
    hm3508->state = M3508_DISCONNECTED;
    hm3508->last_update_tick = HAL_GetTick();
    hm3508->_last_angle = 0;
    hm3508->_first_decode = 0;
}

void M3508_SetCurrent(M3508_HandleTypeDef *hm3508, int16_t current)
{
    /* 限幅 */
    if (current > hm3508->max_current)
        current = hm3508->max_current;
    else if (current < -hm3508->max_current)
        current = -hm3508->max_current;

    /* 状态转换 */
    if (hm3508->state == M3508_IDLE)
        hm3508->state = M3508_RUNNING;
    else if (hm3508->state == M3508_DISCONNECTED)
        return;   /* 电机未连接，拒绝设置电流（参考代码行为） */

    hm3508->target_current = current;
}

void M3508_DecodeCAN(M3508_HandleTypeDef *hm3508, uint8_t *data)
{
    hm3508->_last_angle = hm3508->current_angle;

    hm3508->current_angle = (data[0] << 8) | data[1];
    hm3508->current_velocity = (data[2] << 8) | data[3];
    hm3508->current_current = (data[4] << 8) | data[5];

    hm3508->last_update_tick = HAL_GetTick();

    if (hm3508->state == M3508_DISCONNECTED)
        hm3508->state = M3508_IDLE;

    if (!hm3508->_first_decode) {
        hm3508->_first_decode = 1;
        return;
    }

    if ((hm3508->current_angle < 2048) && (hm3508->_last_angle > 6144))
        hm3508->current_round += 1;
    else if ((hm3508->current_angle > 6144) && (hm3508->_last_angle < 2048))
        hm3508->current_round -= 1;
}

int16_t M3508_GetTargetCurrent(M3508_HandleTypeDef *hm3508)
{
    uint32_t now = HAL_GetTick();

    if (now - hm3508->last_update_tick > M3508_DISCONNECT_TIMEOUT) {
        hm3508->state = M3508_DISCONNECTED;
    }

    if (hm3508->state == M3508_DISCONNECTED) {
        hm3508->target_current = 0;
    }

    return hm3508->target_current;
}

uint8_t M3508_IsConnected(M3508_HandleTypeDef *hm3508)
{
    return (hm3508->state != M3508_DISCONNECTED);
}

void M3508_ResetRound(M3508_HandleTypeDef *hm3508)
{
    hm3508->current_round = 0;
}