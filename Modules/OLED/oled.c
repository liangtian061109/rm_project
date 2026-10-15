#include "oled.h"
#include <string.h>
#include "oledfont.h" // 字库:asc2_0806/1206/1608/2412, Hzk1~4
static void OLED_Write_Command(OLED_Instance *oled, uint8_t command)
{
  uint8_t data[2] = {0x00, command}; // 0x00表示命令模式
  IIC_Transmit(oled->iic, data, 2);
}
static void OLED_Write_Data(OLED_Instance *oled, uint8_t data)
{
  uint8_t buffer[2] = {0x40, data}; // 0x40表示数据模式
  IIC_Transmit(oled->iic, buffer, 2);
}
/*注册oled实例*/
#define Max_OLED_Instance 1
static OLED_Instance OLED_Instances[Max_OLED_Instance];
static uint8_t OLED_Instance_count = 0;

OLED_Instance *OLED_Register(OLED_Init_Config_s *conf)
{
  if(conf == NULL || OLED_Instance_count >= Max_OLED_Instance)
  {
    return NULL;
  }
  // IIC 注册失败(配置无效/地址重复/实例池已满)时不占用 OLED 槽位
  IIC_Instance *iic = IIC_Register(&conf->iic_conf);
  if(iic == NULL)
  {
    return NULL;
  }
  OLED_Instance *OLED = &OLED_Instances[OLED_Instance_count];
  memset(OLED, 0, sizeof(OLED_Instance));
  OLED->iic = iic;
  OLED_Instance_count++;
  return OLED;
}
/* ---------- 清显存 ---------- */
void OLED_Clear(OLED_Instance *oled) {
  for (uint8_t i = 0; i < 8; i++) {
    for (uint16_t n = 0; n < 128; n++) {
      oled->gram[n][i] = 0; // ★实例字段,不是全局 OLED_GRAM
    }
  }
  OLED_Refresh(oled);
}

/* ---------- 初始化:全用 WriteCommand,逐条发例程的命令序列 ---------- */
void OLED_Init(OLED_Instance *oled)
{
  OLED_Write_Command(oled, 0xAE); // display off
  OLED_Write_Command(oled, 0x00); // low column addr
  OLED_Write_Command(oled, 0x10); // high column addr
  OLED_Write_Command(oled, 0x40); // start line
  OLED_Write_Command(oled, 0x81); // set contrast
  OLED_Write_Command(oled, 0xCF); //   contrast value (也是命令!)
  OLED_Write_Command(oled, 0xA1); // seg remap
  OLED_Write_Command(oled, 0xC8); // com scan dir
  OLED_Write_Command(oled, 0xA6); // normal display
  OLED_Write_Command(oled, 0xA8); // multiplex ratio
  OLED_Write_Command(oled, 0x3F); //   1/64 duty
  OLED_Write_Command(oled, 0xD3); // display offset
  OLED_Write_Command(oled, 0x00); //   no offset
  OLED_Write_Command(oled, 0xD5); // clock divide
  OLED_Write_Command(oled, 0x80); //   100 fps
  OLED_Write_Command(oled, 0xD9); // pre-charge
  OLED_Write_Command(oled, 0xF1); //   pre-charge value
  OLED_Write_Command(oled, 0xDA); // com pins
  OLED_Write_Command(oled, 0x12); //   com pins config
  OLED_Write_Command(oled, 0xDB); // vcomh
  OLED_Write_Command(oled, 0x40); //   vcomh level
  OLED_Write_Command(oled, 0x20); // addressing mode
  OLED_Write_Command(oled, 0x02); //   page mode
  OLED_Write_Command(oled, 0x8D); // charge pump
  OLED_Write_Command(oled, 0x14); //   enable
  OLED_Write_Command(oled, 0xA4); // display follows RAM
  OLED_Write_Command(oled, 0xA6); // normal (not inverted)

  OLED_Clear(oled);               // 清屏(内部会调 OLED_Refresh)
  OLED_Write_Command(oled, 0xAF); // display on
}


