#ifndef APP_INIT_H
#define APP_INIT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_NOT_INITIALIZED,
    APP_READY,
    APP_REGISTER_FAILED,
    APP_OLED_INIT_FAILED,
    APP_OLED_REFRESH_FAILED
} App_Status_t;

/* 在所有外设 MX_xxx_Init 和屏幕上电等待之后调用一次。 */
bool App_Init(void);
/* 主循环/任务调用。传入毫秒时钟，APP 显示逻辑无需依赖 HAL_GetTick。 */
void App_Run(uint32_t now_ms);
/* 调试器可读取运行状态；通信失败时停止刷新，重新上电后再初始化。 */
App_Status_t App_Get_Status(void);

#endif
