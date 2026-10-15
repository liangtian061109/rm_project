#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SPI_MAX_INSTANCES 4

/* 一个实例代表一个 SPI 从设备；多个设备可以共享同一硬件总线，靠各自的片选区分。 */
typedef struct
{
  SPI_HandleTypeDef *handle; /* SPI 硬件句柄 */
  GPIO_TypeDef *cs_port;     /* 片选 GPIO 端口 */
  uint16_t cs_pin;           /* 片选 GPIO 引脚 */

  volatile uint8_t busy; /* 重入保护:传输期间置 1 */

  uint32_t timeout_ms;
  HAL_StatusTypeDef last_status;
} SPI_Instance;

typedef struct
{
  SPI_HandleTypeDef *handle; /* SPI 硬件句柄 */
  GPIO_TypeDef *cs_port;     /* 片选 GPIO 端口 */
  uint16_t cs_pin;           /* 片选 GPIO 引脚 */
  uint32_t timeout_ms;
} SPI_Init_Config_s;

/* 启动时调用一次。参数无效、同一句柄+同一片选重复注册或实例池已满时返回 NULL。 */
SPI_Instance *SPI_Register(const SPI_Init_Config_s *conf);

/* 以下接口均为同步阻塞接口，仅在主循环/任务中调用，不用于中断。
 * 片选由 BSP 内部拉低/拉高，一次调用期间保持有效，调用方不需要操作 CS。
 * 返回 HAL_OK/HAL_ERROR/HAL_BUSY/HAL_TIMEOUT,并记录到 last_status。
 * 缓冲区在函数返回后即可复用，因此允许使用栈上的数组。
 */
HAL_StatusTypeDef SPI_Transmit(SPI_Instance *spi, const uint8_t *data, uint16_t size);
HAL_StatusTypeDef SPI_Receive(SPI_Instance *spi, uint8_t *data, uint16_t size);

/* 全双工收发:读寄存器时"先写命令再读数据"必须在同一次片选内完成,
 * 因此单独提供这个接口,而不是让调用方先 Transmit 再 Receive。
 * 两个缓冲区都必须非空(HAL 要求);只写用 Transmit,只读用 Receive。
 */
HAL_StatusTypeDef SPI_TransmitReceive(SPI_Instance *spi, const uint8_t *tx,
                                      uint8_t *rx, uint16_t size);

/* 发一个命令字节并读回一个 ID 字节,与 expect_id 比较。
 * 通信失败和 ID 不符都返回非 HAL_OK,可看 last_status 区分:
 * last_status == HAL_OK 表示通信成功、只是 ID 不对。
 */
HAL_StatusTypeDef SPI_CheckDevice(SPI_Instance *spi, uint8_t read_id_cmd,
                                  uint8_t expect_id);

#endif /* BSP_SPI_H */
