#ifndef BSP_UART_H
#define BSP_UART_H

#include "main.h"
#include <stdint.h>

#define UART_RX_BUF_SIZE 64   // 接收缓冲区大小
#define UART_TX_BUF_SIZE 64   // 发送缓冲区大小(DMA 发送用)

typedef struct Uart_s
{
         UART_HandleTypeDef *handle;
         uint8_t rx_buffer[64];
         uint16_t rx_size;
         uint8_t tx_buffer[64];
         uint16_t tx_size;
         void(*rx_callback) (struct Uart_s *uart,  uint16_t size);
         void *id;

} UART_Instance;

typedef struct
{
  
  UART_HandleTypeDef *handle;
  void(*rx_callback) (struct Uart_s *uart, uint16_t size);
  void *id;

} UART_Init_Config_s;

UART_Instance *UART_Register(UART_Init_Config_s *conf);
void UART_Transmit(UART_Instance *uart, uint8_t *data, uint16_t size);
void UART_Receive(UART_Instance *uart);

#endif