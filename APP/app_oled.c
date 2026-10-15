#include "app_oled.h"
#include "i2c.h"
#include "oled.h"
#include <stddef.h>

#define APP_OLED_PERIOD_MS 500

/* 字体尺寸:本工程 OLED 模块的 size 参数,16 = 8x16 ASCII(asc2_1608)。
 * demo 用的是固定 8x16 字体,这里显式写成 16 保持一致的观感。 */
#define APP_OLED_FONT 16

/* 设备实例属于 OLED APP，不能暴露给 app_init 或其他 APP。 */
static OLED_Instance *display;
static uint32_t counter;
static uint32_t last_update_ms;
static bool clock_started;

/* 本工程 OLED_Refresh/OLED_Init 返回 void,通信结果存在实例的 last_status 里。 */
static bool App_OLED_Last_Transfer_Ok(void)
{
    return display != NULL && display->iic != NULL &&
           display->iic->last_status == HAL_OK;
}

static bool App_OLED_Draw_Frame(void)
{
    /* 注意:本工程的 OLED_Clear 内部会刷新一次,所以一帧会有两次全屏刷新。 */
    OLED_Clear(display);
    OLED_Show_String(display, 0, 0, "OLED I2C DEMO", APP_OLED_FONT);
    OLED_Show_String(display, 0, 16, "COUNT:", APP_OLED_FONT);
    OLED_Show_Num(display, 48, 16, counter, 6, APP_OLED_FONT);
    OLED_Show_String(display, 0, 32, "BSP->MODULE->APP", APP_OLED_FONT);

    OLED_Draw_Line(display, 0, 51, 127, 51);
    OLED_Draw_Line(display, 0, 63, 127, 63);
    OLED_Draw_Line(display, 0, 51, 0, 63);
    OLED_Draw_Line(display, 127, 51, 127, 63);
    uint8_t progress = (uint8_t)(counter % 126);
    for (uint8_t x = 1; x <= progress; ++x) {
        OLED_Draw_Line(display, x, 54, x, 60);
    }

    OLED_Refresh(display);
    return App_OLED_Last_Transfer_Ok();
}

bool App_OLED_Init(void)
{
    /* 板级选择属于 APP：OLED 模块本身不绑定 hi2c1 或 0x3C。 */
    OLED_Init_Config_s oled_conf = {
        .iic_conf = {
            .handle = &hi2c1,
            .dev_addr = 0x3C,
            .timeout_ms = 100
        }
    };

    /* OLED_Register 内部会调用 IIC_Register，APP 不重复注册 I2C。 */
    display = OLED_Register(&oled_conf);
    if (display == NULL) {
        return false;
    }

    OLED_Init(display);
    if (!App_OLED_Last_Transfer_Ok()) {
        display = NULL;
        return false;
    }

    counter = 0;
    last_update_ms = 0;
    clock_started = false;
    return App_OLED_Draw_Frame();
}

bool App_OLED_Update(uint32_t now_ms)
{
    if (display == NULL) {
        return false;
    }
    if (!clock_started) {
        last_update_ms = now_ms;
        clock_started = true;
        return true;
    }
    /* 无符号减法可以正确处理 HAL_GetTick() 回绕。 */
    if ((uint32_t)(now_ms - last_update_ms) < APP_OLED_PERIOD_MS) {
        return true;
    }
    last_update_ms = now_ms;
    counter = (counter + 1) % 1000000;
    return App_OLED_Draw_Frame();
}
