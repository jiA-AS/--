/*
 * motor_M3508.c
 * M3508 电机驱动实现
 * 使用 C 语言实现，通过 CAN1 总线控制
 */

#include "motor_M3508.h"
#include <string.h>

/* M3508 电机断开超时时间 (ms) */
#define M3508_DISCONNECT_TIMEOUT    5000

/* 初始化 M3508 电机 */
void M3508_Init(M3508_HandleTypeDef *hm3508, uint8_t id, int16_t max_current)
{
    hm3508->motor_id = id;
    hm3508->max_current = max_current;
    hm3508->target_current = 0;
    hm3508->current_velocity = 0;
    hm3508->current_angle = 0;
    hm3508->current_round = 0;
    hm3508->state = M3508_DISCONNECTED;
    hm3508->last_update_tick = HAL_GetTick();
    hm3508->_last_angle = 0;
    hm3508->_first_decode = 0;
}

/* 设置目标电流（带限幅） */
void M3508_SetCurrent(M3508_HandleTypeDef *hm3508, int16_t current)
{
    /* 限幅 */
    if (current > hm3508->max_current)
        current = hm3508->max_current;
    else if (current < -hm3508->max_current)
        current = -hm3508->max_current;
    
    /* 如果电机已断开，不允许设置电流 */
    if (hm3508->state == M3508_DISCONNECTED)
        return;
    
    /* 如果电机处于 IDLE 状态，切换到 RUNNING */
    if (hm3508->state == M3508_IDLE)
        hm3508->state = M3508_RUNNING;
    
    hm3508->target_current = current;
}

/* 解码 CAN 反馈数据 */
void M3508_DecodeCAN(M3508_HandleTypeDef *hm3508, uint8_t *data)
{
    /* 保存上一次角度 */
    hm3508->_last_angle = hm3508->current_angle;
    
    /* 解码角度 (12位，0-8191) */
    hm3508->current_angle = (data[0] << 8) | data[1];
    
    /* 解码速度 (RPM) */
    hm3508->current_velocity = (data[2] << 8) | data[3];
    
    /* 更新时间戳 */
    hm3508->last_update_tick = HAL_GetTick();
    
    /* 如果之前是断开状态，现在变为 IDLE */
    if (hm3508->state == M3508_DISCONNECTED)
        hm3508->state = M3508_IDLE;
    
    /* 首次解码，跳过过零检测 */
    if (!hm3508->_first_decode) {
        hm3508->_first_decode = 1;
        return;
    }
    
    /* 判断是否过零 */
    if ((hm3508->current_angle < 2048) && (hm3508->_last_angle > 6144))
        hm3508->current_round += 1;
    else if ((hm3508->current_angle > 6144) && (hm3508->_last_angle < 2048))
        hm3508->current_round -= 1;
}

/* 获取当前应发送的电流值（带状态检查） */
int16_t M3508_GetTargetCurrent(M3508_HandleTypeDef *hm3508)
{
    uint32_t now = HAL_GetTick();
    
    /* 检查是否超时断开 */
    if (now - hm3508->last_update_tick > M3508_DISCONNECT_TIMEOUT) {
        hm3508->state = M3508_DISCONNECTED;
    } else if (hm3508->state == M3508_DISCONNECTED) {
        hm3508->state = M3508_IDLE;
    }
    
    /* 如果电机不在 RUNNING 状态，返回 0 */
    if (hm3508->state != M3508_RUNNING) {
        hm3508->target_current = 0;
    }
    
    return hm3508->target_current;
}

/* 检查电机是否已连接 */
uint8_t M3508_IsConnected(M3508_HandleTypeDef *hm3508)
{
    return (hm3508->state != M3508_DISCONNECTED);
}

/* 重置圈数 */
void M3508_ResetRound(M3508_HandleTypeDef *hm3508)
{
    hm3508->current_round = 0;
}