/* ---------- 刷新:把 gram 整片发给屏幕 ---------- */
void OLED_Refresh(OLED_Instance *oled) {
  uint8_t buf[129]; // buf[0]=0x40前缀 + 128字节数据
  buf[0] = 0x40;

  for (uint8_t i = 0; i < 8; i++) {    // 8 页
    OLED_Write_Command(oled, 0xB0 + i); // 选页
    OLED_Write_Command(oled, 0x00);     // 列低地址 = 0
    OLED_Write_Command(oled, 0x10);     // 列高地址 = 0

    for (uint16_t n = 0; n < 128; n++) { // 拷这一页的 128 字节
      buf[n + 1] = oled->gram[n][i];
    }
    IIC_Transmit(oled->iic, buf, 129); // ★一次发 129 字节,比逐字节快
  }
}
/* ---------- 画点:在显存里点亮/熄灭一个像素 ---------- */
// set=1 点亮, set=0 熄灭
static void oled_draw_point(OLED_Instance *oled, uint8_t x, uint8_t y,
                           uint8_t set) {
  if (x >= 128 || y >= 64)
    return;             // 越界保护
  uint8_t page = y / 8; // 第几页
  uint8_t bit = y % 8;  // 页内第几位
  if (set)
    oled->gram[x][page] |= (1 << bit); // 置1=亮
  else
    oled->gram[x][page] &= ~(1 << bit); // 清0=灭
}

/* ---------- 显示一个字符 ---------- */
// x,y:左上角坐标;chr:字符;size:8/12/16/24 选一
void OLED_Show_Char(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t chr,
                  uint8_t size) {
  uint8_t i, m, temp, size2, chr1;
  uint8_t x0 = x, y0 = y;

  // 一个字符占多少字节
  if (size == 8)
    size2 = 6;
  else
    size2 = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

  chr1 = chr - ' '; // 字库从空格(' ')开始,算偏移

  for (i = 0; i < size2; i++) {
    if (size == 8)
      temp = asc2_0806[chr1][i];
    else if (size == 12)
      temp = asc2_1206[chr1][i];
    else if (size == 16)
      temp = asc2_1608[chr1][i];
    else if (size == 24)
      temp = asc2_2412[chr1][i];
    else
      return;

    for (m = 0; m < 8; m++) {                  // 一个字节8位=8个像素
      oled_draw_point(oled, x, y, temp & 0x01); // bit=1亮,0灭
      temp >>= 1;
      y++;
    }
    x++;                                         // 下一列
    if ((size != 8) && ((x - x0) == size / 2)) { // 画满宽度,换到下半部分
      x = x0;
      y0 = y0 + 8;
    }
    y = y0;
  }
}

/* ---------- 显示字符串 ---------- */
void OLED_Show_String(OLED_Instance *oled, uint8_t x, uint8_t y, const char *s,
                    uint8_t size) {
  while (*s >= ' ' && *s <= '~') { // 可见 ASCII
    OLED_Show_Char(oled, x, y, (uint8_t)*s, size);
    if (size == 8)
      x += 6;
    else
      x += size / 2; // 字宽=size/2
    s++;
  }
}

/* ---------- 反色:0正常 1反色 ---------- */
void OLED_Color_Turn(OLED_Instance *oled, uint8_t i) {
  if (i == 0)
    OLED_Write_Command(oled, 0xA6); // 正常
  if (i == 1)
    OLED_Write_Command(oled, 0xA7); // 反色
}

/* ---------- 翻转180度:0正常 1翻转 ---------- */
void OLED_Display_Turn(OLED_Instance *oled, uint8_t i) {
  if (i == 0) {
    OLED_Write_Command(oled, 0xC8); // 正常方向
    OLED_Write_Command(oled, 0xA1);
  }
  if (i == 1) {
    OLED_Write_Command(oled, 0xC0); // 翻转180
    OLED_Write_Command(oled, 0xA0);
  }
}

/* ---------- 画线:Bresenham ---------- */
void OLED_Draw_Line(OLED_Instance *oled, uint8_t x1, uint8_t y1, uint8_t x2,
                  uint8_t y2) {
  int16_t xerr = 0, yerr = 0, delta_x, delta_y, distance;
  int16_t incx, incy, uRow, uCol;
  delta_x = x2 - x1;
  delta_y = y2 - y1;
  uRow = x1;
  uCol = y1;
  if (delta_x > 0)
    incx = 1;
  else if (delta_x == 0)
    incx = 0;
  else {
    incx = -1;
    delta_x = -delta_x;
  }
  if (delta_y > 0)
    incy = 1;
  else if (delta_y == 0)
    incy = 0;
  else {
    incy = -1;
    delta_y = -delta_y; // 例程原是 -delta_x,笔误,已修
  }
  if (delta_x > delta_y)
    distance = delta_x;
  else
    distance = delta_y;
  for (uint16_t t = 0; t < distance + 1; t++) {
    oled_draw_point(oled, uRow, uCol, 1);
    xerr += delta_x;
    yerr += delta_y;
    if (xerr > distance) {
      xerr -= distance;
      uRow += incx;
    }
    if (yerr > distance) {
      yerr -= distance;
      uCol += incy;
    }
  }
}

