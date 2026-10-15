#ifndef BSP_IIC_H
#define BSP_IIC_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define IIC_MAX_INSTANCES 4

/* 一个实例代表一个 I2C 设备；多个设备可以共享同一硬件总线。 */
typedef struct {
    I2C_HandleTypeDef *handle;
    uint8_t dev_addr;             /* 7 位地址，例如 0x3C；应用不需要左移。 */
    uint32_t timeout_ms;
    HAL_StatusTypeDef last_status;
} IIC_Instance;

typedef struct {
    I2C_HandleTypeDef *handle;
    uint8_t dev_addr;
    uint32_t timeout_ms;
} IIC_Init_Config_s;

/* 启动时调用一次。参数无效、设备地址重复或实例池已满时返回 NULL。
 * 注册仅保存配置，不启动传输；不要在运行中反复注册。
 */
IIC_Instance *IIC_Register(const IIC_Init_Config_s *conf);

/* 以下接口均为同步阻塞接口，仅在主循环/任务中调用，不用于中断。
 * 返回 HAL_OK/HAL_ERROR/HAL_BUSY/HAL_TIMEOUT，并记录到 last_status。
 * 缓冲区在函数返回后即可复用，因此允许使用栈上的数组。
 */
HAL_StatusTypeDef IIC_Is_Ready(IIC_Instance *iic);
HAL_StatusTypeDef IIC_Transmit(IIC_Instance *iic, const uint8_t *data, uint16_t size);
HAL_StatusTypeDef IIC_Receive(IIC_Instance *iic, uint8_t *data, uint16_t size);

#endif
