/*
 * pid_controller.cpp
 * PID 控制器全局实例和 C 桥接
 *
 * 模板定义在 pid_controller.h 中（必须放在头文件，ARM GCC 需要）。
 * 本文件只实例化具体类型并导出 C 接口。
 */

#include "pid_controller.h"
#include "motor_rm.h"

// ============================================================
// 全局控制器实例
// 参数参考 dart_mcu，需根据实际机械结构调试
// pid_controller(kp, ki, kd, sum_max, p_max, i_max, output_max)
// ============================================================

// GM6020：电压控制，输出范围约 ±6000（内部单位）
pid_angle_velocity_controller<double> g_gm6020_ctrl(
    pid_controller<double>(10.0, 0.2, 0.01, 10000, 5000, 5000, 6000),   // 速度环
    pid_controller<double>(0.1, 0.01, 0.01, 100, 10000, 10000, 7000),   // 角度环
    nullptr, VELOCITY_CONTROL);

// M2006：电流控制，输出范围 ±10000
pid_angle_velocity_controller<double> g_m2006_ctrl(
    pid_controller<double>(10.0, 0.2, 0.01, 10000, 5000, 5000, 10000),
    pid_controller<double>(0.1, 0.01, 0.01, 100, 10000, 10000, 10000),
    nullptr, VELOCITY_CONTROL);

// M3508 #1 (ID=2)
pid_angle_velocity_controller<double> g_m3508_2_ctrl(
    pid_controller<double>(25.0, 0.1, 0.01, 100000, 10000, 6000, 16384),
    pid_controller<double>(0.1, 0.01, 0.01, 100, 100, 100, 1000),
    nullptr, VELOCITY_CONTROL);

// M3508 #2 (ID=3)
pid_angle_velocity_controller<double> g_m3508_3_ctrl(
    pid_controller<double>(25.0, 0.1, 0.01, 100000, 10000, 6000, 16384),
    pid_controller<double>(0.1, 0.01, 0.01, 100, 100, 100, 1000),
    nullptr, VELOCITY_CONTROL);

// ============================================================
// C 桥接实现
// ============================================================
extern "C" {

// 外部声明（motor_rm.cpp 中定义的全局实例）
extern motor::MotorRM g_motor_gm6020;
extern motor::MotorRM g_motor_m2006;
extern motor::MotorRM g_motor_m3508_2;
extern motor::MotorRM g_motor_m3508_3;

void pid_controller_init_all(void)
{
    g_gm6020_ctrl.motor_   = &g_motor_gm6020;
    g_m2006_ctrl.motor_    = &g_motor_m2006;
    g_m3508_2_ctrl.motor_  = &g_motor_m3508_2;
    g_m3508_3_ctrl.motor_  = &g_motor_m3508_3;
}

int16_t pid_controller_update_gm6020(void) {
    return (int16_t)g_gm6020_ctrl.update();
}

int16_t pid_controller_update_m2006(void) {
    return (int16_t)g_m2006_ctrl.update();
}

int16_t pid_controller_update_m3508_2(void) {
    return (int16_t)g_m3508_2_ctrl.update();
}

int16_t pid_controller_update_m3508_3(void) {
    return (int16_t)g_m3508_3_ctrl.update();
}

void pid_controller_set_target_gm6020(int16_t target) {
    g_gm6020_ctrl.target_velocity_ = target;
}

void pid_controller_set_target_m2006(int16_t target) {
    g_m2006_ctrl.target_velocity_ = target;
}

void pid_controller_set_target_m3508_2(int16_t target) {
    g_m3508_2_ctrl.target_velocity_ = target;
}

void pid_controller_set_target_m3508_3(int16_t target) {
    g_m3508_3_ctrl.target_velocity_ = target;
}

} // extern "C"