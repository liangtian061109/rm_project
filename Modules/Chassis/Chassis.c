#include "Chassis.h"
#include "Emm_V5.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
  int32_t Origin_Position;
  int32_t Current_Position;
  int32_t Relative_Count;
  uint8_t Initialized;
} Chassis_Wheel_Position_t;

#define CHASSIS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define CHASSIS_PI 3.1415926f
#define CHASSIS_WHEEL_DIAMETER_MM 85.0f
#define CHASSIS_COUNTS_PER_REVOLUTION 65536.0f
#define CHASSIS_SQRT_2 1.41421356f
#define CHASSIS_WHEEL_BASE_MM 250.0f
#define CHASSIS_TRACK_WIDTH_MM 250.0f
#define CHASSIS_MAX_RPM 5000.0f

static Chassis_Wheel_Position_t wheel_position[5] = {0};
static uint8_t current_motor = 1;
static float chassis_forward_distance_mm = 0.0f;
static float chassis_lateral_distance_mm = 0.0f;
static float chassis_rotation_rad = 0.0f;

static void Chassis_Odometry_Update(void);

static float Chassis_Abs(float value)
{
  return (value < 0.0f) ? -value : value;
}

void Chassis_Init(void)
{
  uint8_t addr;
  for (addr = 1; addr <= 4; addr++)
  {
    wheel_position[addr].Origin_Position = 0;
    wheel_position[addr].Current_Position = 0;
    wheel_position[addr].Relative_Count = 0;
    wheel_position[addr].Initialized = 0;

    Emm_V5_Clear_Position_Status(addr);
  }

  current_motor = 1;
  chassis_forward_distance_mm = 0.0f;
  chassis_lateral_distance_mm = 0.0f;
  chassis_rotation_rad = 0.0f;
  Emm_V5_Read_Sys_Params(current_motor, S_CPOS);
}

void Chassis_Update(void)
{
  int32_t position;

  /*
   * 当前电机还没有返回新数据。
   * 直接退出，等待下一次主循环。
   */
  if (!Emm_V5_Position_Updated(current_motor)) {
    return;
  }

  /*
   * 取出新位置，然后清除更新标志。
   */
  position = Emm_V5_Get_Position(current_motor);
  Emm_V5_Clear_Position_Status(current_motor);

  wheel_position[current_motor].Current_Position = position;

  /*
   * 第一次收到该电机的位置时，
   * 把它保存为起点。
   */
  if (!wheel_position[current_motor].Initialized)
    {
      wheel_position[current_motor].Origin_Position = position;
      wheel_position[current_motor].Relative_Count = 0;
      wheel_position[current_motor].Initialized = 1;
  }
  else
    {
       /*
        * CPOS是多圈累计位置，所以直接减去起点。
       */
       wheel_position[current_motor].Relative_Count =
           position - wheel_position[current_motor].Origin_Position;

  }

  if (current_motor == Chassis_Motor_Right_Rear)
  {
    Chassis_Odometry_Update();
  }

  /*
   * 切换到下一台电机。
   */
  current_motor++;

  if (current_motor > 4) {
    current_motor = 1;
  }
  /*
   * 发送下一台电机的位置读取请求。
   */
  Emm_V5_Read_Sys_Params(current_motor, S_CPOS);
}

int32_t Chassis_Get_Wheel_Count(uint8_t addr)
{
  if (addr < 1 || addr > 4)
    {
      return 0;
    }

  if(!wheel_position[addr].Initialized)
  {
    return 0;
  }

  return wheel_position[addr].Relative_Count;
}

static float Chassis_Count_To_Mm(int32_t count)
{
    float revolution;
    float circumference;

    revolution =
        (float)count / CHASSIS_COUNTS_PER_REVOLUTION;

    circumference =
        CHASSIS_PI * CHASSIS_WHEEL_DIAMETER_MM;

    return revolution * circumference;
}

float Chassis_Get_Wheel_Distance(uint8_t addr)
{
    int32_t count;

    count = Chassis_Get_Wheel_Count(addr);

    return Chassis_Count_To_Mm(count);
}

static void Chassis_Odometry_Update(void)
{
  float left_front_mm;
  float right_front_mm;
  float left_rear_mm;
  float right_rear_mm;
  float rotation_radius_mm;

  left_front_mm =
      Chassis_Get_Wheel_Distance(Chassis_Motor_Left_Front);
  right_front_mm =
      Chassis_Get_Wheel_Distance(Chassis_Motor_Right_Front);
  left_rear_mm =
      Chassis_Get_Wheel_Distance(Chassis_Motor_Left_Rear);
  right_rear_mm =
      Chassis_Get_Wheel_Distance(Chassis_Motor_Right_Rear);

  rotation_radius_mm =
      (CHASSIS_WHEEL_BASE_MM + CHASSIS_TRACK_WIDTH_MM) / 2.0f;

  chassis_forward_distance_mm =
      (left_front_mm + right_front_mm +
       left_rear_mm + right_rear_mm) /
      (2.0f * CHASSIS_SQRT_2);

  chassis_lateral_distance_mm =
      (-left_front_mm + right_front_mm +
       left_rear_mm - right_rear_mm) /
      (2.0f * CHASSIS_SQRT_2);

  chassis_rotation_rad =
      (-left_front_mm + right_front_mm -
       left_rear_mm + right_rear_mm) /
      (2.0f * CHASSIS_SQRT_2 * rotation_radius_mm);
}

