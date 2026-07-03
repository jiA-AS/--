/*
 * motor_M2006.h
 * M2006 电机驱动头文件
 * 使用 C 语言实现，通过 CAN1 总线控制
 * 
 * M2006 技术参数：
 *   - 编码器分辨率：12位 (0-8191)
 *   - 最大电流：10000 (对应电流环给定值)
 *   - CAN 控制帧 ID：0x200 (发送电流)
 *   - CAN 反馈帧 ID：0x200 + 电机ID
 */

#ifndef MOTOR_M2006_H
#define MOTOR_M2006_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "can.h"
#include <stdint.h>

/* M2006 电机状态枚举 */
typedef enum {
    M2006_IDLE = 0,
    M2006_RUNNING,
    M2006_DISCONNECTED
} M2006_State;

/* M2006 电机句柄结构体 */
typedef struct {
    uint8_t     motor_id;           /* 电机 CAN ID (1-8) */
    int16_t     max_current;        /* 最大电流 (0-10000) */
    int16_t     target_current;     /* 目标电流（你要它转的电流） */
    int16_t     current_current;    /* 当前实际电流（CAN反馈回来的） */
    int16_t     current_velocity;   /* 当前速度 (RPM) */
    int16_t     current_angle;      /* 当前角度 (0-8191) */
    int32_t     current_round;      /* 当前圈数 */
    M2006_State state;              /* 电机状态 */
    uint32_t    last_update_tick;   /* 最后更新时间戳 */
    
    /* 内部使用 */
    int16_t     _last_angle;
    uint8_t     _first_decode;
} M2006_HandleTypeDef;

/* 初始化 M2006 电机 */
void M2006_Init(M2006_HandleTypeDef *hm2006, uint8_t id, int16_t max_current);

/* 设置目标电流（带限幅） */
void M2006_SetCurrent(M2006_HandleTypeDef *hm2006, int16_t current);

/* 解码 CAN 反馈数据（在 CAN 接收回调中调用） */
void M2006_DecodeCAN(M2006_HandleTypeDef *hm2006, uint8_t *data);

/* 获取当前应发送的电流值（带状态检查） */
int16_t M2006_GetTargetCurrent(M2006_HandleTypeDef *hm2006);

/* 检查电机是否已连接 */
uint8_t M2006_IsConnected(M2006_HandleTypeDef *hm2006);

/* 重置圈数 */
void M2006_ResetRound(M2006_HandleTypeDef *hm2006);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_M2006_H */