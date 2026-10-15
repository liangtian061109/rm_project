#ifndef APP_OLED_H
#define APP_OLED_H

#include <stdbool.h>
#include <stdint.h>

/* OLED APP 自己完成硬件配置、模块注册、设备初始化和首帧显示。 */
bool App_OLED_Init(void);
/* 每隔 500 ms 更新计数和进度条；返回 false 表示刷新失败。 */
bool App_OLED_Update(uint32_t now_ms);

#endif
