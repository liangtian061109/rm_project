#include "bsp_spi.h"
#include <stdbool.h>
#include <stddef.h>

static SPI_Instance spi_instances[SPI_MAX_INSTANCES];
static uint8_t spi_instance_count;

SPI_Instance *SPI_Register(const SPI_Init_Config_s *conf)
{
  if (conf == NULL || conf->handle == NULL || conf->handle->Instance == NULL ||
      conf->cs_port == NULL || conf->cs_pin == 0 || conf->timeout_ms == 0 ||
      conf->timeout_ms == HAL_MAX_DELAY ||
      spi_instance_count >= SPI_MAX_INSTANCES) {
    return NULL;
  }

  for (uint8_t i = 0; i < spi_instance_count; i++) {
    if (spi_instances[i].handle == conf->handle &&
        spi_instances[i].cs_port == conf->cs_port &&
        spi_instances[i].cs_pin == conf->cs_pin) {
      return NULL;
    }
  }

  /* 先完整写入候选槽位，最后提交计数，失败路径不会占用实例。 */
  const uint8_t instance_index = spi_instance_count;
  SPI_Instance *spi = &spi_instances[instance_index];
  spi->handle = conf->handle;
  spi->cs_port = conf->cs_port;
  spi->cs_pin = conf->cs_pin;
  spi->busy = 0;
  spi->timeout_ms = conf->timeout_ms;
  spi->last_status = HAL_OK;
  spi_instance_count = (uint8_t)(instance_index + 1);
  return spi;
}

/* 片选低电平选中。本模块假定低有效,与 CubeMX 里 CS 上电默认高一致。 */
static void SPI_Cs_Select(const SPI_Instance *spi)
{
  HAL_GPIO_WritePin(spi->cs_port, spi->cs_pin, GPIO_PIN_RESET);
}

static void SPI_Cs_Release(const SPI_Instance *spi)
{
  HAL_GPIO_WritePin(spi->cs_port, spi->cs_pin, GPIO_PIN_SET);
}

/* 参数和重入检查;通过后占用实例,调用方必须在返回前释放 busy。
 * busy 只是防重入标志,不是 RTOS 互斥量:多任务共享一条总线时要用锁保护。
 */
static bool SPI_Begin(SPI_Instance *spi)
{
  if (spi == NULL || spi->handle == NULL || spi->handle->Instance == NULL ||
      spi->cs_port == NULL || spi->busy) {
    return false;
  }
  spi->busy = 1;
  return true;
}

HAL_StatusTypeDef SPI_Transmit(SPI_Instance *spi, const uint8_t *data,
                               uint16_t size)
{
  if (data == NULL || size == 0 || !SPI_Begin(spi)) {
    return HAL_ERROR;
  }

  SPI_Cs_Select(spi);
  spi->last_status = HAL_SPI_Transmit(spi->handle, data, size, spi->timeout_ms);
  SPI_Cs_Release(spi);

  spi->busy = 0;
  return spi->last_status;
}

HAL_StatusTypeDef SPI_Receive(SPI_Instance *spi, uint8_t *data, uint16_t size)
{
  if (data == NULL || size == 0 || !SPI_Begin(spi)) {
    return HAL_ERROR;
  }

  SPI_Cs_Select(spi);
  spi->last_status = HAL_SPI_Receive(spi->handle, data, size, spi->timeout_ms);
  SPI_Cs_Release(spi);

  spi->busy = 0;
  return spi->last_status;
}

HAL_StatusTypeDef SPI_TransmitReceive(SPI_Instance *spi, const uint8_t *tx,
                                      uint8_t *rx, uint16_t size)
{
  if (tx == NULL || rx == NULL || size == 0 || !SPI_Begin(spi)) {
    return HAL_ERROR;
  }

  SPI_Cs_Select(spi);
  spi->last_status =
      HAL_SPI_TransmitReceive(spi->handle, tx, rx, size, spi->timeout_ms);
  SPI_Cs_Release(spi);

  spi->busy = 0;
  return spi->last_status;
}

HAL_StatusTypeDef SPI_CheckDevice(SPI_Instance *spi, uint8_t read_id_cmd,
                                  uint8_t expect_id)
{
  if (!SPI_Begin(spi)) {
    return HAL_ERROR;
  }

  const uint8_t tx = read_id_cmd;
  uint8_t rx = 0;

  SPI_Cs_Select(spi);
  spi->last_status =
      HAL_SPI_TransmitReceive(spi->handle, &tx, &rx, 1, spi->timeout_ms);
  SPI_Cs_Release(spi);

  spi->busy = 0;

  if (spi->last_status != HAL_OK) {
    return spi->last_status; /* 通信失败,last_status 保留 HAL 的错误码 */
  }
  if (rx != expect_id) {
    return HAL_ERROR; /* 通信成功但 ID 不符,此时 last_status 仍为 HAL_OK */
  }
  return HAL_OK;
}
