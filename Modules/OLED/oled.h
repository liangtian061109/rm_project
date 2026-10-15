#ifndef OLED_H
#define OLED_H

#include "bsp_iic.h"

#define OLED_Width 128
#define OLED_Height 64

typedef struct
{
  IIC_Instance *iic;
  uint8_t gram[OLED_Width][8];
} OLED_Instance;

typedef struct
{
  IIC_Init_Config_s iic_conf;
} OLED_Init_Config_s;

OLED_Instance *OLED_Register(OLED_Init_Config_s *conf);
void OLED_Show_Char(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t chr,
                  uint8_t size);
void OLED_Show_String(OLED_Instance *oled, uint8_t x, uint8_t y, const char *s,
                    uint8_t size);
void OLED_Show_Num(OLED_Instance *oled, uint8_t x, uint8_t y, uint32_t num,
                 uint8_t len, uint8_t size);
void OLED_Show_Chinese(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t num,
                     uint8_t size);
void OLED_Show_Picture(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t sizex,
                     uint8_t sizey, const uint8_t *bmp);
void OLED_Refresh(OLED_Instance *oled);
void OLED_Init(OLED_Instance *oled);
void OLED_Clear(OLED_Instance *oled);
void OLED_Draw_Line(OLED_Instance *oled, uint8_t x1, uint8_t y1, uint8_t x2,
                  uint8_t y2);
void OLED_Draw_Circle(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t r);
void OLED_Color_Turn(OLED_Instance *oled, uint8_t i);
void OLED_Display_Turn(OLED_Instance *oled, uint8_t i);
void OLED_Scroll_Display(OLED_Instance *oled, uint8_t num, uint8_t space);

#endif   