#include "bsp_uart.h"
#include <stdint.h>
#include <string.h>
/*静态数组存储uart实例*/
#define Max_UART_Instance 4
static UART_Instance UART_Instances[Max_UART_Instance];
static uint8_t UART_Instance_count = 0;

UART_Instance *UART_Register(UART_Init_Config_s *conf)
{
    
   // 检查是否达到最大实例数
  if (UART_Instance_count >= Max_UART_Instance)
    {
      return NULL;
    }
   // 初始化新的UART实例
   UART_Instance *New_Instance = &UART_Instances[UART_Instance_count];
   // 清零实例内存
   memset(New_Instance, 0, sizeof(UART_Instance));
   // 存储配置参数
   New_Instance->handle = conf->handle;
   New_Instance->rx_callback = conf->rx_callback; // 设置接收回调函数
   New_Instance->id = conf->id;                   // 设置用户数据指针
    // // 将新实例添加到数组中
    UART_Instance_count++;
    return New_Instance;
    
}
void UART_Transmit(UART_Instance *uart, uint8_t *data, uint16_t size) {
  HAL_UART_Transmit(uart->handle, data, size, 100);
}
void UART_Receive(UART_Instance *uart) {
  HAL_UARTEx_ReceiveToIdle_DMA(uart->handle, uart->rx_buffer,
                               sizeof(uart->rx_buffer));
  __HAL_DMA_DISABLE_IT(uart->handle->hdmarx, DMA_IT_HT); // 禁半传输中断
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
  for (uint8_t i = 0; i < UART_Instance_count; i++) {
    if (UART_Instances[i].handle == huart) {
      UART_Instances[i].rx_size = size;
      if (UART_Instances[i].rx_callback != NULL) {
        UART_Instances[i].rx_callback(&UART_Instances[i], size);
      }
      UART_Receive(&UART_Instances[i]);
      return;
    }
  }
}