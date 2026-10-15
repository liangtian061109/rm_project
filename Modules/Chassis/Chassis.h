#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  Chassis_Motor_Left_Front = 1,
  Chassis_Motor_Right_Front = 2,
  Chassis_Motor_Left_Rear = 3,
  Chassis_Motor_Right_Rear = 4
} Chassis_Motor_Id_t;

typedef struct {
  float left_front_mm_s;
  float right_front_mm_s;
  float left_rear_mm_s;
  float right_rear_mm_s;
} Chassis_Wheel_Velocity_t;


/*
 * 初始化四轮位置读取。
 */
void Chassis_Init(void);

/*
 * 放在主循环中连续调用。
 * 每次处理一台电机的CPOS回包。
 */
void Chassis_Update(void);

/*
 * 取得某个轮子相对启动位置的编码器计数。
 */
int32_t Chassis_Get_Wheel_Count(uint8_t addr);
/*
 * 计算轮子转动距离
 */
float Chassis_Get_Wheel_Distance(uint8_t addr);
float Chassis_Get_Forward_Distance(void);
float Chassis_Get_Lateral_Distance(void);
float Chassis_Get_Rotation_Rad(void);

/*
 * X 型底盘逆运动学。输入为车体速度，输出为四个轮子的线速度，单位
 * 分别为 mm/s、mm/s 和 rad/s。
 */
void Chassis_Compute_Wheel_Velocities(float forward_mm_s,
                                      float lateral_mm_s,
                                      float rotation_rad_s,
                                      Chassis_Wheel_Velocity_t *wheel_velocity);

/* 将轮子线速度换算为电机速度指令，返回值单位为 RPM。 */
uint16_t Chassis_Wheel_Velocity_To_Rpm(float wheel_velocity_mm_s);

/*
 * 计算并发送四轮速度模式命令。正速度使用 dir=0，负速度使用 dir=1。
 * 当 synchronous 为 true 时，发送完四条命令后触发多机同步启动。
 */
void Chassis_Set_Velocity(float forward_mm_s,
                          float lateral_mm_s,
                          float rotation_rad_s,
                          uint8_t acc,
                          bool synchronous);
#endif
