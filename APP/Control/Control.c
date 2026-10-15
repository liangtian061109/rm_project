#include "Control.h"
#include "Chassis.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define CONTROL_PERIOD_MS 5
#define CONTROL_PERIOD_S 0.005f

#define CONTROL_TIMEOUT_MS 10000U
#define CONTROL_TIMEOUT_SCALE 3.0f
#define CONTROL_TIMEOUT_EXTRA_MS 1000U

#define FORWARD_KP 0.5f
#define LATERAL_KP 0.5f

#define FORWARD_KI 0.0f
#define LATERAL_KI 0.0f

#define FORWARD_KD 0.0f
#define LATERAL_KD 0.0f

#define FORWARD_MAX_SPEED_MM_S 100.0f
#define LATERAL_MAX_SPEED_MM_S 100.0f

#define MOVE_DISTANCE_TOLERANCE_MM 5.0f

static Chassis_Move_Controller_t move_controller;

static float Control_Abs(float value) {
  if (value < 0.0f) {
    return -value;
  }
  return value;
}

static float Control_Clamp(float value, float min_value, float max_value) {

  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }

  return value;
}

static void Control_PID_Reset(Chassis_PID_t *pid) {
  if (pid == 0) {
    return;
  }

  pid->integral = 0.0f;
  pid->previous_error = 0.0f;
}

static float Control_PID_Update(Chassis_PID_t *pid, float error, float dt) {
  float derivative;
  float output;

  if (pid == 0 || dt <= 0.0f) {
    return 0.0f;
  }

  pid->integral += error * dt;

  pid->integral =
      Control_Clamp(pid->integral, -pid->integral_limit, pid->integral_limit);

  derivative = (error - pid->previous_error) / dt;

  output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;

  pid->previous_error = error;

  output = Control_Clamp(output, pid->min_speed, pid->max_speed);

  return output;
}

void Control_Init(void) {
  /* 前进 PID 参数 */
  move_controller.forward_pid.kp = FORWARD_KP;
  move_controller.forward_pid.ki = FORWARD_KI;
  move_controller.forward_pid.kd = FORWARD_KD;

  move_controller.forward_pid.max_speed = FORWARD_MAX_SPEED_MM_S;

  move_controller.forward_pid.min_speed = -FORWARD_MAX_SPEED_MM_S;

  move_controller.forward_pid.base_speed = 0.0f;

  move_controller.forward_pid.integral_limit = 200.0f;

  /* 横移 PID 参数 */
  move_controller.lateral_pid.kp = LATERAL_KP;
  move_controller.lateral_pid.ki = LATERAL_KI;
  move_controller.lateral_pid.kd = LATERAL_KD;

  move_controller.lateral_pid.max_speed = LATERAL_MAX_SPEED_MM_S;

  move_controller.lateral_pid.min_speed = -LATERAL_MAX_SPEED_MM_S;

  move_controller.lateral_pid.base_speed = 0.0f;

  move_controller.lateral_pid.integral_limit = 200.0f;

  /* 清除两个 PID 的历史状态 */
  Control_PID_Reset(&move_controller.forward_pid);
  Control_PID_Reset(&move_controller.lateral_pid);

  /* 清除运动任务状态 */
  move_controller.active = false;
  move_controller.finished = false;

  move_controller.target_forward_mm = 0.0f;
  move_controller.target_lateral_mm = 0.0f;

  move_controller.start_forward_mm = 0.0f;
  move_controller.start_lateral_mm = 0.0f;

  move_controller.start_tick = 0U;
  move_controller.last_update_tick = 0U;
}

void Control_Move_Start(float forward_mm, float lateral_mm) {
  uint32_t now_tick;
  float forward_time_s;
  float lateral_time_s;
  float expected_time_s;

  now_tick = HAL_GetTick();

  forward_time_s = Control_Abs(forward_mm) / FORWARD_MAX_SPEED_MM_S;

  lateral_time_s = Control_Abs(lateral_mm) / LATERAL_MAX_SPEED_MM_S;

  expected_time_s = forward_time_s;

  if (lateral_time_s > expected_time_s) {
    expected_time_s = lateral_time_s;
  }

  move_controller.timeout_ms =
      (uint32_t)(expected_time_s * CONTROL_TIMEOUT_SCALE * 1000.0f) +
      CONTROL_TIMEOUT_EXTRA_MS;

  if (move_controller.timeout_ms < CONTROL_TIMEOUT_MS) {
    move_controller.timeout_ms = CONTROL_TIMEOUT_MS;
  }

  /* 保存本次运动目标 */
  move_controller.target_forward_mm = forward_mm;
  move_controller.target_lateral_mm = lateral_mm;

  /* 保存运动开始时的底盘位置 */
  move_controller.start_forward_mm = Chassis_Get_Forward_Distance();

  move_controller.start_lateral_mm = Chassis_Get_Lateral_Distance();

  /* 清除上一次运动残留的 PID 状态 */
  Control_PID_Reset(&move_controller.forward_pid);
  Control_PID_Reset(&move_controller.lateral_pid);

  /* 设置任务状态 */
  move_controller.active = true;
  move_controller.finished = false;

  /* 保存时间 */
  move_controller.start_tick = now_tick;
  move_controller.last_update_tick = now_tick;
}

void Control_Move_Update(void) {
  uint32_t now_tick;
  uint32_t elapsed_ms;

  float current_forward_mm;
  float current_lateral_mm;

  float relative_forward_mm;
  float relative_lateral_mm;

  float forward_error_mm;
  float lateral_error_mm;

  float forward_speed_mm;
  float lateral_speed_mm;

  if (!move_controller.active) {
    return;
  }
  now_tick = HAL_GetTick();

  /*判断是否超时*/
  elapsed_ms = now_tick - move_controller.start_tick;

  if (elapsed_ms > move_controller.timeout_ms) {
    Control_Move_Stop();

    move_controller.finished = false;

    return;
  }
  /* 读取当前底盘里程 */
  current_forward_mm = Chassis_Get_Forward_Distance();
  current_lateral_mm = Chassis_Get_Lateral_Distance();

  /* 计算本次任务的相对位移 */
  relative_forward_mm = current_forward_mm - move_controller.start_forward_mm;
  relative_lateral_mm = current_lateral_mm - move_controller.start_lateral_mm;

  /* 计算剩余误差 */
  forward_error_mm = move_controller.target_forward_mm - relative_forward_mm;
  lateral_error_mm = move_controller.target_lateral_mm - relative_lateral_mm;

  if (Control_Abs(forward_error_mm) <= MOVE_DISTANCE_TOLERANCE_MM &&
      Control_Abs(lateral_error_mm) <= MOVE_DISTANCE_TOLERANCE_MM) {
    Control_Move_Stop();
    move_controller.finished = true;
    return;
  }
  /* 使用固定的 5 ms 控制周期计算 PID */
  forward_speed_mm = Control_PID_Update(&move_controller.forward_pid,
                                        forward_error_mm, CONTROL_PERIOD_S);
  lateral_speed_mm = Control_PID_Update(&move_controller.lateral_pid,
                                        lateral_error_mm, CONTROL_PERIOD_S);
  /* 当前暂不加入旋转控制 */
  Chassis_Set_Velocity(forward_speed_mm, lateral_speed_mm, 0.0f, 10U, true);

  move_controller.last_update_tick = now_tick;
}

void Control_Move_Stop(void) {
  Chassis_Set_Velocity(0.0f, 0.0f, 0.0f, 0.0f, false);
  move_controller.active = false;
}

bool Control_Move_Active(void) { return move_controller.active; }

bool Control_Move_Finished(void) { return move_controller.finished; }
