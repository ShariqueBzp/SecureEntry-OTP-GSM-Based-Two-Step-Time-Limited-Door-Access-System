/*****************************************************************************
 * @file      lcd.c
 * @brief     LCD Driver - 4-bit mode, 20x4 (LPC2148)
 * @details   Data lines: P0.10-P0.13 (D4-D7)
 *            RS: P0.17, RW: P0.19, EN: P0.18
 *            All command macros from all_peripheral_defines.h
 *****************************************************************************/

#include <LPC21xx.h>
#include "types.h"
#include "delay.h"
#include "all_peripheral_defines.h"
#include "lcd.h"

/*****************************************************************************
 * WriteLCD - sends one byte as two 4-bit nibbles (upper, then lower)
 * Each nibble: place on P0.10-P0.13, pulse EN high?low.
 * RW is held LOW (write).
 *****************************************************************************/
void WriteLCD(u8 byte)
{
    /* Select write operation */
    IOCLR0 = (1UL << LCD_RW);

    /* ---- Upper nibble (D7-D4) for ---- */
    IOPIN0 = (IOPIN0 & ~(0x0FUL << LCD_DATA))
           | (((u32)(byte >> 4) & 0x0F) << LCD_DATA);
    IOSET0 = (1UL << LCD_EN);
    delay_us(1);
    IOCLR0 = (1UL << LCD_EN);
    delay_us(50);

    /* ---- Lower nibble (D3-D0) ---- */
    IOPIN0 = (IOPIN0 & ~(0x0FUL << LCD_DATA))
           | (((u32)byte & 0x0F) << LCD_DATA);
    IOSET0 = (1UL << LCD_EN);
    delay_us(1);
    IOCLR0 = (1UL << LCD_EN);
    delay_us(50);
}

/*****************************************************************************
 * CmdLCD - command to instruction register (RS = 0)
 *****************************************************************************/
void CmdLCD(u8 cmd)
{
    IOCLR0 = (1UL << LCD_RS);
    WriteLCD(cmd);
}

/*****************************************************************************
 * InitLCD - 4-bit wakeup sequence for 20x4 HD44780-compatible controller
 *
 * IMPORTANT: because we are already in 4-bit mode (data pins D4-D7),
 * the function-set command 0x28 is sent as TWO nibbles:
 *   upper = 0x2 ? "switch to 4-bit",   lower = 0x8 ? "N=1, F=0".
 *****************************************************************************/
void InitLCD(void)
{
    /* Configure P0.10-P0.13 (data), P0.17 (RS), P0.18 (EN), P0.19 (RW) as outputs */
    u32 pin_mask = ((0x0FUL << LCD_DATA)
                  | (1UL << LCD_RS)
                  | (1UL << LCD_RW)
                  | (1UL << LCD_EN));
    IODIR0 |= pin_mask;

    /* Power-on settle (datasheet >15 ms; use 50 ms for clones) */
    delay_ms(50);

    /* 4-bit wakeup: send 0x03 as upper nibble 3 times, then 0x02 */
    CmdLCD(LCD_RESET);
	delay_ms(5);        /* 0x03 */
    CmdLCD(LCD_RESET);
	delay_us(150);      /* 0x03 */
    CmdLCD(LCD_RESET);
	delay_us(150);      /* 0x03 */
    CmdLCD(RET_CUR_HOME);
	delay_us(150);      /* 0x02 ? enter 4-bit */

    /* Function set: 4-bit, N=1 (4-line) - 0x28 */
    CmdLCD(MODE_4BIT_2LINE); 
		delay_us(100);

    /* Display ON, cursor OFF */
    CmdLCD(DSP_ON_CUR_OFF);
		delay_us(100);

    /* Clear display - takes ~1.52 ms */
    CmdLCD(CLEAR_LCD); 
		delay_ms(2);

    /* Entry mode: increment, no display shift */
    CmdLCD(SHIFT_CUR_RIGHT);  
		delay_us(100);
}

/*****************************************************************************
 * CharLCD - write data byte (RS = 1)
 *****************************************************************************/
void CharLCD(u8 asciiVal)
{
    IOSET0 = (1UL << LCD_RS);
    WriteLCD(asciiVal);
}

void StrLCD(s8 *Str)
{
    while (*Str) CharLCD((u8)*Str++);
}

void U32LCD(u32 num)
{
    char a[10];
    s32 i = 0;
    if (num == 0) { CharLCD('0'); return; }
    while (num > 0) { a[i++] = (char)((num % 10) + 48); num /= 10; }
    for (i--; i >= 0; i--) CharLCD((u8)a[i]);
}

void S32LCD(s32 num)
{
    if (num < 0) { CharLCD('-'); num = -num; }
    U32LCD((u32)num);
}

void F32LCD(f32 fnum, u32 nDP)
{
    u32 num;
    s32 i;
    if (fnum < 0.0f) { CharLCD('-'); fnum = -fnum; }
    num = (u32)fnum;
    U32LCD(num);
    CharLCD('.');
    for (i = 0; i < (s32)nDP; i++) {
        fnum = (fnum - (f32)num) * 10.0f;
        num = (u32)fnum;
        CharLCD((u8)num + 48);
    }
}

void BuildCGRAM(s8 *p, u32 nBytes)
{
    u32 i;
    CmdLCD(GOTO_CGRAM_START);
    IOSET0 = (1UL << LCD_RS);
    IOCLR0 = (1UL << LCD_RW);
    for (i = 0; i < nBytes; i++) WriteLCD((u8)p[i]);
    CmdLCD(GOTO_LINE1_POSO);
}