float Chassis_Get_Forward_Distance(void)
{
  return chassis_forward_distance_mm;
}

float Chassis_Get_Lateral_Distance(void)
{
  return chassis_lateral_distance_mm;
}

float Chassis_Get_Rotation_Rad(void)
{
  return chassis_rotation_rad;
}

void Chassis_Compute_Wheel_Velocities(float forward_mm_s, float lateral_mm_s,
                                      float rotation_rad_s,
                                      Chassis_Wheel_Velocity_t *wheel_velocity)
{

  float rotation_velocity_mm_s;

  if (!wheel_velocity)
  {
    return;
  }

  rotation_velocity_mm_s =
      rotation_rad_s * (CHASSIS_WHEEL_BASE_MM + CHASSIS_TRACK_WIDTH_MM) / 2.0f;

  wheel_velocity->left_front_mm_s =
      (forward_mm_s - lateral_mm_s - rotation_velocity_mm_s) / CHASSIS_SQRT_2;

  wheel_velocity->right_front_mm_s =
      (forward_mm_s + lateral_mm_s + rotation_velocity_mm_s) / CHASSIS_SQRT_2;

  wheel_velocity->left_rear_mm_s =
      (forward_mm_s + lateral_mm_s - rotation_velocity_mm_s) / CHASSIS_SQRT_2;

  wheel_velocity->right_rear_mm_s =
      (forward_mm_s - lateral_mm_s + rotation_velocity_mm_s) / CHASSIS_SQRT_2;
}

uint16_t Chassis_Wheel_Velocity_To_Rpm(float wheel_velocity_mm_s) {
  float circumference_mm;
  float rpm;

  circumference_mm = CHASSIS_PI * CHASSIS_WHEEL_DIAMETER_MM;

  rpm = Chassis_Abs(wheel_velocity_mm_s) * 60.0f / circumference_mm;

  return (uint16_t)(rpm + 0.5f);
}

void Chassis_Set_Velocity(float forward_mm_s, float lateral_mm_s,
                          float rotation_rad_s, uint8_t acc, bool synchronous)
  {
   
    Chassis_Wheel_Velocity_t wheel_velocity;
    float max_velocity_mm_s;
    float max_allowed_velocity_mm_s;
    float scale;
    uint8_t dir;
    uint16_t rpm;

    (void)synchronous;

    Chassis_Compute_Wheel_Velocities(forward_mm_s, lateral_mm_s, rotation_rad_s,
                                     &wheel_velocity);

    float lf = Chassis_Abs(wheel_velocity.left_front_mm_s);
    float rf = Chassis_Abs(wheel_velocity.right_front_mm_s);
    float lr = Chassis_Abs(wheel_velocity.left_rear_mm_s);
    float rr = Chassis_Abs(wheel_velocity.right_rear_mm_s);

    max_velocity_mm_s = CHASSIS_MAX(CHASSIS_MAX(lf, rf), CHASSIS_MAX(lr, rr));

    max_allowed_velocity_mm_s =
        CHASSIS_MAX_RPM * CHASSIS_PI * CHASSIS_WHEEL_DIAMETER_MM / 60.0f;

    if (max_velocity_mm_s > max_allowed_velocity_mm_s) {
      scale = max_allowed_velocity_mm_s / max_velocity_mm_s;

      wheel_velocity.left_front_mm_s *= scale;
      wheel_velocity.right_front_mm_s *= scale;
      wheel_velocity.left_rear_mm_s *= scale;
      wheel_velocity.right_rear_mm_s *= scale;
    }

    /* Load all four velocity commands into the Y42 multi-motor buffer. */
    MMCL_count = 0U;

    dir = (wheel_velocity.left_front_mm_s < 0.0f) ? 1 : 0;
    rpm = Chassis_Wheel_Velocity_To_Rpm(wheel_velocity.left_front_mm_s);
    Emm_V5_MMCL_Vel_Control(Chassis_Motor_Left_Front,
                            dir, rpm, acc, false);

    dir = (wheel_velocity.right_front_mm_s < 0.0f) ? 1 : 0;
    rpm = Chassis_Wheel_Velocity_To_Rpm(wheel_velocity.right_front_mm_s);
    Emm_V5_MMCL_Vel_Control(Chassis_Motor_Right_Front,
                            dir, rpm, acc, false);

    dir = (wheel_velocity.left_rear_mm_s < 0.0f) ? 1 : 0;
    rpm = Chassis_Wheel_Velocity_To_Rpm(wheel_velocity.left_rear_mm_s);
    Emm_V5_MMCL_Vel_Control(Chassis_Motor_Left_Rear,
                            dir, rpm, acc, false);

    dir = (wheel_velocity.right_rear_mm_s < 0.0f) ? 1 : 0;
    rpm = Chassis_Wheel_Velocity_To_Rpm(wheel_velocity.right_rear_mm_s);
    Emm_V5_MMCL_Vel_Control(Chassis_Motor_Right_Rear,
                            dir, rpm, acc, false);

    /* Broadcast the complete multi-motor command once. b*/
    Emm_V5_Multi_Motor_Cmd(0);
  }
