/*
 * motor_M4310.c
 * M4310 电机驱动实现
 * 使用 C 语言实现，通过 CAN 总线控制
 * 
 * M4310 技术参数：
 *   - 编码器分辨率：12位 (0-8191)
 *   - 最大电流：10000
 *   - CAN 控制帧 ID：0x200 (发送电流)
 *   - CAN 反馈帧 ID：0x200 + 电机ID (0x201-0x208)
 */

#include "motor_M4310.h"
#include <string.h>

/* M4310 电机断开超时时间 (ms) */
#define M4310_DISCONNECT_TIMEOUT    5000

/* 初始化 M4310 电机 */
void M4310_Init(M4310_HandleTypeDef *hm4310, uint8_t id, int16_t max_current)
{
    hm4310->motor_id = id;
    hm4310->max_current = max_current;
    hm4310->target_current = 0;
    hm4310->current_current = 0;
    hm4310->current_velocity = 0;
    hm4310->current_angle = 0;
    hm4310->current_round = 0;
    hm4310->state = M4310_DISCONNECTED;
    hm4310->last_update_tick = HAL_GetTick();
    hm4310->_last_angle = 0;
    hm4310->_first_decode = 0;
}

/* 设置目标电流（带限幅） */
void M4310_SetCurrent(M4310_HandleTypeDef *hm4310, int16_t current)
{
    /* 限幅 */
    if (current > hm4310->max_current)
        current = hm4310->max_current;
    else if (current < -hm4310->max_current)
        current = -hm4310->max_current;
    
    /* IDLE → RUNNING */
    if (hm4310->state == M4310_IDLE)
        hm4310->state = M4310_RUNNING;

    /* DISCONNECTED 状态下拒绝设置 */
    if (hm4310->state == M4310_DISCONNECTED)
        return;
    
    hm4310->target_current = current;
}

/* 解码 CAN 反馈数据 */
void M4310_DecodeCAN(M4310_HandleTypeDef *hm4310, uint8_t *data)
{
    /* 保存上一次角度 */
    hm4310->_last_angle = hm4310->current_angle;
    
    /* 解码角度 (12位，0-8191) */
    hm4310->current_angle = (data[0] << 8) | data[1];
    
    /* 解码速度 (RPM) */
    hm4310->current_velocity = (data[2] << 8) | data[3];
    
    /* 解码当前实际电流 */
    hm4310->current_current = (data[4] << 8) | data[5];
    
    /* 更新时间戳 */
    hm4310->last_update_tick = HAL_GetTick();
    
    /* 如果之前是断开状态，现在变为 IDLE */
    if (hm4310->state == M4310_DISCONNECTED)
        hm4310->state = M4310_IDLE;
    
    /* 首次解码，跳过过零检测 */
    if (!hm4310->_first_decode) {
        hm4310->_first_decode = 1;
        return;
    }
    
    /* 判断是否过零 */
    if ((hm4310->current_angle < 2048) && (hm4310->_last_angle > 6144))
        hm4310->current_round += 1;
    else if ((hm4310->current_angle > 6144) && (hm4310->_last_angle < 2048))
        hm4310->current_round -= 1;
}

/* 获取当前应发送的电流值（带状态检查） */
int16_t M4310_GetTargetCurrent(M4310_HandleTypeDef *hm4310)
{
    uint32_t now = HAL_GetTick();
    
    /* 检查是否超时断开 */
    if (now - hm4310->last_update_tick > M4310_DISCONNECT_TIMEOUT) {
        hm4310->state = M4310_DISCONNECTED;
    }
    
    /* 如果电机已断开，返回 0 */
    if (hm4310->state == M4310_DISCONNECTED) {
        hm4310->target_current = 0;
    }
    
    return hm4310->target_current;
}

/* 检查电机是否已连接 */
uint8_t M4310_IsConnected(M4310_HandleTypeDef *hm4310)
{
    return (hm4310->state != M4310_DISCONNECTED);
}

/* 重置圈数 */
void M4310_ResetRound(M4310_HandleTypeDef *hm4310)
{
    hm4310->current_round = 0;
}