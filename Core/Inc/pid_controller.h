#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void pid_controller_init_all(void);
int16_t pid_controller_update_gm6020(void);
int16_t pid_controller_update_m2006(void);
int16_t pid_controller_update_m3508_2(void);
int16_t pid_controller_update_m3508_3(void);
void    pid_controller_set_target_gm6020(int16_t target);
void    pid_controller_set_target_m2006(int16_t target);
void    pid_controller_set_target_m3508_2(int16_t target);
void    pid_controller_set_target_m3508_3(int16_t target);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include "motor_rm.h"

#define LIMIT_MIN_MAX(x, min, max) \
    ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// 单级 PID 控制器模板
template <typename T>
struct pid_controller
{
    T kp, ki, kd;
    T cur_error, last_error, sum_error;
    T sum_error_max;
    T p_max, i_max, output_max;
    T output;
    T target;

    pid_controller(T kp_, T ki_, T kd_, T sum_error_max_,
                   T p_max_, T i_max_, T output_max_)
        : kp(kp_), ki(ki_), kd(kd_), sum_error_max(sum_error_max_)
        , p_max(p_max_), i_max(i_max_), output_max(output_max_)
        , cur_error(0), last_error(0), sum_error(0), output(0), target(0)
    {}

    T update(T current)
    {
        last_error = cur_error;
        cur_error = target - current;
        sum_error = LIMIT_MIN_MAX(sum_error + cur_error,
                                  -sum_error_max, sum_error_max);

        output = LIMIT_MIN_MAX((kp * cur_error), -p_max, p_max)
               + LIMIT_MIN_MAX((ki * sum_error), -i_max, i_max)
               + kd * (cur_error - last_error);

        output = LIMIT_MIN_MAX(output, -output_max, output_max);
        return output;
    }

    void reset()
    {
        cur_error = 0;
        last_error = 0;
        sum_error = 0;
        output = 0;
    }
};

// PID 控制器模式枚举
enum E_PID_Controller_State
{
    VELOCITY_CONTROL,
    ANGLE_CONTROL,
    OPEN_LOOP
};

// 串级角度-速度 PID 控制器模板
template <typename T>
struct pid_angle_velocity_controller
{
    pid_controller<T> pid_velocity_;
    pid_controller<T> pid_angle_;

    int32_t target_angle_with_rounds_;
    int32_t current_angle_with_rounds_;

    int16_t target_velocity_;
    int16_t current_velocity_;

    int16_t target_openloop_;

    motor::MotorRM *motor_;
    int state_;

    pid_angle_velocity_controller(pid_controller<T> pid_velocity,
                                  pid_controller<T> pid_angle,
                                  motor::MotorRM *motor,
                                  int state)
        : pid_velocity_(pid_velocity), pid_angle_(pid_angle)
        , target_angle_with_rounds_(0), current_angle_with_rounds_(0)
        , target_velocity_(0), current_velocity_(0)
        , target_openloop_(0)
        , motor_(motor), state_(state)
    {}

    T update()
    {
        if (state_ == VELOCITY_CONTROL)
        {
            current_angle_with_rounds_ =
                motor_->current_round_ * 8192 + motor_->current_angle_;
            current_velocity_ = motor_->current_velocity_ * 0.6
                              + current_velocity_ * 0.4;
            pid_velocity_.target = target_velocity_;
            return pid_velocity_.update(current_velocity_);
        }
        else if (state_ == ANGLE_CONTROL)
        {
            LIMIT_MIN_MAX(target_angle_with_rounds_, 0, 370000);
            current_angle_with_rounds_ =
                motor_->current_round_ * 8192 + motor_->current_angle_;
            current_velocity_ = motor_->current_velocity_ * 0.6
                              + current_velocity_ * 0.4;
            pid_angle_.target = target_angle_with_rounds_;
            pid_velocity_.target =
                pid_angle_.update(current_angle_with_rounds_);
            return pid_velocity_.update(current_velocity_);
        }
        else // OPEN_LOOP
        {
            return target_openloop_;
        }
    }

    void set_state(int state)
    {
        if (state_ != state)
        {
            state_ = state;
            reset();
        }
    }

    void reset()
    {
        pid_velocity_.reset();
        pid_angle_.reset();
    }
};

// 全局控制器实例声明
extern pid_angle_velocity_controller<double> g_gm6020_ctrl;
extern pid_angle_velocity_controller<double> g_m2006_ctrl;
extern pid_angle_velocity_controller<double> g_m3508_2_ctrl;
extern pid_angle_velocity_controller<double> g_m3508_3_ctrl;

#endif // __cplusplus

#endif // PID_CONTROLLER_H