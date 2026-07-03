/*
 * motor_M2006.c
 * M2006 电机驱动实现
 * 使用 C 语言实现，通过 CAN1 总线控制
 */

#include "motor_M2006.h"
#include <string.h>

/* M2006 电机断开超时时间 (ms) */
#define M2006_DISCONNECT_TIMEOUT    5000

/* 初始化 M2006 电机 */
void M2006_Init(M2006_HandleTypeDef *hm2006, uint8_t id, int16_t max_current)
{
    hm2006->motor_id = id;
    hm2006->max_current = max_current;
    hm2006->target_current = 0;
    hm2006->current_velocity = 0;
    hm2006->current_angle = 0;
    hm2006->current_round = 0;
    hm2006->state = M2006_DISCONNECTED;
    hm2006->last_update_tick = HAL_GetTick();
    hm2006->_last_angle = 0;
    hm2006->_first_decode = 0;
}

/* 设置目标电流（带限幅） */
void M2006_SetCurrent(M2006_HandleTypeDef *hm2006, int16_t current)
{
    /* 限幅 */
    if (current > hm2006->max_current)
        current = hm2006->max_current;
    else if (current < -hm2006->max_current)
        current = -hm2006->max_current;
    
    /* 如果电机已断开，不允许设置电流 */
    if (hm2006->state == M2006_DISCONNECTED)
        return;
    
    /* 如果电机处于 IDLE 状态，切换到 RUNNING */
    if (hm2006->state == M2006_IDLE)
        hm2006->state = M2006_RUNNING;
    
    hm2006->target_current = current;
}

/* 解码 CAN 反馈数据 */
void M2006_DecodeCAN(M2006_HandleTypeDef *hm2006, uint8_t *data)
{
    /* 保存上一次角度 */
    hm2006->_last_angle = hm2006->current_angle;
    
    /* 解码角度 (12位，0-8191) */
    hm2006->current_angle = (data[0] << 8) | data[1];
    
    /* 解码速度 (RPM) */
    hm2006->current_velocity = (data[2] << 8) | data[3];
    
    /* 更新时间戳 */
    hm2006->last_update_tick = HAL_GetTick();
    
    /* 如果之前是断开状态，现在变为 IDLE */
    if (hm2006->state == M2006_DISCONNECTED)
        hm2006->state = M2006_IDLE;
    
    /* 首次解码，跳过过零检测 */
    if (!hm2006->_first_decode) {
        hm2006->_first_decode = 1;
        return;
    }
    
    /* 判断是否过零 */
    if ((hm2006->current_angle < 2048) && (hm2006->_last_angle > 6144))
        hm2006->current_round += 1;
    else if ((hm2006->current_angle > 6144) && (hm2006->_last_angle < 2048))
        hm2006->current_round -= 1;
}

/* 获取当前应发送的电流值（带状态检查） */
int16_t M2006_GetTargetCurrent(M2006_HandleTypeDef *hm2006)
{
    uint32_t now = HAL_GetTick();
    
    /* 检查是否超时断开 */
    if (now - hm2006->last_update_tick > M2006_DISCONNECT_TIMEOUT) {
        hm2006->state = M2006_DISCONNECTED;
    } else if (hm2006->state == M2006_DISCONNECTED) {
        hm2006->state = M2006_IDLE;
    }
    
    /* 如果电机不在 RUNNING 状态，返回 0 */
    if (hm2006->state != M2006_RUNNING) {
        hm2006->target_current = 0;
    }
    
    return hm2006->target_current;
}

/* 检查电机是否已连接 */
uint8_t M2006_IsConnected(M2006_HandleTypeDef *hm2006)
{
    return (hm2006->state != M2006_DISCONNECTED);
}

/* 重置圈数 */
void M2006_ResetRound(M2006_HandleTypeDef *hm2006)
{
    hm2006->current_round = 0;
}