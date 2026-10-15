#include "app_init.h"
#include "app_oled.h"

static volatile App_Status_t app_status = APP_NOT_INITIALIZED;

bool App_Init(void)
{
    /* 顶层只安排各个 APP 的初始化，不拥有任何具体设备实例。 */
    if (app_status != APP_NOT_INITIALIZED) {
        return app_status == APP_READY;
    }

    if (!App_OLED_Init()) {
        app_status = APP_OLED_INIT_FAILED;
        return false;
    }

    app_status = APP_READY;
    return true;
}

void App_Run(uint32_t now_ms)
{
    if (app_status == APP_READY && !App_OLED_Update(now_ms)) {
        app_status = APP_OLED_REFRESH_FAILED;
    }
}

App_Status_t App_Get_Status(void)
{
    return app_status;
}