/* ---------- 画圆 ---------- */
void OLED_Draw_Circle(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t r) {
  int16_t a = 0, b = r, num;
  while (2 * b * b >= r * r) {
    oled_draw_point(oled, x + a, y - b, 1);
    oled_draw_point(oled, x - a, y - b, 1);
    oled_draw_point(oled, x - a, y + b, 1);
    oled_draw_point(oled, x + a, y + b, 1);
    oled_draw_point(oled, x + b, y + a, 1);
    oled_draw_point(oled, x + b, y - a, 1);
    oled_draw_point(oled, x - b, y - a, 1);
    oled_draw_point(oled, x - b, y + a, 1);
    a++;
    num = (a * a + b * b) - r * r;
    if (num > 0) {
      b--;
      a--;
    }
  }
}

/* ---------- m^n(给 ShowNum 用) ---------- */
static uint32_t oled_pow(uint8_t m, uint8_t n) {
  uint32_t result = 1;
  while (n--)
    result *= m;
  return result;
}

/* ---------- 显示数字(高位不足补0) ----------
   num:要显示的数;len:位数;size:字体 */
void OLED_Show_Num(OLED_Instance *oled, uint8_t x, uint8_t y, uint32_t num,
                 uint8_t len, uint8_t size) {
  uint8_t t, temp, m = 0;
  if (size == 8)
    m = 2;
  for (t = 0; t < len; t++) {
    temp = (num / oled_pow(10, len - t - 1)) % 10;
    OLED_Show_Char(oled, x + (size / 2 + m) * t, y, temp + '0', size);
  }
}

/* ---------- 显示中文 ----------
   num:oledfont.h 里 Hzk 的序号;size:16/24/32/64
   字库 Hzk1~4,要自己用取模软件加字。 */
void OLED_Show_Chinese(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t num,
                     uint8_t size) {
  uint8_t m, temp;
  uint8_t x0 = x, y0 = y;
  uint16_t i, size3 = (size / 8 + ((size % 8) ? 1 : 0)) * size;
  for (i = 0; i < size3; i++) {
    if (size == 16)
      temp = Hzk1[num][i];
    else if (size == 24)
      temp = Hzk2[num][i];
    else if (size == 32)
      temp = Hzk3[num][i];
    else if (size == 64)
      temp = Hzk4[num][i];
    else
      return;
    for (m = 0; m < 8; m++) {
      oled_draw_point(oled, x, y, temp & 0x01);
      temp >>= 1;
      y++;
    }
    x++;
    if ((x - x0) == size) {
      x = x0;
      y0 = y0 + 8;
    }
    y = y0;
  }
}

/* ---------- 显示图片 ----------
   sizex,sizey:尺寸;bmp:点阵数据 */
void OLED_Show_Picture(OLED_Instance *oled, uint8_t x, uint8_t y, uint8_t sizex,
                     uint8_t sizey, const uint8_t *bmp) {
  uint16_t j = 0;
  uint8_t i, n, temp, m;
  uint8_t x0 = x, y0 = y;
  sizey = sizey / 8 + ((sizey % 8) ? 1 : 0);
  for (n = 0; n < sizey; n++) {
    for (i = 0; i < sizex; i++) {
      temp = bmp[j++];
      for (m = 0; m < 8; m++) {
        oled_draw_point(oled, x, y, temp & 0x01);
        temp >>= 1;
        y++;
      }
      x++;
      if ((x - x0) == sizex) {
        x = x0;
        y0 = y0 + 8;
      }
      y = y0;
    }
  }
}

/* ---------- 滚动显示中文(从右往左滑入) ----------
   num:滚几个字;space:每字间隔
   注意:现 gram 宽 128,新字写到 x=128 会被越界保护忽略,只能滑已有内容。
   要完整滑入新字,把 gram 改成 [144][8] 并把下面两处 OLED_Width 换成 144。 */
void OLED_Scroll_Display(OLED_Instance *oled, uint8_t num, uint8_t space) {
  uint8_t i, n, t = 0, m = 0, r;
  while (1) {
    if (m == 0) {
      OLED_Show_Chinese(oled, OLED_Width, 24, t, 16); // 写到屏幕右外
      t++;
    }
    if (t == num) {
      for (r = 0; r < 16 * space; r++) {
        for (i = 1; i < OLED_Width; i++)
          for (n = 0; n < 8; n++)
            oled->gram[i - 1][n] = oled->gram[i][n];
        OLED_Refresh(oled);
      }
      t = 0;
    }
    m++;
    if (m == 16)
      m = 0;
    for (i = 1; i < OLED_Width; i++)
      for (n = 0; n < 8; n++)
        oled->gram[i - 1][n] = oled->gram[i][n];
    OLED_Refresh(oled);
  }
}