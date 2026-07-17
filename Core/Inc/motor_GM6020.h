/*
 * motor_GM6020.h
 * GM6020 电机驱动头文件
 * 使用 C 语言实现，通过 CAN1 总线控制
 * 
 * GM6020 技术参数：
 *   - 编码器分辨率：12位 (0-8191)
 *   - 最大电压：30000
 *   - CAN 控制帧 ID：0x1FF (电压控制，不同于0x200电流控制)
 *   - CAN 反馈帧 ID：0x204 + 电机ID
 */

#ifndef MOTOR_GM6020_H
#define MOTOR_GM6020_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "can.h"
#include <stdint.h>

/* GM6020 电机状态枚举 */
typedef enum {
    GM6020_IDLE = 0,
    GM6020_RUNNING,
    GM6020_DISCONNECTED
} GM6020_State;

/* GM6020 电机句柄结构体 */
typedef struct {
    uint8_t     motor_id;           /* 电机 CAN ID (1-8) */
    int16_t     max_voltage;        /* 最大电压 (0-30000) */
    int16_t     target_voltage;     /* 目标电压（你要它转的电压） */
    int16_t     current_current;    /* 当前实际电流（CAN反馈回来的） */
    int16_t     current_velocity;   /* 当前速度 (RPM) */
    int16_t     current_angle;      /* 当前角度 (0-8191) */
    int32_t     current_round;      /* 当前圈数 */
    GM6020_State state;              /* 电机状态 */
    uint32_t    last_update_tick;   /* 最后更新时间戳 */
    
    /* 内部使用 */
    int16_t     _last_angle;
    uint8_t     _first_decode;
} GM6020_HandleTypeDef;

/* 初始化 GM6020 电机 */
void GM6020_Init(GM6020_HandleTypeDef *hgm6020, uint8_t id, int16_t max_voltage);

/* 设置目标电压（带限幅） */
void GM6020_SetVoltage(GM6020_HandleTypeDef *hgm6020, int16_t voltage);

/* 解码 CAN 反馈数据（在 CAN 接收回调中调用） */
void GM6020_DecodeCAN(GM6020_HandleTypeDef *hgm6020, uint8_t *data);

/* 获取当前应发送的电压值（带状态检查） */
int16_t GM6020_GetTargetVoltage(GM6020_HandleTypeDef *hgm6020);

/* 检查电机是否已连接 */
uint8_t GM6020_IsConnected(GM6020_HandleTypeDef *hgm6020);

/* 重置圈数 */
void GM6020_ResetRound(GM6020_HandleTypeDef *hgm6020);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_GM6020_H */