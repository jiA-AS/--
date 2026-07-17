/*
 * motor_rm.cpp
 * M2006 / M3508 / GM6020 电机统一实现
 * 将原来三套独立代码合并为一个 C++ 类
 *
 * RM 电机 CAN 反馈帧格式 (8 字节)：
 *   data[0:1] = 角度 (0-8191, 12位编码器)
 *   data[2:3] = 速度 (RPM)
 *   data[4:5] = 实际电流
 *   data[6:7] = 温度 (未使用)
 *
 * M2006/M3508: 控制帧 ID = 0x200, 反馈帧 ID = 0x200 + motor_id
 * GM6020:      控制帧 ID = 0x1FF, 反馈帧 ID = 0x204 + motor_id
 */

#include "motor_rm.h"

/* 电机断开超时时间 (ms) */
#define MOTOR_DISCONNECT_TIMEOUT    5000

namespace motor {

/* 初始化电机 */
void MotorRM::init(uint8_t id, MotorType type, int16_t max_output, bool angle_reverse)
{
    motor_id_    = id;
    type_        = type;
    max_output_  = max_output;
    angle_reverse_ = angle_reverse;
    target_output_  = 0;
    current_current_ = 0;
    current_velocity_ = 0;
    current_angle_  = 0;
    current_round_  = 0;
    state_          = MotorState::DISCONNECTED;
    last_update_tick_ = HAL_GetTick();
    last_angle_     = 0;
    first_decode_   = false;
}

/* 设置目标输出（带限幅，带状态管理） */
void MotorRM::setOutput(int16_t output)
{
    // 限幅
    if (output > max_output_)
        output = max_output_;
    else if (output < -max_output_)
        output = -max_output_;

    // IDLE → RUNNING
    if (state_ == MotorState::IDLE)
        state_ = MotorState::RUNNING;

    // DISCONNECTED 状态下拒绝设置（与 dart_mcu 行为一致）
    if (state_ == MotorState::DISCONNECTED)
        return;

    target_output_ = output;
}

/* 解码 CAN 反馈数据 */
void MotorRM::decodeCanMsg(const uint8_t *data)
{
    // 保存上一次角度
    last_angle_ = current_angle_;

    // 解码角度 (12 位，0-8191)
    current_angle_ = (data[0] << 8) | data[1];

    // 解码速度 (RPM)
    current_velocity_ = (data[2] << 8) | data[3];

    // 解码实际电流
    current_current_ = (data[4] << 8) | data[5];

    // 更新时间戳
    last_update_tick_ = HAL_GetTick();

    // DISCONNECTED → IDLE
    if (state_ == MotorState::DISCONNECTED)
        state_ = MotorState::IDLE;

    // 首次解码，跳过过零检测
    if (!first_decode_) {
        first_decode_ = true;
        return;
    }

    // 判断是否过零（12 位编码器）
    if ((current_angle_ < 2048) && (last_angle_ > 6144))
        current_round_ += 1;
    else if ((current_angle_ > 6144) && (last_angle_ < 2048))
        current_round_ -= 1;
}

/* 获取应发送的输出值（带超时检测） */
int16_t MotorRM::getOutput()
{
    uint32_t now = HAL_GetTick();

    // 检查是否超时断开
    if (now - last_update_tick_ > MOTOR_DISCONNECT_TIMEOUT) {
        state_ = MotorState::DISCONNECTED;
    }

    // 如果断开，清零输出
    if (state_ == MotorState::DISCONNECTED) {
        target_output_ = 0;
    }

    return target_output_;
}

/* 检查电机是否已连接 */
bool MotorRM::isConnected()
{
    return (state_ != MotorState::DISCONNECTED);
}

/* 重置圈数 */
void MotorRM::resetRound()
{
    current_round_ = 0;
}

/* 工具函数：将 int16 值填入 CAN 8 字节数组的指定槽位 */
void packCanArray(uint8_t *aData, uint8_t slot, int16_t value)
{
    aData[slot * 2]     = (uint8_t)(value >> 8);
    aData[slot * 2 + 1] = (uint8_t)(value);
}

} // namespace motor

// ============================================================
// 全局电机对象实例
// ============================================================
motor::MotorRM g_motor_m2006;
motor::MotorRM g_motor_m3508_2;
motor::MotorRM g_motor_m3508_3;
motor::MotorRM g_motor_gm6020;

