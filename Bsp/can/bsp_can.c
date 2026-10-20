#include "bsp_can.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal_can.h"
#include "stm32f4xx_hal_def.h"
#include <stdint.h>
#include <string.h>

static CAN_Instance can_instances[CAN_MAX_INSTANCES];
static uint8_t can_instance_count;

static uint32_t CAN_FilterWord(uint32_t id, uint32_t ide) {
  if (ide == CAN_ID_STD)
    return (id & 0x7FFU) << 21;

  return (((id >> 18) & 0x7FFU) << 21) | (((id >> 13) & 0x1FU) << 16) |
         ((id & 0x1FFFU) << 3) | 0x0004U;
}

static uint8_t CAN_NextFilterBank(CAN_HandleTypeDef *handle)
{
  uint8_t bank = (handle->Instance == CAN2) ? 14 : 0;
  uint8_t last = (handle->Instance == CAN2) ? 28 : CAN1_FILTER_BANK_CNT;

  for(;bank < last; bank++)
  {
    uint32_t filter_bank_used = 0;
    for(uint8_t i = 0; i < can_instance_count; i++)
    {
      if(can_instances[i].handle == handle && can_instances[i].filter_bank == bank)
      {
        filter_bank_used = 1;
        break;
      }
    }
    if(filter_bank_used == 0)
    {
      return bank;
    }
  }
  return 0xFF;
}
static HAL_StatusTypeDef CAN_AddFilter(CAN_HandleTypeDef *handle,
                                       uint32_t rx_id, uint32_t rx_ide,
                                       uint8_t bank) 
{
  CAN_FilterTypeDef filter = {0};


  const uint32_t word = CAN_FilterWord(rx_id, rx_ide);

  filter.FilterBank = bank;
  filter.FilterMode = CAN_FILTERMODE_IDLIST;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = (uint16_t)(word >> 16);
  filter.FilterIdLow = (uint16_t)(word & 0xFFFF);
  filter.FilterMaskIdHigh = filter.FilterIdHigh;
  filter.FilterMaskIdLow = filter.FilterIdLow;
  filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = CAN1_FILTER_BANK_CNT;

  if (HAL_CAN_ConfigFilter(handle, &filter) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef *handle) {
  HAL_CAN_StateTypeDef state;

  if (handle == NULL || handle->Instance == NULL)
    return HAL_ERROR;

  state = HAL_CAN_GetState(handle);

  if (state == HAL_CAN_STATE_READY) {
    /* 只有 READY 才能成功 Start */
    if (HAL_CAN_Start(handle) != HAL_OK)
      return HAL_ERROR;
  } else if (state != HAL_CAN_STATE_LISTENING) {
    /* RESET / SLEEP_* / BUSY / TIMEOUT / ERROR 等异常状态, 拒绝启动 */
    return HAL_ERROR;
  }

  /* 激活 FIFO0 接收中断 —— 与过滤器、回调的 FIFO 保持一致 */
  if (HAL_CAN_ActivateNotification(handle, CAN_IT_RX_FIFO0_MSG_PENDING) !=
      HAL_OK)
    return HAL_ERROR;

  return HAL_OK;
}

CAN_Instance *CAN_Register(CAN_Init_Config_s *conf)
{
  /* 1) 五道校验 */
  if (conf == NULL || conf->handle == NULL || conf->handle->Instance == NULL ||
      conf->rx_callback == NULL || can_instance_count >= CAN_MAX_INSTANCES)
    return NULL;
  if (conf->rx_ide == CAN_ID_STD && conf->rx_id > 0x7FF)
    return NULL;
  if (conf->rx_ide == CAN_ID_EXT && conf->rx_id > 0x1FFFFFFF)
    return NULL;

  /* 2) 重复注册*/
  for (uint8_t i = 0; i < can_instance_count; i++)
  {
    if(can_instances[i].handle == conf->handle&&can_instances[i].rx_id == conf->rx_id&&can_instances[i].rx_ide == conf->rx_ide)
    {
      return NULL;
    }
  }

  /* 3) 写候选槽 */
  const uint8_t index = can_instance_count;
  CAN_Instance *can = &can_instances[index];
  memset(can, 0, sizeof(CAN_Instance));
  can->handle = conf->handle;
  can->rx_id = conf->rx_id;
  can->rx_ide = conf->rx_ide;
  can->rx_callback = conf->rx_callback;
  can->id = conf->id;

  /* 4) 分配组号 + 配过滤器, 任一步失败就清掉候选槽 */
  const uint8_t bank = CAN_NextFilterBank(conf->handle);
  if(bank == 0xFF || CAN_AddFilter(conf->handle, conf->rx_id, conf->rx_ide, bank) != HAL_OK)
  {
    memset(can, 0, sizeof(CAN_Instance));
    return NULL;
  }
  can->filter_bank = bank;

  can_instance_count = (uint8_t)(index + 1);

  return can;
}

HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef *handle, uint32_t id,
                               uint32_t ide, const uint8_t *data, uint8_t len)
{
  if (handle == NULL || handle->Instance == NULL || data == NULL)
    return HAL_ERROR;
  if (len == 0 || len > 8)
    return HAL_ERROR;
  if (ide == CAN_ID_STD && id > 0x7FF)
    return HAL_ERROR;
  if (ide == CAN_ID_EXT && id > 0x1FFFFFFF)
    return HAL_ERROR;

  if (HAL_CAN_GetTxMailboxesFreeLevel(handle) == 0)
    return HAL_BUSY;

  CAN_TxHeaderTypeDef tx = {0};
  if (ide == CAN_ID_STD)
    tx.StdId = id;
  else
    tx.ExtId = id;
  tx.IDE = ide;
  tx.RTR = CAN_RTR_DATA;
  tx.DLC = len;
  tx.TransmitGlobalTime = DISABLE;

  uint32_t mailbox;
  return HAL_CAN_AddTxMessage(handle, &tx, data, &mailbox);
}

static void CAN_RxDispatch(CAN_HandleTypeDef *hcan, uint32_t fifo) {
  CAN_RxHeaderTypeDef hdr;
  uint8_t data[8];

  while (HAL_CAN_GetRxFifoFillLevel(hcan, fifo) >
         0U) /* 读空, 一次中断常有多帧 */
  {
    if (HAL_CAN_GetRxMessage(hcan, fifo, &hdr, data) != HAL_OK)
      break;

    const uint32_t id = (hdr.IDE == CAN_ID_STD) ? hdr.StdId : hdr.ExtId;
    const uint8_t dlc = (hdr.DLC > 8U) ? 8U : (uint8_t)hdr.DLC;

    for (uint8_t i = 0; i < can_instance_count; i++) {
      CAN_Instance *can = &can_instances[i];
      if (can->handle != hcan)
        continue;
      if (can->rx_ide != hdr.IDE || can->rx_id != id)
        continue;

      memcpy(can->rx_data, data, dlc);
      can->rx_len = dlc;
      can->rx_header = hdr;
      can->rx_count++;
      can->last_rx_tick = HAL_GetTick();
      if (can->rx_callback != NULL)
        can->rx_callback(can);
    }
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  CAN_RxDispatch(hcan, CAN_RX_FIFO0);
}
