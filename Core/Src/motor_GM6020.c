/*
 * motor_GM6020.c
 * GM6020 电机驱动实现
 * 使用 C 语言实现，通过 CAN 总线控制
 * 
 * GM6020 技术参数：
 *   - 编码器分辨率：12位 (0-8191)
 *   - 最大电压：30000
 *   - CAN 控制帧 ID：0x1FF（电压控制，每组4个电机）
 *   - CAN 反馈帧 ID：0x204 + 电机ID (0x205-0x208)
 */

#include "motor_GM6020.h"
#include <string.h>

/* GM6020 电机断开超时时间 (ms) */
#define GM6020_DISCONNECT_TIMEOUT    5000

/* 初始化 GM6020 电机 */
void GM6020_Init(GM6020_HandleTypeDef *hgm6020, uint8_t id, int16_t max_voltage)
{
    hgm6020->motor_id = id;
    hgm6020->max_voltage = max_voltage;
    hgm6020->target_voltage = 0;
    hgm6020->current_current = 0;
    hgm6020->current_velocity = 0;
    hgm6020->current_angle = 0;
    hgm6020->current_round = 0;
    hgm6020->state = GM6020_DISCONNECTED;
    hgm6020->last_update_tick = HAL_GetTick();
    hgm6020->_last_angle = 0;
    hgm6020->_first_decode = 0;
}

/* 设置目标电压（带限幅） */
void GM6020_SetVoltage(GM6020_HandleTypeDef *hgm6020, int16_t voltage)
{
    /* 限幅 */
    if (voltage > hgm6020->max_voltage)
        voltage = hgm6020->max_voltage;
    else if (voltage < -hgm6020->max_voltage)
        voltage = -hgm6020->max_voltage;
    
    /* 如果电机处于 IDLE 状态，切换到 RUNNING */
    if (hgm6020->state == GM6020_IDLE)
        hgm6020->state = GM6020_RUNNING;
    
    hgm6020->target_voltage = voltage;
}

/* 解码 CAN 反馈数据 */
void GM6020_DecodeCAN(GM6020_HandleTypeDef *hgm6020, uint8_t *data)
{
    /* 保存上一次角度 */
    hgm6020->_last_angle = hgm6020->current_angle;
    
    /* 解码角度 (12位，0-8191) */
    hgm6020->current_angle = (data[0] << 8) | data[1];
    
    /* 解码速度 (RPM) */
    hgm6020->current_velocity = (data[2] << 8) | data[3];
    
    /* 解码当前实际电流 */
    hgm6020->current_current = (data[4] << 8) | data[5];
    
    /* 更新时间戳 */
    hgm6020->last_update_tick = HAL_GetTick();
    
    /* 如果之前是断开状态，现在变为 IDLE */
    if (hgm6020->state == GM6020_DISCONNECTED)
        hgm6020->state = GM6020_IDLE;
    
    /* 首次解码，跳过过零检测 */
    if (!hgm6020->_first_decode) {
        hgm6020->_first_decode = 1;
        return;
    }
    
    /* 判断是否过零 */
    if ((hgm6020->current_angle < 2048) && (hgm6020->_last_angle > 6144))
        hgm6020->current_round += 1;
    else if ((hgm6020->current_angle > 6144) && (hgm6020->_last_angle < 2048))
        hgm6020->current_round -= 1;
}

/* 获取当前应发送的电压值（带状态检查） */
int16_t GM6020_GetTargetVoltage(GM6020_HandleTypeDef *hgm6020)
{
    uint32_t now = HAL_GetTick();
    
    /* 检查是否超时断开 */
    if (now - hgm6020->last_update_tick > GM6020_DISCONNECT_TIMEOUT) {
        hgm6020->state = GM6020_DISCONNECTED;
    }
    
    /* 如果电机已断开，返回 0 */
    if (hgm6020->state == GM6020_DISCONNECTED) {
        hgm6020->target_voltage = 0;
    }
    
    return hgm6020->target_voltage;
}

/* 检查电机是否已连接 */
uint8_t GM6020_IsConnected(GM6020_HandleTypeDef *hgm6020)
{
    return (hgm6020->state != GM6020_DISCONNECTED);
}

/* 重置圈数 */
void GM6020_ResetRound(GM6020_HandleTypeDef *hgm6020)
{
    hgm6020->current_round = 0;
}