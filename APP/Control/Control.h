#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float kp;
    float ki;
    float kd;

    float max_speed;
    float min_speed;
    float base_speed;

    float integral;
    float previous_error;
    float integral_limit;
} Chassis_PID_t;

typedef struct {
  Chassis_PID_t forward_pid;
  Chassis_PID_t lateral_pid;

  bool active;
  bool finished;

  float target_forward_mm;
  float target_lateral_mm;

  float start_forward_mm;
  float start_lateral_mm;

  uint32_t start_tick;
  uint32_t last_update_tick;

  uint32_t timeout_ms;
} Chassis_Move_Controller_t;

void Control_Init(void);

void Control_Move_Start(float forward_mm, float lateral_mm);

void Control_Move_Update(void);

void Control_Move_Stop(void);

bool Control_Move_Active(void);

bool Control_Move_Finished(void);

#endif