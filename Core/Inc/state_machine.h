#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 状态枚举 */
enum DartState {
    STATE_BOOT = 0,     /* 启动：等待电机连接 */
    STATE_PROTECT,      /* 保护：所有电机锁定 */
    STATE_REMOTE        /* 遥控：PID 闭环控制 */
};

/* 当前状态（全局变量，freertos.c 中读取） */
extern int g_dart_state;

void state_machine_init(void);
void state_machine_update(void);

#ifdef __cplusplus
}
#endif

#endif // STATE_MACHINE_H