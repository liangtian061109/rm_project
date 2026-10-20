#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "main.h"
#include <stdint.h>

#define CAN_MAX_INSTANCES 8
#define CAN1_FILTER_BANK_CNT 14

typedef struct CAN_S {
  CAN_HandleTypeDef *handle;
  uint32_t rx_id;
  uint32_t rx_ide;
  uint8_t filter_bank;
  CAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];
  uint8_t rx_len;
  uint32_t rx_count;
  uint32_t last_rx_tick;
  void (*rx_callback)(struct CAN_S *can);
  void *id;
} CAN_Instance;

typedef struct {
  CAN_HandleTypeDef *handle;
  uint32_t rx_id;
  uint32_t rx_ide;
  void (*rx_callback)(struct CAN_S *can);
  void *id;
} CAN_Init_Config_s;

#endif