// 不透明指针（供 C 文件使用）
MotorRM_T* motor_rm_m2006   = reinterpret_cast<MotorRM_T*>(&g_motor_m2006);
MotorRM_T* motor_rm_m3508_2 = reinterpret_cast<MotorRM_T*>(&g_motor_m3508_2);
MotorRM_T* motor_rm_m3508_3 = reinterpret_cast<MotorRM_T*>(&g_motor_m3508_3);
MotorRM_T* motor_rm_gm6020  = reinterpret_cast<MotorRM_T*>(&g_motor_gm6020);

// ============================================================
// 静态映射表：CAN 反馈帧 ID → 电机对象指针
// ============================================================
static const struct {
    uint32_t        can_id;
    motor::MotorRM* motor;
} can1_dispatch_table[] = {
    {0x205, &g_motor_gm6020},   // CAN1 ID=1 → 反馈 0x204+1
    {0x204, &g_motor_m2006},    // CAN1 ID=4 → 反馈 0x200+4
    {0x202, &g_motor_m3508_2},  // CAN1 ID=2 → 反馈 0x200+2
    {0x203, &g_motor_m3508_3},  // CAN1 ID=3 → 反馈 0x200+3
};

static const int can1_dispatch_count = sizeof(can1_dispatch_table) / sizeof(can1_dispatch_table[0]);

// ============================================================
// C 桥接实现
// ============================================================
extern "C" {

// packCanArray 的 C 可见包装（转发到 namespace motor 的实现）
void packCanArray(uint8_t *aData, uint8_t slot, int16_t value)
{
    motor::packCanArray(aData, slot, value);
}

// 初始化所有电机
void motor_rm_c_init_all(void)
{
    g_motor_gm6020.init(1, motor::MotorType::GM6020, 30000);
    g_motor_m2006.init(4, motor::MotorType::M2006, 10000);
    g_motor_m3508_2.init(2, motor::MotorType::M3508, 16384);
    g_motor_m3508_3.init(3, motor::MotorType::M3508, 16384);
}

// CAN1 反馈帧分发
void motor_rm_can1_dispatch(uint32_t std_id, uint8_t *data)
{
    for (int i = 0; i < can1_dispatch_count; i++) {
        if (can1_dispatch_table[i].can_id == std_id) {
            can1_dispatch_table[i].motor->decodeCanMsg(data);
            return;
        }
    }
}

// CAN2 反馈帧分发（保留给 M4310，不归此类管理）
void motor_rm_can2_dispatch(uint32_t std_id, uint8_t *data)
{
    // M4310 暂不归入 RM 基类，在 can.c 中单独处理
    (void)std_id;
    (void)data;
}

// 获取输出值（带超时检测）
int16_t motor_rm_c_get_output(MotorRM_T *motor)
{
    if (!motor) return 0;
    return reinterpret_cast<motor::MotorRM*>(motor)->getOutput();
}

// 检查是否连接
int8_t motor_rm_c_is_connected(MotorRM_T *motor)
{
    if (!motor) return 0;
    return reinterpret_cast<motor::MotorRM*>(motor)->isConnected() ? 1 : 0;
}

// 设置输出
void motor_rm_c_set_output(MotorRM_T *motor, int16_t output)
{
    if (!motor) return;
    reinterpret_cast<motor::MotorRM*>(motor)->setOutput(output);
}

// 重置圈数
void motor_rm_c_reset_round(MotorRM_T *motor)
{
    if (!motor) return;
    reinterpret_cast<motor::MotorRM*>(motor)->resetRound();
}

// 是否为电压控制
int8_t motor_rm_c_is_voltage_control(MotorRM_T *motor)
{
    if (!motor) return 0;
    return reinterpret_cast<motor::MotorRM*>(motor)->isVoltageControl() ? 1 : 0;
}

// 获取电机状态 (0=DISCONNECTED, 1=IDLE, 2=RUNNING)
int16_t motor_rm_c_get_state(MotorRM_T *motor)
{
    if (!motor) return 0;
    return static_cast<int16_t>(reinterpret_cast<motor::MotorRM*>(motor)->state_);
}

} // extern "C"
