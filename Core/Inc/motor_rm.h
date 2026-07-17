#ifndef MOTOR_RM_H
#define MOTOR_RM_H

#include "main.h"
#include "can.h"
#include <stdint.h>

// ============================================================
// C 语言桥接接口（供 can.c / main.c / freertos.c 使用）
// 用 extern "C" 确保 C 和 C++ 编译器看到一致的链接
// ============================================================
#ifdef __cplusplus
extern "C" {
#endif

// 不透明指针类型
typedef struct MotorRM_T MotorRM_T;

// 全局电机对象指针（在 motor_rm.cpp 中定义）
extern MotorRM_T* motor_rm_m2006;
extern MotorRM_T* motor_rm_m3508_2;
extern MotorRM_T* motor_rm_m3508_3;
extern MotorRM_T* motor_rm_gm6020;

// CAN 反馈分发
void motor_rm_can1_dispatch(uint32_t std_id, uint8_t *data);
void motor_rm_can2_dispatch(uint32_t std_id, uint8_t *data);

// 工具函数：将 int16 值填入 CAN 8 字节数组的指定槽位
void packCanArray(uint8_t *aData, uint8_t slot, int16_t value);

// 电机控制接口
void    motor_rm_c_init_all(void);
int16_t motor_rm_c_get_output(MotorRM_T *motor);
int8_t  motor_rm_c_is_connected(MotorRM_T *motor);
void    motor_rm_c_set_output(MotorRM_T *motor, int16_t output);
void    motor_rm_c_reset_round(MotorRM_T *motor);
int8_t  motor_rm_c_is_voltage_control(MotorRM_T *motor);
int16_t motor_rm_c_get_state(MotorRM_T *motor); // 0=DISCONNECTED, 1=IDLE, 2=RUNNING

#ifdef __cplusplus
}
#endif

// ============================================================
// 以下为 C++ 专用部分（C 编译器不可见）
// ============================================================
#ifdef __cplusplus

// M2006 / M3508 / GM6020 三个电机的统一接口封装
namespace motor {

enum class MotorType { M2006, M3508, GM6020 };
enum class MotorState { IDLE, RUNNING, DISCONNECTED };

class MotorRM {
public:
    MotorType   type_;
    MotorState  state_ = MotorState::DISCONNECTED;
    uint8_t     motor_id_;
    int16_t     max_output_;       // max_current 或 max_voltage
    int16_t     target_output_;    // target_current 或 target_voltage
    int16_t     current_current_;
    int16_t     current_velocity_;
    int16_t     current_angle_;
    int32_t     current_round_;
    uint32_t    last_update_tick_;
    bool        angle_reverse_;

private:
    int16_t     last_angle_;
    bool        first_decode_;

public:
    void init(uint8_t id, MotorType type, int16_t max_output, bool angle_reverse = false);
    void setOutput(int16_t output);
    void decodeCanMsg(const uint8_t *data);
    int16_t getOutput();      // 带超时检测
    bool    isConnected();
    void    resetRound();
    bool    isVoltageControl() const { return type_ == MotorType::GM6020; }
};

} // namespace motor

#endif // __cplusplus

#endif // MOTOR_RM_H